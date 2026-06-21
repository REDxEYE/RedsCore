// Created by RED on 17.09.2025.

#pragma once
#include <memory>
#include <span>
#include <vector>
#include <fstream>

#include "redscore/int_def.h"
#include "redscore/platform/buffer/buffer.h"

namespace IO {
    class File {
    public:
        virtual ~File() = default;

        virtual void set_position(std::streamoff position, std::ios::seekdir origin) = 0;

        virtual void set_position(const u64 position) {
            if (position > std::numeric_limits<std::streamoff>::max()) {
                throw std::invalid_argument("Position exceeds maximum stream offset");
            }
            set_position(static_cast<std::streamoff>(position), std::ios::beg);
        }

        virtual std::streamsize get_position() = 0;

        virtual size_t remaining();

        virtual void align(size_t alignment);

        virtual size_t read(void *dst, std::streamsize size) = 0;

        virtual size_t write(const void *src, std::streamsize size) = 0;

        virtual std::vector<uint8> read(size_t size);

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        std::vector<T> read_exact(size_t size) {
            std::vector<T> data(size);
            read_exact(data);
            return std::move(data);
        }

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void read_exact(std::vector<T> &data) {
            const size_t bytesRead = read(data.data(), data.size() * sizeof(T));
            if (bytesRead < data.size() * sizeof(T)) {
                throw std::runtime_error("Failed to read enough bytes for type array");
            }
        }

        template<typename T>
            requires std::is_trivially_copyable_v<T> && (!std::is_const_v<T>)
        void read_exact(std::span<T> data) {
            const size_t bytesRead = read(data.data(), data.size() * sizeof(T));
            if (bytesRead < data.size() * sizeof(T)) {
                throw std::runtime_error("Failed to read enough bytes for type array");
            }
        }

        virtual size_t get_size() = 0;

        virtual void close() = 0;

        virtual size_t skip(uint32 size) = 0;

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        T read_pod() {
            T value;
            if (const size_t bytesRead = read(&value, sizeof(T)); bytesRead < sizeof(T)) {
                throw std::runtime_error("Failed to read enough bytes for type");
            }
            return value;
        }

        f32 read_f32() {
            return read_pod<f32>();
        }

        u64 read_u64() {
            return read_pod<u64>();
        }

        u32 read_u32() {
            return read_pod<u32>();
        }

        u16 read_u16() {
            return read_pod<u16>();
        }

        u8 read_u8() {
            return read_pod<u8>();
        }

        Buffer read_bytes(const u64 size) {
            Buffer buffer = Buffer::of_fixed_size(size);
            read_exact(buffer.as_span());
            return buffer;
        }

        void read_cstring(std::string &str);

        std::string read_cstring();

        void read_string(uint32 size, std::string &str);

        std::string read_cstring_at(i64 offset, u32 size = -1);

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void write(T data) {
            const size_t bytesWritten = write(&data, sizeof(T));
            if (bytesWritten < sizeof(T)) {
                throw std::runtime_error("Failed to write enough bytes for type");
            }
        }

        virtual std::span<const uint8> cbuffer() = 0;

        // virtual std::span<uint8> buffer() = 0;
    };

    using FilePtr = std::unique_ptr<File>;
}
