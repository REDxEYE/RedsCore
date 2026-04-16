// Created by RED on 17.09.2025.

#include "redscore/platform/file/native_file.h"

const std::ifstream & IO::NativeFile::stream() const {
    return m_stream;
}

void IO::NativeFile::open_read(const std::filesystem::path &path) {
    open(path, std::ios::in | std::ios::binary);
}

void IO::NativeFile::open(const std::filesystem::path &path, const std::ios::openmode mode) {
    close();
    m_stream.clear();
    m_path = path;
    m_stream.open(m_path, mode);
}

void IO::NativeFile::set_position(const std::streamoff position, const std::ios::seekdir origin) {
    m_stream.seekg(position, origin);
}

std::streamsize IO::NativeFile::get_position() {
    return m_stream.tellg();
}

size_t IO::NativeFile::read(void *dst, const std::streamsize size) {
    m_stream.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
    return m_stream.gcount();
}

size_t IO::NativeFile::write(const void *, std::streamsize) {
    throw std::runtime_error("NativeFile is read-only");
}

size_t IO::NativeFile::get_size() {
    const auto current_pos = m_stream.tellg();
    m_stream.seekg(0, std::ios::end);
    const auto size = m_stream.tellg();
    m_stream.seekg(current_pos, std::ios::beg);
    return static_cast<size_t>(size);
}

void IO::NativeFile::close() {
    if (m_stream.is_open()) {
        m_stream.close();
    }
}

size_t IO::NativeFile::skip(uint32 size) {
    m_stream.seekg(size, std::ios::cur);
    return size;
}

const std::filesystem::path & IO::NativeFile::path() const {
    return m_path;
}

std::span<const uint8> IO::NativeFile::cbuffer() {
    throw std::runtime_error("NativeFile does not support direct buffer access");
}

const std::ofstream & IO::WritableNativeFile::stream() const {
    return m_stream;
}

void IO::WritableNativeFile::open_write(const std::filesystem::path &path) {
    open(path, std::ios::out | std::ios::binary);
}

void IO::WritableNativeFile::open(const std::filesystem::path &path, const std::ios::openmode mode) {
    close();
    m_stream.clear();
    m_path = path;
    m_stream.open(m_path, mode);
}

void IO::WritableNativeFile::set_position(const std::streamoff position, const std::ios::seekdir origin) {
    m_stream.seekp(position, origin);
}

std::streamsize IO::WritableNativeFile::get_position() {
    return m_stream.tellp();
}

size_t IO::WritableNativeFile::read(void *, std::streamsize) {
    throw std::runtime_error("WritableNativeFile is write-only");
}

size_t IO::WritableNativeFile::write(const void *src, std::streamsize size) {
    m_stream.write(static_cast<const char *>(src), static_cast<std::streamsize>(size));
    return static_cast<size_t>(size);
}

size_t IO::WritableNativeFile::get_size() {
    const auto current_pos = m_stream.tellp();
    m_stream.seekp(0, std::ios::end);
    const auto size = m_stream.tellp();
    m_stream.seekp(current_pos, std::ios::beg);
    return static_cast<size_t>(size);
}

void IO::WritableNativeFile::close() {
    if (m_stream.is_open()) {
        m_stream.close();
    }
}

size_t IO::WritableNativeFile::skip(uint32 size) {
    m_stream.seekp(size, std::ios::cur);
    return size;
}

const std::filesystem::path & IO::WritableNativeFile::path() const {
    return m_path;
}

std::span<const uint8> IO::WritableNativeFile::cbuffer() {
    throw std::runtime_error("WritableNativeFile does not support direct buffer access");
}


