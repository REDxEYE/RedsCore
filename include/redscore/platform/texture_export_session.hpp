#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "redscore/platform/model/model.hpp"

namespace ISR::detail {

// A save-local prefetch window. Only references whose loaders explicitly opt in
// are moved to worker threads; other callbacks run on the writer thread.
class TextureExportSession {
public:
  explicit TextureExportSession(const Scene &scene);
  ~TextureExportSession();

  TextureExportSession(const TextureExportSession &) = delete;
  TextureExportSession &operator=(const TextureExportSession &) = delete;

private:
  struct Job {
    enum class Status { New, Queued, Running, Ready, Consumed };
    explicit Job(TexturePtr ref) : texture(std::move(ref)) {}

    TexturePtr texture;
    std::function<DataBuffer()> loader;
    DataBuffer result;
    std::exception_ptr error;
    Status status = Status::New;
  };

  std::vector<Job> jobs;
  std::vector<std::thread> workers;
  std::vector<size_t> pending;
  std::mutex mutex;
  std::condition_variable changed;
  size_t head = 0, queued = 0, active = 0, next = 0;
  bool stopping = false;

  void collect(const Scene &scene);
  void schedule();
  void work();
  DataBuffer take(size_t index);
  void stop_and_restore();
};

} // namespace ISR::detail
