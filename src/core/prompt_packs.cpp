// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SDL3/SDL.h>

#include "common/logging/log.h"
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

} // namespace PromptPacks
