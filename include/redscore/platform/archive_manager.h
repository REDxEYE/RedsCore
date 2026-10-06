// Created by RED on 02.10.2025.

#pragma once

#include <algorithm>
#include <vector>
#include <utility>
#include <memory>
#include <deque>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "logger.h"
#include "redscore/platform/archive.h"
#include "tracy/Tracy.hpp"

template<typename KeyType>
class ArchiveManager : public Archive<KeyType> {
protected:
    virtual std::pair<bool, KeyType> load_child_archive(const KeyType &hash) =0;

public:
    explicit ArchiveManager() {
    }

    [[nodiscard]] bool is_mounted(const KeyType &key) const {
        return m_archives.contains(key);
    }

    void mount(std::unique_ptr<Archive<KeyType> > archive) {
        m_archives.emplace(archive->key(), std::move(archive));
    }

    void unmount(const KeyType &key) {
        forget_dynamic_mount(key);
        m_archives.erase(key);
    }

    [[nodiscard]] bool has(const KeyType &key) override {
        ZoneScoped;
        for (const auto &archive: m_archives | std::views::values) {
            if (archive->has(key)) return true;
        }
        return false;
    }

    const KeyType& get_parent_key_for(const KeyType &key) {
        for (const auto &archive: m_archives | std::views::values) {
            if (archive->has(key)) {
                return archive->get_parent_key();
            }
        }
        static constexpr u64 z = 0;
        return z;
    }

    const KeyType& get_parent_key() override {
        static constexpr u64 z = 0;
        return z;

    }

    std::unique_ptr<IO::File> get(const KeyType &key) override {
        ZoneScoped
        for (const auto &archive: m_archives | std::views::values) {
            if (auto file = archive->get(key)) {
                return std::move(file);
            }
        }
        GLog_Error("File with hash {} not found in any archive", key);
        return nullptr;
    }

    [[nodiscard]] std::string_view name() const override {
        return "Root";
    }

    [[nodiscard]] const KeyType &key() const override {
        static KeyType value{};
        return value;
    }

    bool foreach_file(const std::function<bool(const typename Archive<KeyType>::ArchiveEntry &)> &callback) override {
        // Callbacks may mount/unmount archives. Keep a stable snapshot and pin its
        // archives until enumeration finishes, including nested enumerations.
        std::vector<std::shared_ptr<Archive<KeyType> > > archives;
        archives.reserve(m_archives.size());
        for (const auto &archive: m_archives | std::views::values) {
            archives.push_back(archive);
        }

        bool completed = true;
        // try {
        for (const auto &archive: archives) {
            if (m_dynamic_mount_set.contains(archive->key())) {
                touch_dynamic_mount(archive->key());
            }
            if (!archive->foreach_file(callback)) {
                completed = false;
                break;
            }
        }
        // } catch (...) {
        //     archives.clear();
        //     evict_dynamic_mounts();
        //     throw;
        // }
        archives.clear();
        evict_dynamic_mounts();
        return completed;
    }

    ArchiveManager(const ArchiveManager &) = delete;

    ArchiveManager &operator=(const ArchiveManager &) = delete;

    ArchiveManager(ArchiveManager &&) noexcept = default;

    ArchiveManager &operator=(ArchiveManager &&) noexcept = default;

protected:
    std::unordered_map<KeyType, std::shared_ptr<Archive<KeyType> > > m_archives;

    static constexpr size_t MAX_DYNAMIC_MOUNTS = 32;

    void touch_dynamic_mount(KeyType key) {
        if (!m_dynamic_mount_set.insert(key).second) {
            const auto &it = std::ranges::find(m_dynamic_mount_order, key);
            if (it != m_dynamic_mount_order.end()) {
                m_dynamic_mount_order.erase(it);
            }
        }
        m_dynamic_mount_order.push_back(key);
    }

    void evict_dynamic_mounts() {
        // Limit unused mounts. Active traversals may pin additional archives;
        // counting those would evict a newly loaded child before get() can read it.
        auto unused = std::ranges::count_if(m_dynamic_mount_order, [this](const auto &key) {
            const auto archive = m_archives.find(key);
            return archive == m_archives.end() || archive->second.use_count() == 1;
        });
        auto it = m_dynamic_mount_order.begin();
        while (unused > MAX_DYNAMIC_MOUNTS && it != m_dynamic_mount_order.end()) {
            const auto archive = m_archives.find(*it);
            if (archive != m_archives.end() && archive->second.use_count() > 1) {
                ++it;
                continue;
            }
            m_dynamic_mount_set.erase(*it);
            if (archive != m_archives.end()) m_archives.erase(archive);
            it = m_dynamic_mount_order.erase(it);
            --unused;
        }
    }

    void forget_dynamic_mount(KeyType key) {
        if (!m_dynamic_mount_set.erase(key)) {
            return;
        }

        const auto it = std::ranges::find(m_dynamic_mount_order, key);
        if (it != m_dynamic_mount_order.end()) {
            m_dynamic_mount_order.erase(it);
        }
    }

    std::deque<KeyType> m_dynamic_mount_order;
    std::unordered_set<KeyType> m_dynamic_mount_set;
};
