// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

// The button prompt packs of GR2fork for Gravity Rush 2 and Gravity Rush Remastered. The emulator
// reads them from the prompt_mods folder of its user folder; the launcher ships them in the
// prompt_mods folder of its install folder.
namespace PromptPacks {

/// The pack layout of the connected pad: "xbox" for an Xbox pad, "switch" for a Switch Pro
/// controller or a pair of Joy-Cons, "steamdeck" for the controls of a Steam Deck,
/// "steamcontroller" for a Steam Controller, empty for any other pad or none. The selected gamepad
/// decides when it is connected, else the first pad with a layout.
std::string ConnectedPadLayout(const std::string& selected_guid);

} // namespace PromptPacks
