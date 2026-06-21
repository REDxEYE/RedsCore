// Created by RED on 17.09.2025.

#pragma once

#include <filesystem>
#include <fstream>

#include "redscore/platform/buffer/buffer.h"

#include "redscore/platform/file/file.h"

namespace IO {
    class NativeFile : public File {
    public:
        ~NativeFile() override {
            close();
        }

        NativeFile() = default;

        explicit NativeFile(const std::filesystem::path &path) {
            open_read(path);
        }

        explicit NativeFile(const std::filesystem::path &path, const std::ios::openmode mode) {
            open(path, mode);
        }

        [[nodiscard]] const std::ifstream &stream() const;

        void open_read(const std::filesystem::path &path);

        void open(const std::filesystem::path &path, std::ios::openmode mode);

        void set_position(std::streamoff position, std::ios::seekdir origin) override;

        std::streamsize get_position() override;

        size_t read(void *dst, std::streamsize size) override;

        size_t write(const void *src, std::streamsize size) override;

        size_t get_size() override;

        void close() override;

        size_t skip(uint32 size) override;

        [[nodiscard]] const std::filesystem::path &path() const;

        std::span<const uint8> cbuffer() override;

    private:
        std::filesystem::path m_path{};
        std::ifstream m_stream;
    };

    class WritableNativeFile : public File {
    public:
        ~WritableNativeFile() override {
            close();
        }

        WritableNativeFile() = default;

        explicit WritableNativeFile(const std::filesystem::path &path) {
            open_write(path);
        }

        explicit WritableNativeFile(const std::filesystem::path &path, const std::ios::openmode mode) {
            open(path, mode);
        }

        [[nodiscard]] const std::ofstream &stream() const;

        void open_write(const std::filesystem::path &path);

        void open(const std::filesystem::path &path, std::ios::openmode mode);

        void set_position(std::streamoff position, std::ios::seekdir origin) override;

        std::streamsize get_position() override;

        size_t read(void *dst, std::streamsize size) override;

        size_t write(const void *src, std::streamsize size) override;

        size_t get_size() override;

        void close() override;

        size_t skip(uint32 size) override;

        [[nodiscard]] const std::filesystem::path &path() const;

        std::span<const uint8> cbuffer() override;

    private:
        std::filesystem::path m_path{};
        std::ofstream m_stream;
    };

    // inline FilePtr open_file(std::string_view path) {
    //     return std::make_unique<NativeFile>(path);
    // }

    inline FilePtr open_file(const std::filesystem::path &path) {
        return std::make_unique<NativeFile>(path);
    }

    inline FilePtr open_file_write(const std::filesystem::path &path) {
        return std::make_unique<WritableNativeFile>(path);
    }

    inline Buffer read_file(const std::filesystem::path &path) {
        NativeFile file{path};
        Buffer buffer = Buffer::of_fixed_size(file.get_size());
        file.read_exact(buffer.as_span());
        return buffer;
    }

    inline Buffer read_file(const FilePtr &file) {
        Buffer buffer = Buffer::of_fixed_size(file->get_size());
        file->read_exact(buffer.as_span());
        return buffer;
    }
}
