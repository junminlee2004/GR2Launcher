// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class QString; // to avoid including <QString> in this header

namespace Common::FS {

enum class PathType {
    UserDir,          // Where shadPS4 stores its data.
    LogDir,           // Where log files are stored.
    ScreenshotsDir,   // Where screenshots are stored.
    ShaderDir,        // Where shaders are stored.
    TempDataDir,      // Where game temp data is stored.
    GameDataDir,      // Where game data is stored.
    SysModuleDir,     // Where system modules are stored.
    DownloadDir,      // Where downloads/temp files are stored.
    CapturesDir,      // Where rdoc captures are stored.
    CheatsDir,        // Where cheats are stored.
    PatchesDir,       // Where patches are stored.
    MetaDataDir,      // Where game metadata (e.g. trophies and menu backgrounds) is stored.
    CustomTrophy,     // Where custom files for trophies are stored.
    CustomConfigs,    // Where custom files for different games are stored.
    VersionDir,       // Where emulator versions are stored.
    LauncherDir,      // Where launcher stores its data.
    LauncherMetaData, // Where launcher stores its game metadata.
    CacheDir,         // Where pipeline and shader cache is stored.
    FontsDir,         // Where dumped system fonts are stored.
    TrophyDir,        // Where general trophy metadata is stored.
    HomeDir,          // PS4 home directory
};

constexpr auto PORTABLE_DIR = "user";
constexpr auto PORTABLE_LAUNCHER_DIR = "launcher";

// Sub-directories contained within a user data directory
constexpr auto LOG_DIR = "log";
constexpr auto SCREENSHOTS_DIR = "screenshots";
constexpr auto SHADER_DIR = "shader";
constexpr auto GAMEDATA_DIR = "data";
constexpr auto TEMPDATA_DIR = "temp";
constexpr auto SYSMODULES_DIR = "sys_modules";
constexpr auto DOWNLOAD_DIR = "download";
constexpr auto CAPTURES_DIR = "captures";
constexpr auto CHEATS_DIR = "cheats";
constexpr auto PATCHES_DIR = "patches";
constexpr auto METADATA_DIR = "game_data";
constexpr auto CUSTOM_TROPHY = "custom_trophy";
constexpr auto CUSTOM_CONFIGS = "custom_configs";
constexpr auto VERSION_DIR = "versions";
constexpr auto CACHE_DIR = "cache";
constexpr auto FONTS_DIR = "fonts";
constexpr auto HOME_DIR = "home";
constexpr auto TROPHY_DIR = "trophy";

// Filenames
constexpr auto LOG_FILE = "shad_log.txt";

/**
 * Validates a given path.
 *
 * A given path is valid if it meets these conditions:
 * - The path is not empty
 * - The path is not too long
 *
 * @param path Filesystem path
 *
 * @returns True if the path is valid, false otherwise.
 */
[[nodiscard]] bool ValidatePath(const std::filesystem::path& path);

/**
 * Converts a filesystem path to a UTF-8 encoded std::string.
 *
 * @param path Filesystem path
 *
 * @returns UTF-8 encoded std::string.
 */
[[nodiscard]] std::string PathToUTF8String(const std::filesystem::path& path);

/**
 * Gets the filesystem path associated with the PathType enum.
 *
 * @param user_path PathType enum
 *
 * @returns The filesystem path associated with the PathType enum.
 */
[[nodiscard]] const std::filesystem::path& GetUserPath(PathType user_path);

/**
 * Gets the filesystem path associated with the PathType enum as a UTF-8 encoded std::string.
 *
 * @param user_path PathType enum
 *
 * @returns The filesystem path associated with the PathType enum as a UTF-8 encoded std::string.
 */
[[nodiscard]] std::string GetUserPathString(PathType user_path);

/**
 * Sets a new filesystem path associated with the PathType enum.
 * If the filesystem object at new_path is not a directory, this function will not do anything.
 *
 * @param user_path PathType enum
 * @param new_path New filesystem path
 */
void SetUserPath(PathType user_path, const std::filesystem::path& new_path);

/**
 * Converts an std::filesystem::path to a QString.
 * The native underlying string of a path is wstring on Windows and string on POSIX.
 *
 * @param result The resulting QString
 * @param path The path to convert
 */
void PathToQString(QString& result, const std::filesystem::path& path);

/**
 * Converts a QString to an std::filesystem::path.
 * The native underlying string of a path is wstring on Windows and string on POSIX.
 *
 * @param path The path to convert
 */
[[nodiscard]] std::filesystem::path PathFromQString(const QString& path);

/**
 * Converts a path for the launcher's own files (qt_ui.ini, versions.json). While the launcher keeps
 * its data in the portable launcher folder of its working directory, a path inside that directory
 * is written relative to it, so the folder can be moved. Any other path is returned as it is.
 *
 * @param path UTF-8 encoded path
 *
 * @returns UTF-8 encoded path to store.
 */
[[nodiscard]] std::string ToStoredPath(const std::string& path);

/**
 * Resolves a path read from the launcher's own files against the working directory. An empty or
 * absolute path is returned as it is.
 *
 * @param path UTF-8 encoded stored path
 *
 * @returns UTF-8 encoded path.
 */
[[nodiscard]] std::string FromStoredPath(const std::string& path);

/**
 * Tells whether a path lies inside the launcher's data folder: the portable launcher folder of the
 * working directory, or the platform data folder when there is none. Symbolic links are resolved
 * first, so a link inside the folder to a file outside it does not count.
 *
 * @param path The path to check
 *
 * @returns true if the path is inside the launcher folder.
 */
[[nodiscard]] bool IsInLauncherDir(const std::filesystem::path& path);

/**
 * Recursively searches for a game directory by its ID.
 * Limits search depth to prevent excessive filesystem traversal.
 *
 * @param dir Base directory to start the search from
 * @param game_id The game ID to search for
 * @param max_depth Maximum directory depth to search
 *
 * @returns Path to eboot.bin if found, std::nullopt otherwise
 */
[[nodiscard]] std::optional<std::filesystem::path> FindGameByID(const std::filesystem::path& dir,
                                                                const std::string& game_id,
                                                                int max_depth);

} // namespace Common::FS
