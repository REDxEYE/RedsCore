// Created by RED on 02.10.2025.

#pragma once
#include "redscore/platform/file/memory_file.h"
#include <format>


template<typename KeyType>
requires std::formattable<KeyType, char>
class Container {
public:
    virtual ~Container() = default;

    [[nodiscard]] virtual bool has(const KeyType &key) = 0;

    virtual std::unique_ptr<IO::File> get(const KeyType &key) = 0;
};
