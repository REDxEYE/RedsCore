#include "redscore/platform/gltf_helper.h"
#include "redscore/utils/simple_fileio.h"

void GltfHelper::save(const std::filesystem::path &path, const bool separate_binary) {
    if (has_shared_bin_folder()) {
        const std::filesystem::path binary_path = path.parent_path() / (m_shared_bin_path);
        std::filesystem::create_directories(binary_path);

        for (auto &image : m_model.images) {
            if (image.bufferView != -1) {
                auto &buffer_view = m_model.bufferViews[image.bufferView];
                auto &buffer = m_model.buffers[buffer_view.buffer];
                if (buffer_view.byteOffset == 0 && buffer.data.size() == buffer_view.byteLength) {
                    auto output_path = binary_path / (image.name + ".png");
                    write_file(output_path, buffer.data);
                    buffer.externalBufferSize = buffer.data.size();
                    buffer.data.clear();
                    image.bufferView = -1;
                    image.uri = output_path.string();
                }
            }
        }
    }

    if (separate_binary) {
        const std::filesystem::path binary_path = path.parent_path() / (path.stem().string() + "_bin");
        std::filesystem::create_directories(binary_path);

        for (auto &image : m_model.images) {
            if (image.bufferView != -1) {
                auto &buffer_view = m_model.bufferViews[image.bufferView];
                auto &buffer = m_model.buffers[buffer_view.buffer];
                if (buffer_view.byteOffset == 0 && buffer.data.size() == buffer_view.byteLength) {
                    auto output_path = binary_path / (image.name + ".png");
                    write_file(output_path, buffer.data);
                    buffer.externalBufferSize = buffer.data.size();
                    buffer.data.clear();
                    image.bufferView = -1;
                    image.uri = output_path.string();
                }
            }
        }

        u32 buffer_id = 0;
        for (auto &buffer : m_model.buffers) {
            if (!buffer.data.empty()) {
                auto output_path = binary_path / std::format("buffer_{}.bin", buffer_id);
                write_file(output_path, buffer.data);
                buffer.externalBufferSize = buffer.data.size();
                buffer.data.clear();
                buffer.uri = output_path.string();
            }
            buffer_id++;
        }
    }
    tinygltf::TinyGLTF writer;
    writer.WriteGltfSceneToFile(&m_model, path.string(), true, true, true, false);
}
void GltfHelper::set_shared_bin_folder(const std::string &path) { m_shared_bin_path = path; }
bool GltfHelper::has_shared_bin_folder() const { return !m_shared_bin_path.empty(); }
const std::string &GltfHelper::shared_bin_folder() { return m_shared_bin_path; }
