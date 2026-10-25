// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "bundled_data.h"
#include "common/logging/log.h"
#include "common/path_util.h"
#include "gr2_mods.h"

namespace BundledData {

namespace {

// Written into the user folder's copy of the FSR 4 files: the name, size and write time of each
// file of the install folder's copy at the time of the copy.
constexpr const char* Fsr4CopyStamp = "launcher_copy.txt";

std::optional<std::string> ReadText(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

// One line per file of folder with its name, size and write time; empty when folder cannot be
// read.
std::string FolderStamp(const std::filesystem::path& folder) {
    std::error_code ec;
    std::vector<std::string> lines;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto size = entry.file_size(ec);
        if (ec) {
            return {};
        }
        const auto time = entry.last_write_time(ec).time_since_epoch();
        if (ec) {
            return {};
        }
        // Microseconds fit 64 bits for each clock epoch of the standard libraries.
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(time).count();
        lines.push_back(Common::FS::PathToUTF8String(entry.path().filename()) + '\t' +
                        std::to_string(size) + '\t' + std::to_string(micros) + '\n');
    }
    if (ec) {
        return {};
    }
    std::sort(lines.begin(), lines.end());
    std::string stamp;
    for (const auto& line : lines) {
        stamp += line;
    }
    return stamp;
}

// Replaces dest with a copy of source, plus a file stamp_name holding stamp when stamp_name is
// set. The copy goes to a temporary folder first, so an interrupted copy leaves no pack.json or
// stamp that matches.
void ReplaceFolder(const std::filesystem::path& source, const std::filesystem::path& dest,
                   const char* stamp_name = nullptr, const std::string& stamp = {}) {
    std::error_code ec;
    auto staging = dest;
    staging += ".new";
    std::filesystem::remove_all(staging, ec);
    std::filesystem::create_directories(staging.parent_path(), ec);
    std::filesystem::copy(source, staging, std::filesystem::copy_options::recursive, ec);
    if (!ec && stamp_name != nullptr) {
        std::ofstream file{staging / stamp_name, std::ios::binary};
        file << stamp;
        file.close();
        if (!file) {
            ec = std::make_error_code(std::errc::io_error);
        }
    }
    if (!ec) {
        std::filesystem::remove_all(dest, ec);
        std::filesystem::rename(staging, dest, ec);
    }
    if (ec) {
        LOG_ERROR(Common_Filesystem, "Cannot copy {}: {}", Common::FS::PathToUTF8String(source),
                  ec.message());
        std::filesystem::remove_all(staging, ec);
        return;
    }
    LOG_INFO(Common_Filesystem, "Copied {} to {}", Common::FS::PathToUTF8String(source),
             Common::FS::PathToUTF8String(dest));
}

void SyncPacks(const std::filesystem::path& install, const std::filesystem::path& user) {
    const auto source = install / "prompt_mods";
    const auto target = user / "prompt_mods";
    std::error_code ec;
    if (!std::filesystem::is_directory(source, ec)) {
        return;
    }
    for (const auto& game : std::filesystem::directory_iterator(source, ec)) {
        if (!game.is_directory(ec)) {
            continue;
        }
        for (const auto& layout : std::filesystem::directory_iterator(game.path(), ec)) {
            const auto pack = layout.path();
            const auto manifest = ReadText(pack / "pack.json");
            const auto dest = target / game.path().filename() / pack.filename();
            if (manifest && manifest != ReadText(dest / "pack.json")) {
                ReplaceFolder(pack, dest);
            }
        }
    }
}

// The install folder's copy changes only when the launcher is updated or extracted again, which
// also gives its files new write times, so the stamp needs no file to be read.
void SyncFsr4Files(const std::filesystem::path& install) {
    const auto source = install / "fsr4_v07";
    const auto dest = Gr2Mods::ResolveFsr4AssetDir({});
    std::error_code ec;
    if (!std::filesystem::is_directory(source, ec)) {
        return;
    }
    const std::string stamp = FolderStamp(source);
    if (!stamp.empty() && stamp != ReadText(dest / Fsr4CopyStamp)) {
        ReplaceFolder(source, dest, Fsr4CopyStamp, stamp);
    }
}

} // Anonymous namespace

void Sync() {
    // The launcher runs from its install folder, which also decides whether it is portable.
    const auto install = std::filesystem::current_path();
    SyncPacks(install, Common::FS::GetUserPath(Common::FS::PathType::UserDir));
    SyncFsr4Files(install);
}

} // namespace BundledData
