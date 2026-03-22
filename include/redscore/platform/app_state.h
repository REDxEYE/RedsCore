// Created by RED on 01.02.2026.

#ifndef APEXPREDATOR_APP_STATE_H
#define APEXPREDATOR_APP_STATE_H
#include "redscore/platform/archive_manager.h"
#include "gltf_helper.h"
#include "redscore/utils/common.h"

class AppState {
public:
    AppState(const std::filesystem::__cxx11::path &game_root) : AppState() {
        m_game_root = game_root;
        convert_to_wsl(m_game_root);
    }

    AppState() : m_archive_manager(nullptr) {
    }

    ArchiveManager &manager();

    [[nodiscard]] const std::filesystem::path &export_path() const;

    void export_path(const std::filesystem::path &path);

    GltfHelper &helper() {
        return m_gltf_helper;
    }

    bool skip_textures{false};

private:
    GltfHelper m_gltf_helper{};
    ArchiveManager m_archive_manager;
    std::filesystem::path m_game_root;
    std::filesystem::path m_export_path;

};

#endif //APEXPREDATOR_APP_STATE_H
