// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "common/logging/log.h"
#include "common/path_util.h"
#include "gr2_mods.h"

namespace Gr2Mods {

namespace {

constexpr const char* Section = "GR2Fork";
constexpr const char* Fsr3EnableKey = "fsr3Enable";

// The same serials as the emulator's Common::IsGravityRush2 and Common::IsGravityRushRemastered.
constexpr std::array<std::string_view, 7> Gr2Serials = {
    "CUSA03694", "CUSA04943", "CUSA04934", "CUSA00547", "PCJS50010", "PCAS00079", "CUSA04935",
};
constexpr std::array<std::string_view, 12> GrrSerials = {
    "CUSA01112", "CUSA01113", "CUSA01113P", "CUSA01130", "CUSA02318", "CUSA00546",
    "CUSA02443", "CUSA04246", "PCJS50004",  "PCJS50008", "PCJS66015", "PCJS66029",
};

std::filesystem::path ConfigFile(const std::string& serial) {
    if (serial.empty()) {
        return Common::FS::GetUserPath(Common::FS::PathType::UserDir) / "config.json";
    }
    return Common::FS::GetUserPath(Common::FS::PathType::CustomConfigs) / (serial + ".json");
}

// A file that is missing or does not parse gives a discarded value.
nlohmann::json ParseFile(const std::filesystem::path& path) {
    std::ifstream in{path};
    return nlohmann::json::parse(in, nullptr, false);
}

} // Anonymous namespace

bool IsGravityRush2(std::string_view serial) {
    return std::ranges::find(Gr2Serials, serial) != Gr2Serials.end();
}

bool IsGravityRushRemastered(std::string_view serial) {
    return std::ranges::find(GrrSerials, serial) != GrrSerials.end();
}

std::optional<bool> ReadFsr3Enable(const std::string& serial) {
    const auto root = ParseFile(ConfigFile(serial));
    if (!root.is_object() || !root.contains(Section) || !root.at(Section).is_object()) {
        return std::nullopt;
    }
    const auto& section = root.at(Section);
    if (!section.contains(Fsr3EnableKey) || !section.at(Fsr3EnableKey).is_boolean()) {
        return std::nullopt;
    }
    return section.at(Fsr3EnableKey).get<bool>();
}

void WriteFsr3Enable(std::optional<bool> enable, const std::string& serial) {
    const auto path = ConfigFile(serial);
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    auto root = exists ? ParseFile(path) : nlohmann::json::object();
    if (!root.is_object()) {
        LOG_ERROR(Config, "Cannot store the FSR 3.1 switch: {} is not a settings file",
                  path.string());
        return;
    }
    auto& section = root[Section];
    if (!section.is_object()) {
        section = nlohmann::json::object();
    }
    if (enable) {
        section[Fsr3EnableKey] = *enable;
    } else {
        section.erase(Fsr3EnableKey);
    }
    if (section.empty()) {
        root.erase(Section);
    }
    if (!exists && root.empty()) {
        return;
    }
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out{path};
    out << std::setw(2) << root;
}

} // namespace Gr2Mods
