//
// Created by red_eye on 10/14/26.
//

#include "redscore/platform/file/memmap_file.hpp"


inline void IO::MappedFile::set_position(const std::streamoff offset, const std::ios::seekdir origin) {
    size_t base;

    switch (origin) {
        case std::ios::beg:
            base = 0;
            break;
        case std::ios::cur:
            base = position_;
            break;
        case std::ios::end:
            base = size_;
            break;
        default:
            throw std::invalid_argument("Invalid seek origin");
    }

    size_t target;

    if (offset >= 0) {
        const auto delta = static_cast<uint64_t>(offset);
        if (delta > size_ - base)
            throw std::out_of_range("Seek exceeds file size");

        target = base + static_cast<size_t>(delta);
    } else {
        const auto delta = uint64_t(-(offset + 1)) + 1;
        if (delta > base)
            throw std::out_of_range("Seek before beginning of file");

        target = base - static_cast<size_t>(delta);
    }

    position_ = target;
}

std::streamsize IO::MappedFile::get_position() {
    if (position_ > static_cast<uint64_t>(
            std::numeric_limits<std::streamsize>::max()))
        throw std::overflow_error("Position exceeds streamsize");

    return static_cast<std::streamsize>(position_);
}

size_t IO::MappedFile::read(void *dst, const std::streamsize size) {
    if (size <= 0)
        return 0;

    const size_t count = std::min(
        static_cast<uint64_t>(size),
        size_ - position_
    );

    if (count != 0) {
        if (!dst)
            throw std::invalid_argument("Null read destination");

        std::memcpy(dst, data_ + position_, count);
        position_ += count;
    }

    return count;
}

size_t IO::MappedFile::skip(const uint32 size) {
    const size_t count = std::min(
        static_cast<size_t>(size),
        size_ - position_
    );

    position_ += count;
    return count;
}

void IO::MappedFile::close() {
#ifdef _WIN32
    if (data_)
        UnmapViewOfFile(data_);

    if (mapping_)
        CloseHandle(mapping_);

    if (file_ != INVALID_HANDLE_VALUE)
        CloseHandle(file_);

    mapping_ = nullptr;
    file_ = INVALID_HANDLE_VALUE;
#else
    if (data_)
        munmap(const_cast<uint8*>(data_), size_);

    if (fd_ >= 0)
        ::close(fd_);

    fd_ = -1;
#endif
    data_ = nullptr;
    size_ = 0;
    position_ = 0;
}

void IO::MappedFile::open(const std::filesystem::path &path) {
#ifdef _WIN32
    file_ = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (file_ == INVALID_HANDLE_VALUE)
        throw std::system_error(
            GetLastError(), std::system_category(), "CreateFileW"
        );

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(file_, &file_size)) {
        const auto error = GetLastError();
        close();
        throw std::system_error(
            error, std::system_category(), "GetFileSizeEx"
        );
    }

    if (static_cast<uint64_t>(file_size.QuadPart) >
        std::numeric_limits<size_t>::max()) {
        close();
        throw std::overflow_error("File exceeds addressable size");
    }

    size_ = static_cast<size_t>(file_size.QuadPart);

    if (size_ == 0)
        return;

    mapping_ = CreateFileMappingW(
        file_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (!mapping_) {
        const auto error = GetLastError();
        close();
        throw std::system_error(
            error, std::system_category(), "CreateFileMappingW"
        );
    }

    data_ = static_cast<const uint8*>(
        MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0)
    );

    if (!data_) {
        const auto error = GetLastError();
        close();
        throw std::system_error(
            error, std::system_category(), "MapViewOfFile"
        );
    }
#else
    fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);

    if (fd_ < 0)
        throw std::system_error(
            errno, std::generic_category(), "open"
        );

    struct stat st{};

    if (fstat(fd_, &st) != 0) {
        const int error = errno;
        close();
        throw std::system_error(
            error, std::generic_category(), "fstat"
        );
    }

    if (st.st_size < 0 ||
        static_cast<uint64_t>(st.st_size) >
        std::numeric_limits<size_t>::max()) {
        close();
        throw std::overflow_error("File exceeds addressable size");
    }

    size_ = static_cast<size_t>(st.st_size);

    if (size_ == 0)
        return;

    void* mapping = mmap(
        nullptr,
        size_,
        PROT_READ,
        MAP_PRIVATE,
        fd_,
        0
    );

    if (mapping == MAP_FAILED) {
        const int error = errno;
        close();
        throw std::system_error(
            error, std::generic_category(), "mmap"
        );
    }

    data_ = static_cast<const uint8*>(mapping);
#endif
}
