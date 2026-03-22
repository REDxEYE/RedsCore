// Created by RED on 18.02.2026.

#pragma once
#include "redscore/int_def.h"

#include <filesystem>
#include <span>

void write_file(const std::filesystem::path& path, const std::span<const uint8> &data);

