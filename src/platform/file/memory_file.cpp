// Created by RED on 18.09.2025.

#include "redscore/platform/file/memory_file.h"
#include <cstdlib>
#include <cstring>

#include "redscore/platform/logger.h"

void common_set_position(std::streamoff &position, const std::streamoff new_position, std::streamsize buffer_size,
                         const std::ios::seekdir origin) {
    switch (origin) {
        case std::ios::beg:
            if (position < 0) return;
            position = new_position;
            break;
        case std::ios::cur:
            if (position < 0 && (-position) > position) return;
            if (position + new_position > buffer_size) return;
            position += new_position;
            break;
        case std::ios::end:
            if (position < 0 && (-position) > buffer_size) return;
            position = buffer_size + new_position;
            break;
        default:
            throw std::runtime_error("Invalid seek dir");
    }
}

size_t common_read(const std::span<const u8> &buffer, std::streamoff &position, void *dst, const std::streamsize size) {
    if (size == 0) {
        return 0;
    }

    if (position + size > buffer.size()) {
        throw std::runtime_error("Attempt to read beyond end of buffer");
    }
    std::memcpy(dst, buffer.data() + position, size);
    position += size;
    return size;
}

size_t common_write(IO::Buffer *buffer, std::streamoff &position, const void *src, const std::streamsize size) {
    if (size == 0) {
        return 0;
    }
    if (position + size > buffer->size()) {
        if (!buffer->is_expandable()) {
        throw std::runtime_error("Not enough to remaining in buffer and buffer does not support resizing");

        }
        buffer->resize(position + size);
    }

    std::memcpy(buffer->data() + position, src, size);
    position += size;
    return size;
}


void IO::MemoryFile::set_position(const std::streamoff position, const std::ios::seekdir origin) {
    common_set_position(m_position, position, m_data.size(), origin);
}

std::streamsize IO::MemoryFile::get_position() {
    return m_position;
}

size_t IO::MemoryFile::read(void *dst, const std::streamsize size) {
    return common_read(m_data, m_position, dst, size);
}

size_t IO::MemoryFile::write(const void *src, const std::streamsize size) {
    return common_write(&m_data, m_position, src, size);
}

size_t IO::MemoryFile::get_size() {
    return m_data.size();
}

void IO::MemoryFile::close() {
    m_data = Buffer{};
    m_position = 0;
}

size_t IO::MemoryFile::skip(const uint32 size) {
    if (m_position + size > m_data.size()) {
        throw std::runtime_error("Attempt to skip beyond end of buffer");
    }
    m_position += size;
    return size;
}

IO::MemoryViewFile IO::MemoryFile::take_span(const size_t size, const size_t offset) {
    return take_view(size, offset);
}

IO::MemoryViewFile IO::MemoryFile::take_view(const size_t size, const size_t offset) {
    if (offset + size > m_data.size()) {
        throw std::runtime_error("Attempt to take view beyond end of buffer");
    }
    return MemoryViewFile(m_data.view(offset, size));
}

std::span<const uint8> IO::MemoryFile::cbuffer() {
    return {m_data.data(), m_data.size()};
}

void IO::MemoryFile::resize(const size_t new_size) {
    m_data.resize(new_size);
}

void IO::MemoryViewFile::set_position(const std::streamoff position, const std::ios::seekdir origin) {
    common_set_position(m_position, position, m_data.size(), origin);
}

std::streamsize IO::MemoryViewFile::get_position() {
    return m_position;
}

size_t IO::MemoryViewFile::read(void *dst, const std::streamsize size) {
    return common_read(m_data, m_position, dst, size);
}

size_t IO::MemoryViewFile::write(const void *src, std::streamsize size) {
    throw std::runtime_error("Cannot write to a read-only MemoryViewBuffer");
}

size_t IO::MemoryViewFile::get_size() {
    return m_data.size();
}

void IO::MemoryViewFile::close() {
    m_data = ConstByteBufferView{};
    m_position = 0;
}

size_t IO::MemoryViewFile::skip(const uint32 size) {
    if (m_position + size > m_data.size()) {
        throw std::runtime_error("Attempt to skip beyond end of buffer");
    }
    m_position += size;
    return size;
}

std::span<const uint8> IO::MemoryViewFile::cbuffer() {
    return {m_data.data(), m_data.size()};
}

void IO::ExternalMemoryBufferFile::set_position(const std::streamoff position, const std::ios::seekdir origin) {
    common_set_position(m_position, position, m_buffer.size(), origin);
}

std::streamsize IO::ExternalMemoryBufferFile::get_position() {
    return m_position;
}

size_t IO::ExternalMemoryBufferFile::read(void *dst, std::streamsize size) {
    return common_read(m_buffer.as_span(), m_position, dst, size);
}

size_t IO::ExternalMemoryBufferFile::write(const void *src, std::streamsize size) {
    if (m_position + size > m_buffer.size() && m_buffer.is_expandable()) {
        m_buffer.resize(m_position + size);
    }
    return common_write(&m_buffer, m_position, src, size);
}

size_t IO::ExternalMemoryBufferFile::get_size() {
    return m_buffer.size();
}

void IO::ExternalMemoryBufferFile::close() {
}

size_t IO::ExternalMemoryBufferFile::skip(const uint32 size) {
    if (m_position + size) {
        if (!m_buffer.is_expandable()) {
            throw std::runtime_error("Trying to skip past unresizable buffer size");
        }
        m_buffer.resize(m_position + size);
    }
    m_position+=size;
    return m_position;
}

std::span<const uint8> IO::ExternalMemoryBufferFile::cbuffer() {
    return m_buffer.as_span();
}
