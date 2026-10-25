// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <SDL3/SDL.h>

#include "common/logging/log.h"
#include "common/path_util.h"
#include "input/input.h"
#include "prompt_packs.h"

namespace PromptPacks {

namespace {

std::string LayoutOfType(SDL_GamepadType type) {
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return "xbox";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return "switch";
    default:
        return {};
    }
}

std::optional<std::string> ReadText(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

} // Anonymous namespace

std::string ConnectedPadLayout(const std::string& selected_guid) {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        LOG_WARNING(Input, "Cannot check the pad type: {}", SDL_GetError());
        return {};
    }
    SDL_UpdateJoysticks();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::string layout;
    if (ids != nullptr) {
        const int selected =
            selected_guid.empty() ? -1 : GamepadSelect::GetIndexfromGUID(ids, count, selected_guid);
        if (selected >= 0) {
            layout = LayoutOfType(SDL_GetGamepadTypeForID(ids[selected]));
        } else {
            for (int i = 0; i < count && layout.empty(); ++i) {
                layout = LayoutOfType(SDL_GetGamepadTypeForID(ids[i]));
            }
        }
        SDL_free(ids);
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    return layout;
}

void SyncBundledPacks() {
    // The launcher runs from its install folder, which also decides whether it is portable.
    const auto source = std::filesystem::current_path() / "prompt_mods";
    const auto target = Common::FS::GetUserPath(Common::FS::PathType::UserDir) / "prompt_mods";
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
            if (!manifest || manifest == ReadText(dest / "pack.json")) {
                continue;
            }
            // The copy goes to a temporary folder first, so an interrupted copy leaves no pack.json
            // that matches.
            auto staging = dest;
            staging += ".new";
            std::filesystem::remove_all(staging, ec);
            std::filesystem::create_directories(staging.parent_path(), ec);
            std::filesystem::copy(pack, staging, std::filesystem::copy_options::recursive, ec);
            if (!ec) {
                std::filesystem::remove_all(dest, ec);
                std::filesystem::rename(staging, dest, ec);
            }
            if (ec) {
                LOG_ERROR(Common_Filesystem, "Cannot copy the prompt pack {}: {}",
                          Common::FS::PathToUTF8String(pack), ec.message());
                std::filesystem::remove_all(staging, ec);
                continue;
            }
            LOG_INFO(Common_Filesystem, "Copied the prompt pack {} to {}",
                     Common::FS::PathToUTF8String(pack), Common::FS::PathToUTF8String(dest));
        }
    }
}

} // namespace PromptPacks
