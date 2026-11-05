#include "../../include/redscore/platform/texture_export_session.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace ISR::detail {

TextureExportSession::TextureExportSession(const Scene &scene) {
  collect(scene);
  if (jobs.empty())
    return;

  const size_t count =
      std::min({jobs.size(), size_t{4},
                size_t{std::max(1u, std::thread::hardware_concurrency())}});
  // Each scheduled job holds a permit until the writer takes its PNG.
  pending.resize(std::min(jobs.size(), count * 2));
  workers.reserve(count);
  std::vector<std::function<DataBuffer()>> replacements;
  replacements.reserve(jobs.size());
  for (size_t i = 0; i < jobs.size(); ++i)
    replacements.emplace_back([this, i] { return take(i); });
  for (size_t i = 0; i < jobs.size(); ++i) {
    jobs[i].loader = std::move(jobs[i].texture->load_png);
    jobs[i].texture->load_png = std::move(replacements[i]);
  }
  try {
    for (size_t i = 0; i < count; ++i)
      workers.emplace_back([this] { work(); });
    std::lock_guard lock(mutex);
    schedule();
  } catch (...) {
    stop_and_restore();
    throw;
  }
}

TextureExportSession::~TextureExportSession() { stop_and_restore(); }

void TextureExportSession::collect(const Scene &scene) {
  std::unordered_set<const Node *> visited_nodes;
  std::unordered_set<const TextureReference *> visited_textures;
  std::vector<NodePtr> nodes;
  nodes.insert(nodes.end(), scene.roots.rbegin(), scene.roots.rend());
  auto add_texture = [&](const TexturePtr &ref) {
    if (ref && ref->parallel_safe && ref->load_png &&
        visited_textures.insert(ref.get()).second)
      jobs.emplace_back(ref);
  };
  while (!nodes.empty()) {
    auto node = std::move(nodes.back());
    nodes.pop_back();
    if (!node || !visited_nodes.insert(node.get()).second)
      continue;
    if (node->model)
      for (const auto &submodel : node->model->submodels)
        for (const auto &mesh : submodel.meshes)
          for (const auto &primitive : mesh.primitives)
            if (const auto &material = primitive.material) {
              add_texture(material->albedo);
              add_texture(material->metallic_roughness);
              add_texture(material->specular);
              add_texture(material->specularColor);
              add_texture(material->normal);
              add_texture(material->occlusion);
              add_texture(material->emissive);
            }
    nodes.insert(nodes.end(), node->children.rbegin(), node->children.rend());
    if (node->prefab)
      nodes.push_back(node->prefab);
  }
}

// Called with mutex held; a full window cannot block an out-of-order writer.
void TextureExportSession::schedule() {
  while (active < pending.size() && next < jobs.size()) {
    const size_t index = next++;
    if (jobs[index].status != Job::Status::New)
      continue;
    jobs[index].status = Job::Status::Queued;
    pending[(head + queued) % pending.size()] = index;
    ++queued;
    ++active;
  }
  changed.notify_all();
}

void TextureExportSession::work() {
  for (;;) {
    size_t index;
    {
      std::unique_lock lock(mutex);
      changed.wait(lock, [&] { return stopping || queued; });
      if (stopping)
        return;
      index = pending[head];
      head = (head + 1) % pending.size();
      --queued;
      jobs[index].status = Job::Status::Running;
    }
    DataBuffer png;
    std::exception_ptr error;
    try {
      png = jobs[index].loader();
    } catch (...) {
      error = std::current_exception();
    }
    {
      std::lock_guard lock(mutex);
      jobs[index].result = std::move(png);
      jobs[index].error = std::move(error);
      jobs[index].status = Job::Status::Ready;
    }
    changed.notify_all();
  }
}

DataBuffer TextureExportSession::take(size_t index) {
  std::unique_lock lock(mutex);
  Job &job = jobs[index];
  if (job.status == Job::Status::New || job.status == Job::Status::Consumed) {
    // Decode a request outside the prefetch window on the caller thread.
    job.status = Job::Status::Consumed;
    lock.unlock();
    return job.loader();
  }
  changed.wait(lock, [&] {
    return job.status == Job::Status::Ready ||
           job.status == Job::Status::Consumed;
  });
  if (job.status == Job::Status::Consumed) {
    lock.unlock();
    return job.loader();
  }
  DataBuffer png = std::move(job.result);
  auto error = std::move(job.error);
  job.status = Job::Status::Consumed;
  --active;
  schedule();
  lock.unlock();
  if (error)
    std::rethrow_exception(error);
  return png;
}

void TextureExportSession::stop_and_restore() {
  {
    std::lock_guard lock(mutex);
    stopping = true;
  }
  changed.notify_all();
  for (auto &worker : workers)
    worker.join();
  // No worker can still be reading a moved loader here.
  for (auto &job : jobs)
    if (job.loader)
      job.texture->load_png = std::move(job.loader);
}

} // namespace ISR::detail
