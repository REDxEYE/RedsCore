// Created by RED on 02.10.2025.

#pragma once
#include "container.h"
#include "redscore/platform/file/memory_file.h"
#include <functional>


template<typename KeyType>
class Archive : public Container<KeyType> {
public:
    struct ArchiveEntry {
        KeyType key;
        uint64 size;
    };

    ~Archive() override = default;

    // virtual void all_entries(std::vector<ArchiveEntry> &entries) const = 0;

    [[nodiscard]] virtual std::string_view name() const = 0;

    [[nodiscard]] virtual const KeyType& key() const = 0;

    // virtual uint64 hash() = 0;

    virtual bool foreach_file(const std::function<bool (const ArchiveEntry &)> &callback) = 0;
};
