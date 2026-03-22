// Created by RED on 01.02.2026.

#pragma once
#include "redscore/platform/archive_manager.h"
#include "gltf_helper.h"
#include "redscore/utils/common.h"

class AppState {
public:
    AppState(const std::filesystem::path &game_root) : AppState() {
        m_game_root = game_root;
        convert_to_wsl(m_game_root);
    }

    AppState(){}

    ArchiveManager &manager();

    [[nodiscard]] const std::filesystem::path &export_path() const;

    void export_path(const std::filesystem::path &path);

    GltfHelper &helper() {
        return m_gltf_helper;
    }


protected:
    GltfHelper m_gltf_helper{};
    std::filesystem::path m_game_root;
    std::filesystem::path m_export_path;

};

