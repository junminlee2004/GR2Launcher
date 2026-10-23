// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

// The mod switches of the GR2fork emulator: keys of the "GR2Fork" section of config.json or of a
// per-game file. The emulator reads the per-game file first; the keys are not part of the settings
// model.
namespace Gr2Mods {

/// Whether serial is a release of Gravity Rush 2, by the emulator's list.
bool IsGravityRush2(std::string_view serial);

/// Whether serial is a release of Gravity Rush Remastered, by the emulator's list.
bool IsGravityRushRemastered(std::string_view serial);

/// fsr3Enable (the FSR 3.1 upscaler of Gravity Rush 2) of the per-game file of serial, or of
/// config.json for an empty serial. Empty when the file does not set it.
std::optional<bool> ReadFsr3Enable(const std::string& serial = "");

/// Sets fsr3Enable in the same file, or removes it for an empty value, and leaves the rest of the
/// file as it is. A missing file is created when a value is set; a file that holds no JSON object
/// is left alone.
void WriteFsr3Enable(std::optional<bool> enable, const std::string& serial = "");

} // namespace Gr2Mods
