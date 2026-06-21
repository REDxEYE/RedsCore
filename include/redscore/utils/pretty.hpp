//
// Created by red_eye on 5/12/26.
//

#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pretty {
    using u32 = std::uint32_t;

    struct NoContext {};

    template<typename T>
    struct is_std_array : std::false_type {};

    template<typename T, std::size_t N>
    struct is_std_array<std::array<T, N>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_std_array_v = is_std_array<std::remove_cvref_t<T>>::value;

    template<typename T>
    struct is_vector : std::false_type {};

    template<typename T, typename Alloc>
    struct is_vector<std::vector<T, Alloc>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_vector_v = is_vector<std::remove_cvref_t<T>>::value;

    template<typename T, typename Ctx>
    concept PrettyPrintableWithContext = requires(const T &value, std::ostream &os, u32 indent, const Ctx &ctx) {
        value.pretty_print(os, indent, ctx);
    };

    template<typename T>
    concept PrettyPrintable = requires(const T &value, std::ostream &os, u32 indent) {
        value.pretty_print(os, indent);
    };

    template<typename T>
    concept StreamPrintable = requires(const T &value, std::ostream &os) {
        os << value;
    };

    inline void print_indent(std::ostream &os, const u32 indent) {
        for (u32 i = 0; i < indent; ++i) {
            os << ' ';
        }
    }

    inline void print_struct_begin(std::ostream &os) {
        os << "{\n";
    }

    inline void print_struct_end(std::ostream &os, const u32 indent) {
        print_indent(os, indent);
        os << "}";
    }

    inline void print_field_name(std::ostream &os, const std::string_view name, const u32 indent) {
        print_indent(os, indent);
        os << name << ": ";
    }

    template<typename T, typename Ctx = NoContext>
    void print_value(std::ostream &os, const T &value, u32 indent, const Ctx &ctx = {}) {
        using U = std::remove_cvref_t<T>;

        if constexpr (PrettyPrintableWithContext<U, Ctx>) {
            value.pretty_print(os, indent, ctx);
        } else if constexpr (PrettyPrintable<U>) {
            value.pretty_print(os, indent);
        } else if constexpr (std::is_enum_v<U>) {
            os << static_cast<std::underlying_type_t<U>>(value);
        } else if constexpr (StreamPrintable<U>) {
            os << value;
        } else {
            os << "<unprintable>";
        }
    }

    template<typename T, typename Ctx = NoContext>
    void print_array(std::ostream &os, const T &array, const u32 indent, const Ctx &ctx = {}) {
        os << "[\n";

        for (const auto &value: array) {
            print_indent(os, indent);
            print_value(os, value, indent, ctx);
            os << "\n";
        }

        print_indent(os, indent - 2);
        os << "]";
    }

    template<typename T, typename Ctx = NoContext>
    void print_field(
        std::ostream &os,
        const std::string_view name,
        const T &value,
        const u32 indent,
        const Ctx &ctx = {}
    ) {
        using U = std::remove_cvref_t<T>;

        print_field_name(os, name, indent);

        if constexpr (std::is_array_v<U> || is_std_array_v<U> || is_vector_v<U>) {
            print_array(os, value, indent + 2, ctx);
        } else {
            print_value(os, value, indent, ctx);
        }

        os << "\n";
    }
}

#define PRETTY_FIELD(name) \
    ::pretty::print_field(os, #name, name, indent + 2)

#define PRETTY_FIELD_CTX(name) \
    ::pretty::print_field(os, #name, name, indent + 2, ctx)

#define PRETTY_VALUE(name, value) \
    ::pretty::print_field(os, name, value, indent + 2)

#define PRETTY_VALUE_CTX(name, value) \
    ::pretty::print_field(os, name, value, indent + 2, ctx)

#define PRETTY_PRINT(...)                                             \
    void pretty_print(std::ostream &os, const ::pretty::u32 indent = 0) const { \
        ::pretty::print_struct_begin(os);                             \
        __VA_ARGS__                                                   \
        ::pretty::print_struct_end(os, indent);                       \
    }

#define PRETTY_PRINT_CTX(CtxType, ...)                                \
    void pretty_print(std::ostream &os, const ::pretty::u32 indent, const CtxType &ctx) const { \
        ::pretty::print_struct_begin(os);                             \
        __VA_ARGS__                                                   \
        ::pretty::print_struct_end(os, indent);                       \
    }