// Created by RED on 02.10.2025.

#include "redscore/platform/archive_manager.h"

#include "redscore/platform/logger.h"
#include "tracy/Tracy.hpp"

#include <ranges>
#include <string_view>
#include <algorithm>

static constexpr auto hash_string = std::hash<std::string_view>{};

// void ArchiveManager::all_entries(std::vector<ArchiveEntry> &entries) const {
//     ZoneScoped;
//     for (const auto &archive: m_archives | std::views::values) {
//         archive->all_entries(entries);
//     }
// }

