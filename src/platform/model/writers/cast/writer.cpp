#include "writer.hpp"

#include <utility>

namespace ISR::cast_detail {
uint32_t narrow(const size_t n, const std::string_view what) {
    if (n > UINT32_MAX)
        throw std::invalid_argument("CAST " + std::string(what) + " exceeds 32-bit limit");
    return static_cast<uint32_t>(n);
}

std::string lower_ascii(const std::string_view value) {
    std::string result(value);
    for (char &c : result)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return result;
}

std::string resource_stem(std::string_view source, const std::string_view fallback) {
    if (const auto slash = source.find_last_of("/\\"); slash != source.npos)
        source.remove_prefix(slash + 1);
    if (const auto dot = source.find_last_of('.'); dot != source.npos && dot != 0) {
        const auto extension = lower_ascii(source.substr(dot));
        if (extension == ".dds" || extension == ".png" || extension == ".cast")
            source.remove_suffix(source.size() - dot);
    }
    std::string stem;
    stem.reserve(std::min(source.size(), size_t{100}));
    for (const unsigned char c : source.substr(0, 100)) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
            c == '.')
            stem += static_cast<char>(c);
        else
            stem += '_';
    }
    while (!stem.empty() && (stem.front() == '.' || stem.back() == '.')) {
        if (stem.front() == '.')
            stem.erase(stem.begin());
        else
            stem.pop_back();
    }
    if (stem.empty())
        stem = fallback;
    const auto lower = lower_ascii(stem);
    if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul" ||
        (lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower.back() >= '1' &&
         lower.back() <= '9'))
        stem.insert(stem.begin(), '_');
    return stem;
}
void write_bytes(const std::filesystem::path &path, const DataBuffer &buffer) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("Failed to open CAST PNG: " + path.string());
    f.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!f)
        throw std::runtime_error("Failed to write CAST PNG: " + path.string());
}

} // namespace ISR::cast_detail
