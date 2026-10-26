// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <string_view>
#include <SDL3/SDL.h>

#include "common/logging/log.h"
#include "input/input.h"
#include "prompt_packs.h"

namespace PromptPacks {

namespace {

// Valve's pads. Under Steam Input SDL reports the vendor and product of the pad behind Steam's
// virtual gamepad.
constexpr Uint16 ValveVendor = 0x28DE;
constexpr Uint16 SteamDeckProduct = 0x1205;
constexpr Uint16 SteamVirtualGamepadProduct = 0x11FF;
constexpr std::array<Uint16, 11> SteamControllerProducts{
    0x1101, 0x1102, 0x1105, 0x1106, 0x1142, 0x1201, 0x1202, 0x1302, 0x1303, 0x1304, 0x1305,
};

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

std::string LayoutOfValvePad(Uint16 product) {
    if (product == SteamDeckProduct) {
        return "steamdeck";
    }
    if (std::find(SteamControllerProducts.begin(), SteamControllerProducts.end(), product) !=
        SteamControllerProducts.end()) {
        return "steamcontroller";
    }
    // Steam's virtual gamepad without the information of the pad behind it. Steam sets
    // SteamDeck=1 on a Steam Deck, where that pad is most likely the built-in controls.
    const char* deck = SDL_getenv("SteamDeck");
    if (product == SteamVirtualGamepadProduct && deck != nullptr && std::string_view(deck) == "1") {
        return "steamdeck";
    }
    return {};
}

std::string LayoutOfPad(SDL_JoystickID id) {
    const Uint16 vendor = SDL_GetGamepadVendorForID(id);
    const Uint16 product = SDL_GetGamepadProductForID(id);
    const SDL_GamepadType type = SDL_GetGamepadTypeForID(id);
    std::string layout = vendor == ValveVendor ? LayoutOfValvePad(product) : std::string{};
    if (layout.empty()) {
        layout = LayoutOfType(type);
    }
    const char* type_name = SDL_GetGamepadStringForType(type);
    LOG_INFO(Input, "Pad {:04x}:{:04x} ({}): button prompt layout \"{}\"", vendor, product,
             type_name != nullptr ? type_name : "unknown", layout);
    return layout;
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
            layout = LayoutOfPad(ids[selected]);
        } else {
            for (int i = 0; i < count && layout.empty(); ++i) {
                layout = LayoutOfPad(ids[i]);
            }
        }
        SDL_free(ids);
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    return layout;
}

} // namespace PromptPacks
