// Created by RED on 02.10.2025.

#pragma once

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

    // void all_entries(std::vector<ArchiveEntry> &entries) const override;

    [[nodiscard]] std::string_view name() const override {
        return "Root";
    }

    [[nodiscard]] const KeyType& key() const override {
        static KeyType value{};
        return value;
    }

    void foreach_file(std::function<void(const KeyType&, const std::string&)> callback) const {
        for (const auto &archive: m_archives | std::views::values) {
            archive->foreach_file(callback);
        }
    }

    ArchiveManager(const ArchiveManager &) = delete;

    ArchiveManager &operator=(const ArchiveManager &) = delete;

    ArchiveManager(ArchiveManager &&) noexcept = default;

    ArchiveManager &operator=(ArchiveManager &&) noexcept = default;

protected:
    std::unordered_map<KeyType, std::unique_ptr<Archive<KeyType> > > m_archives;

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
        while (m_dynamic_mount_order.size() > MAX_DYNAMIC_MOUNTS) {
            const auto oldest_hash = m_dynamic_mount_order.front();
            m_dynamic_mount_order.pop_front();
            m_dynamic_mount_set.erase(oldest_hash);

            for (auto it = m_archives.begin(); it != m_archives.end();) {
                if (it->second->key() == oldest_hash) {
                    GLog_Info("Evicting {}", it->second->name());
                    it = m_archives.erase(it);
                } else {
                    ++it;
                }
            }
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
