#pragma once

#include "redscore/platform/model/model.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace ISR::cast_detail {
constexpr uint32_t root_id = 0x746f6f72, model_id = 0x6c646f6d, mesh_id = 0x6873656d;
constexpr uint32_t skeleton_id = 0x6c656b73, bone_id = 0x656e6f62, shape_id = 0x68736c62;
constexpr uint32_t material_id = 0x6c74616d, file_id = 0x656c6966, color_id = 0x726c6f63;
constexpr uint32_t instance_id = 0x74736e69, metadata_id = 0x6174656d;
constexpr uint32_t animation_id = 0x6d696e61, curve_id = 0x76727563;
constexpr uint64_t hash_base = 0x534e495752545250ULL;

uint32_t narrow(size_t n, std::string_view what);
std::string lower_ascii(std::string_view value);
std::string resource_stem(std::string_view source, std::string_view fallback);

class ResourceNames {
    std::string folder;
    std::unordered_set<std::string> used, directories;

    std::string path_key(const std::string_view filename) const {
        std::string key = folder;
        key += '/';
        key += filename;
        for (char &c : key)
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        return key;
    }

    bool claim(const std::string &filename) {
        auto key = path_key(filename);
        return !directories.contains(key) && used.insert(std::move(key)).second;
    }

  public:
    ResourceNames(const Scene &scene, const std::filesystem::path &path, const std::filesystem::path &dir)
        : folder(dir.filename().generic_string()) {
        used.insert(lower_ascii(path.filename().generic_string()));
        directories.insert(lower_ascii(folder));
        for (const auto &extra : scene.extra_files) {
            const std::filesystem::path relative(extra.name);
            if (relative.empty() || relative.is_absolute() ||
                std::ranges::any_of(relative, [](const auto &part) { return part == ".."; }))
                throw std::invalid_argument("CAST extra file must have a relative safe path");
            const auto normalized = relative.lexically_normal();
            const auto key = lower_ascii(normalized.generic_string());
            if (normalized.filename().empty() || directories.contains(key) || !used.insert(key).second)
                throw std::invalid_argument("CAST extra file conflicts with another output path");
            for (auto parent = normalized.parent_path(); !parent.empty(); parent = parent.parent_path()) {
                auto directory = lower_ascii(parent.generic_string());
                if (used.contains(directory))
                    throw std::invalid_argument("CAST extra file conflicts with another output path");
                directories.insert(std::move(directory));
            }
        }
    }

    std::string allocate(const std::string_view source, const std::string_view fallback,
                         const std::string_view extension, const size_t index) {
        const auto stem = resource_stem(source, fallback);
        std::string filename = stem + std::string(extension);
        if (claim(filename))
            return filename;
        const auto numbered = stem + "_" + std::to_string(index);
        for (size_t attempt = 0;; ++attempt) {
            filename = numbered;
            if (attempt)
                filename += "_" + std::to_string(attempt);
            filename += extension;
            if (claim(filename))
                return filename;
        }
    }
};

class Writer {
    std::ofstream stream;
    uint64_t next_hash = hash_base;

    struct Frame {
        std::streampos start;
        uint32_t properties = 0, children = 0;
        bool in_children = false;
    };

    std::vector<Frame> frames;

    void raw(const void *data, const size_t size) {
        stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        if (!stream)
            throw std::runtime_error("Failed to write CAST file");
    }

  public:
    explicit Writer(const std::filesystem::path &path) : stream(path, std::ios::binary | std::ios::trunc) {
        if (!stream)
            throw std::runtime_error("Failed to open CAST file: " + path.string());
        u32(0x74736163);
        u32(1);
        u32(1);
        u32(0);
    }

    void u8(const uint8_t v) { raw(&v, sizeof(v)); }
    void u16(const uint16_t v) { raw(&v, sizeof(v)); }
    void u32(const uint32_t v) { raw(&v, sizeof(v)); }
    void u64(const uint64_t v) { raw(&v, sizeof(v)); }

    void f32(const float v) {
        if (!std::isfinite(v))
            throw std::invalid_argument("Non-finite CAST float");
        raw(&v, sizeof(v));
    }

    void bytes(const void *data, const size_t size) { raw(data, size); }
    uint64_t next() const { return next_hash; }

    uint64_t begin(const uint32_t id) {
        if (!frames.empty()) {
            auto &parent = frames.back();
            parent.in_children = true;
            ++parent.children;
        }
        auto pos = stream.tellp();
        u32(id);
        u32(0);
        const uint64_t hash = next_hash++;
        u64(hash);
        u32(0);
        u32(0);
        frames.push_back({pos});
        return hash;
    }

    void end() {
        auto frame = frames.back();
        frames.pop_back();
        const auto end_pos = stream.tellp();
        const auto length = end_pos - frame.start;
        if (length < 24 || length > UINT32_MAX)
            throw std::invalid_argument("CAST node exceeds 32-bit size limit");
        stream.seekp(frame.start + std::streamoff(4));
        u32(static_cast<uint32_t>(length));
        stream.seekp(frame.start + std::streamoff(16));
        u32(frame.properties);
        u32(frame.children);
        stream.seekp(end_pos);
        if (!stream)
            throw std::runtime_error("Failed to finalize CAST node");
    }

    template <class F>
    void property(const std::string_view name, const std::string_view tag, const size_t count, F values) {
        if (frames.empty() || frames.back().in_children)
            throw std::logic_error("CAST properties must precede children");
        if (name.size() > UINT16_MAX || tag.empty() || tag.size() > 2 || name.find('\0') != name.npos)
            throw std::invalid_argument("Invalid CAST property name/type");
        ++frames.back().properties;
        u8(static_cast<uint8_t>(tag[0]));
        u8(tag.size() == 2 ? static_cast<uint8_t>(tag[1]) : 0);
        u16(static_cast<uint16_t>(name.size()));
        u32(narrow(count, "property array"));
        raw(name.data(), name.size());
        values();
    }

    void text(const std::string_view name, const std::string_view value) {
        if (value.find('\0') != value.npos)
            throw std::invalid_argument("NUL in CAST string");
        property(name, "s", 1, [&] {
            raw(value.data(), value.size());
            u8(0);
        });
    }

    void integer(const std::string_view name, const uint32_t v) {
        property(name, "i", 1, [&] { u32(v); });
    }
    void byte(const std::string_view name, const uint8_t v) {
        property(name, "b", 1, [&] { u8(v); });
    }
    void link(const std::string_view name, const uint64_t v) {
        property(name, "l", 1, [&] { u64(v); });
    }
    void scalar(const std::string_view name, const float v) {
        property(name, "f", 1, [&] { f32(v); });
    }

    void vec3(const std::string_view name, const glm::vec3 &v) {
        property(name, "3v", 1, [&] {
            f32(v.x);
            f32(v.y);
            f32(v.z);
        });
    }

    void vec4(const std::string_view name, const glm::vec4 &v) {
        property(name, "4v", 1, [&] {
            for (int i = 0; i < 4; ++i)
                f32(v[i]);
        });
    }
};

void write_bytes(const std::filesystem::path &path, const DataBuffer &buffer);

} // namespace ISR::cast_detail
