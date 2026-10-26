// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
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

/// fsr4Enable (the experimental FSR 4 INT8 upscaler of Gravity Rush 2) of the per-game file of
/// serial, or of config.json for an empty serial. Empty when the file does not set it.
std::optional<bool> ReadFsr4Enable(const std::string& serial = "");

/// Sets fsr4Enable as WriteFsr3Enable sets fsr3Enable.
void WriteFsr4Enable(std::optional<bool> enable, const std::string& serial = "");

/// grrFsr3Enable (the FSR 3.1 anti-aliasing of Gravity Rush Remastered) of the per-game file of
/// serial, or of config.json for an empty serial. Empty when the file does not set it.
std::optional<bool> ReadGrrFsr3Enable(const std::string& serial = "");

/// Sets grrFsr3Enable as WriteFsr3Enable sets fsr3Enable.
void WriteGrrFsr3Enable(std::optional<bool> enable, const std::string& serial = "");

/// grrFsr4Enable (the experimental FSR 4 INT8 anti-aliasing of Gravity Rush Remastered) of the
/// per-game file of serial, or of config.json for an empty serial. Empty when the file does not
/// set it.
std::optional<bool> ReadGrrFsr4Enable(const std::string& serial = "");

/// Sets grrFsr4Enable as WriteFsr3Enable sets fsr3Enable.
void WriteGrrFsr4Enable(std::optional<bool> enable, const std::string& serial = "");

/// fsr4AssetDir, the UTF-8 path of the folder of the FSR 4 shader and model files, of the
/// per-game file of serial, or of config.json for an empty serial. Empty when the file does not
/// set it.
std::optional<std::string> ReadFsr4AssetDir(const std::string& serial = "");

/// Sets fsr4AssetDir as WriteFsr3Enable sets fsr3Enable.
void WriteFsr4AssetDir(std::optional<std::string> dir, const std::string& serial = "");

/// gr2PromptLayout, the button prompt pack of Gravity Rush 2 ("off", "xbox", "switch",
/// "steamdeck" or "steamcontroller"), of the per-game file of serial, or of config.json for an
/// empty serial. Empty when the file does not set it.
std::optional<std::string> ReadGr2PromptLayout(const std::string& serial = "");

/// Sets gr2PromptLayout as WriteFsr3Enable sets fsr3Enable.
void WriteGr2PromptLayout(std::optional<std::string> layout, const std::string& serial = "");

/// grrPromptLayout, the button prompt pack of Gravity Rush Remastered ("off", "xbox", "switch",
/// "steamdeck" or "steamcontroller"), of the per-game file of serial, or of config.json for an
/// empty serial. Empty when the file does not set it.
std::optional<std::string> ReadGrrPromptLayout(const std::string& serial = "");

/// Sets grrPromptLayout as WriteFsr3Enable sets fsr3Enable.
void WriteGrrPromptLayout(std::optional<std::string> layout, const std::string& serial = "");

/// gr2HalloweenItems (the Halloween photo item mod pack of Gravity Rush 2, on when the key is
/// missing) of the per-game file of serial, or of config.json for an empty serial. Empty when the
/// file does not set it.
std::optional<bool> ReadGr2HalloweenItems(const std::string& serial = "");

/// Sets gr2HalloweenItems as WriteFsr3Enable sets fsr3Enable.
void WriteGr2HalloweenItems(std::optional<bool> enable, const std::string& serial = "");

/// The folder the emulator reads the FSR 4 files from for the fsr4AssetDir value dir: the fsr4_v07
/// folder of the user folder for an empty value, else dir, taken from the user folder when it is
/// relative.
std::filesystem::path ResolveFsr4AssetDir(const std::string& dir);

} // namespace Gr2Mods
