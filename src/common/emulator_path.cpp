// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/emulator_path.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>
#include <QCoreApplication>

namespace EmulatorPath {

namespace {

// Normalize a filename stem for comparison: lowercase every ASCII letter and
// drop everything that isn't a letter or digit. This way the user can name
// their build anything reasonable and we'll still find it. All of the
// following match against the prefix "shadPS4gr2fork":
//
//   shadPS4gr2fork.exe              -> "shadps4gr2fork"
//   ShadPS4Gr2Fork.exe              -> "shadps4gr2fork"
//   SHADPS4GR2FORK.EXE              -> "shadps4gr2fork"
//   shadPS4-gr2fork.exe             -> "shadps4gr2fork"
//   shadPS4_gr2fork-v1.2.3.exe      -> "shadps4gr2forkv123"
//   ShadPS4 GR2 Fork (release).exe  -> "shadps4gr2forkrelease"
//
// We then require the normalized stem to START WITH the normalized prefix,
// not just contain it -- so unrelated files that happen to mention the
// substring inside their name won't accidentally match.
std::string Normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

bool ExtensionLooksRunnable(const std::filesystem::path& p) {
#ifdef _WIN32
    // Windows: require .exe (case-insensitive). p.extension() returns the dot
    // plus the extension, e.g. ".EXE". Lowercase before comparing.
    const std::string ext = p.extension().string();
    if (ext.size() != 4)
        return false;
    return (std::tolower(static_cast<unsigned char>(ext[1])) == 'e' &&
            std::tolower(static_cast<unsigned char>(ext[2])) == 'x' &&
            std::tolower(static_cast<unsigned char>(ext[3])) == 'e');
#else
    // Linux/macOS: accept any regular file matching the prefix -- the user may
    // ship the build as `shadPS4gr2fork`, `shadPS4gr2fork.AppImage`, etc.
    (void)p;
    return true;
#endif
}

} // namespace

std::filesystem::path FindGr2forkExecutable() {
    // Determine the directory to scan for the emulator.
    //
    // Normal case (native binary on any OS): use the launcher's own directory,
    // i.e. QCoreApplication::applicationDirPath().
    //
    // AppImage case (Linux): when the launcher is packaged as an AppImage, the
    // AppImage runtime mounts the squashfs to a transient path like
    // /tmp/.mount_xxxxxx/ before exec'ing the inner binary. applicationDirPath()
    // would then resolve to that mount path -- which never contains the user's
    // emulator. The AppImage runtime exposes the .AppImage file's *real*
    // on-disk path via the $APPIMAGE env var; we use dirname($APPIMAGE) so the
    // launcher finds the emulator dropped next to the .AppImage file. If
    // $APPIMAGE isn't set we're not in an AppImage, fall back to the usual
    // applicationDirPath().
    std::filesystem::path app_dir;
#ifndef _WIN32
    if (const char* appimage_env = std::getenv("APPIMAGE"); appimage_env && *appimage_env) {
        std::error_code app_ec;
        std::filesystem::path appimage_path(appimage_env);
        std::filesystem::path parent = appimage_path.parent_path();
        if (!parent.empty() && std::filesystem::is_directory(parent, app_ec)) {
            LOG_INFO(Common,
                     "EmulatorPath: running inside AppImage ($APPIMAGE={}); "
                     "searching for emulator next to the .AppImage file: {}",
                     appimage_env, parent.string());
            app_dir = parent;
        }
    }
#endif
    if (app_dir.empty()) {
        const QString app_dir_q = QCoreApplication::applicationDirPath();
        app_dir =
#ifdef _WIN32
            std::filesystem::path(app_dir_q.toStdWString());
#else
            std::filesystem::path(app_dir_q.toStdString());
#endif
    }

    std::error_code ec;
    if (!std::filesystem::exists(app_dir, ec) || !std::filesystem::is_directory(app_dir, ec)) {
        LOG_WARNING(Common, "EmulatorPath: launcher dir '{}' is not a directory ({})",
                    app_dir.string(), ec.message());
        return {};
    }

    const std::string normalized_prefix = Normalize(kGr2forkExecutablePrefix);

    // Track what we considered so failures are diagnosable from the log.
    std::vector<std::string> rejected;
    rejected.reserve(16);

    for (std::filesystem::directory_iterator it(app_dir, ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto& entry = *it;
        std::error_code fec;
        if (!entry.is_regular_file(fec))
            continue;

        const auto& path = entry.path();
        const std::string stem = path.stem().string();
        const std::string normalized = Normalize(stem);

        // Prefix check on normalized strings -- case-insensitive AND
        // punctuation-insensitive (hyphens, underscores, spaces all ignored).
        if (normalized.size() < normalized_prefix.size() ||
            normalized.compare(0, normalized_prefix.size(), normalized_prefix) != 0) {
            rejected.push_back(path.filename().string() + " (prefix mismatch)");
            continue;
        }
        if (!ExtensionLooksRunnable(path)) {
            rejected.push_back(path.filename().string() + " (wrong extension)");
            continue;
        }

        LOG_INFO(Common, "EmulatorPath: matched '{}' (normalized stem '{}' starts with '{}')",
                 path.filename().string(), normalized, normalized_prefix);
        return path;
    }

    // Surface the search context when nothing matches -- invaluable when the
    // user reports "it can't find the exe" and we need to know why.
    LOG_WARNING(Common, "EmulatorPath: no match in '{}' (looking for stem normalized to '{}*')",
                app_dir.string(), normalized_prefix);
    for (const auto& r : rejected) {
        LOG_WARNING(Common, "  rejected: {}", r);
    }
    return {};
}

} // namespace EmulatorPath
