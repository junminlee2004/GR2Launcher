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
constexpr const char* Fsr4EnableKey = "fsr4Enable";
constexpr const char* GrrFsr3EnableKey = "grrFsr3Enable";
constexpr const char* GrrFsr4EnableKey = "grrFsr4Enable";
constexpr const char* Fsr4AssetDirKey = "fsr4AssetDir";
constexpr const char* Gr2PromptLayoutKey = "gr2PromptLayout";
constexpr const char* GrrPromptLayoutKey = "grrPromptLayout";
constexpr const char* Gr2HalloweenItemsKey = "gr2HalloweenItems";
#ifdef _WIN32
constexpr const char* DlssEnableKey = "dlssEnable";
constexpr const char* GrrDlssEnableKey = "grrDlssEnable";
#endif

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

// The value of key in the "GR2Fork" section of the file of serial when it is a T. contains() is
// false for a value that is not an object.
template <typename T>
std::optional<T> ReadKey(const char* key, const std::string& serial) {
    const auto root = ParseFile(ConfigFile(serial));
    if (!root.contains(Section) || !root.at(Section).contains(key)) {
        return std::nullopt;
    }
    if (const T* value = root.at(Section).at(key).get_ptr<const T*>()) {
        return *value;
    }
    return std::nullopt;
}

// Sets key in the "GR2Fork" section of the file of serial, or removes it for an empty value.
template <typename T>
void WriteKey(const char* key, const std::optional<T>& value, const std::string& serial) {
    const auto path = ConfigFile(serial);
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    auto root = exists ? ParseFile(path) : nlohmann::json::object();
    if (!root.is_object()) {
        LOG_ERROR(Config, "Cannot store GR2Fork.{}: {} is not a settings file", key,
                  Common::FS::PathToUTF8String(path));
        return;
    }
    auto& section = root[Section];
    if (!section.is_object()) {
        section = nlohmann::json::object();
    }
    if (value) {
        section[key] = *value;
    } else {
        section.erase(key);
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

} // Anonymous namespace

bool IsGravityRush2(std::string_view serial) {
    return std::ranges::find(Gr2Serials, serial) != Gr2Serials.end();
}

bool IsGravityRushRemastered(std::string_view serial) {
    return std::ranges::find(GrrSerials, serial) != GrrSerials.end();
}

std::optional<bool> ReadFsr3Enable(const std::string& serial) {
    return ReadKey<bool>(Fsr3EnableKey, serial);
}

void WriteFsr3Enable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(Fsr3EnableKey, enable, serial);
}

std::optional<bool> ReadFsr4Enable(const std::string& serial) {
    return ReadKey<bool>(Fsr4EnableKey, serial);
}

void WriteFsr4Enable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(Fsr4EnableKey, enable, serial);
}

std::optional<bool> ReadGrrFsr3Enable(const std::string& serial) {
    return ReadKey<bool>(GrrFsr3EnableKey, serial);
}

void WriteGrrFsr3Enable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(GrrFsr3EnableKey, enable, serial);
}

std::optional<bool> ReadGrrFsr4Enable(const std::string& serial) {
    return ReadKey<bool>(GrrFsr4EnableKey, serial);
}

void WriteGrrFsr4Enable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(GrrFsr4EnableKey, enable, serial);
}

std::optional<std::string> ReadFsr4AssetDir(const std::string& serial) {
    return ReadKey<std::string>(Fsr4AssetDirKey, serial);
}

void WriteFsr4AssetDir(std::optional<std::string> dir, const std::string& serial) {
    WriteKey(Fsr4AssetDirKey, dir, serial);
}

std::optional<std::string> ReadGr2PromptLayout(const std::string& serial) {
    return ReadKey<std::string>(Gr2PromptLayoutKey, serial);
}

void WriteGr2PromptLayout(std::optional<std::string> layout, const std::string& serial) {
    WriteKey(Gr2PromptLayoutKey, layout, serial);
}

std::optional<std::string> ReadGrrPromptLayout(const std::string& serial) {
    return ReadKey<std::string>(GrrPromptLayoutKey, serial);
}

void WriteGrrPromptLayout(std::optional<std::string> layout, const std::string& serial) {
    WriteKey(GrrPromptLayoutKey, layout, serial);
}

std::optional<bool> ReadGr2HalloweenItems(const std::string& serial) {
    return ReadKey<bool>(Gr2HalloweenItemsKey, serial);
}

void WriteGr2HalloweenItems(std::optional<bool> enable, const std::string& serial) {
    WriteKey(Gr2HalloweenItemsKey, enable, serial);
}

std::filesystem::path ResolveFsr4AssetDir(const std::string& dir) {
    const auto& user = Common::FS::GetUserPath(Common::FS::PathType::UserDir);
    if (dir.empty()) {
        return user / "fsr4_v07";
    }
    return user / std::filesystem::path{std::u8string(dir.begin(), dir.end())};
}

#ifdef _WIN32
std::optional<bool> ReadDlssEnable(const std::string& serial) {
    return ReadKey<bool>(DlssEnableKey, serial);
}

void WriteDlssEnable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(DlssEnableKey, enable, serial);
}

std::optional<bool> ReadGrrDlssEnable(const std::string& serial) {
    return ReadKey<bool>(GrrDlssEnableKey, serial);
}

void WriteGrrDlssEnable(std::optional<bool> enable, const std::string& serial) {
    WriteKey(GrrDlssEnableKey, enable, serial);
}
#endif

} // namespace Gr2Mods
