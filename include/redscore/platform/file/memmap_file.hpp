//
// Created by red_eye on 10/14/26.
//

#pragma once


#pragma once

#include "file.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace IO {

class MappedFile final : public File {
public:
    explicit MappedFile(const std::filesystem::path& path) {
        open(path);
    }

    ~MappedFile() override {
        close();
    }

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&&) = delete;
    MappedFile& operator=(MappedFile&&) = delete;

    using File::set_position;
    using File::read;
    using File::write;

    void set_position(std::streamoff offset, std::ios::seekdir origin) override;

    std::streamsize get_position() override;

    size_t read(void* dst, std::streamsize size) override;

    size_t write(const void*, std::streamsize) override {
        throw std::runtime_error("MappedFile is read-only");
    }

    size_t get_size() override {
        return size_;
    }

    size_t skip(uint32 size) override;

    std::span<const uint8> cbuffer() override {
        return {data_, size_};
    }

    void close() override;

private:
    void open(const std::filesystem::path& path);

    const uint8* data_ = nullptr;
    size_t size_ = 0;
    size_t position_ = 0;

#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};

}
