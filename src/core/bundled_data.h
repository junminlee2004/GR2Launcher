// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// The emulator files that the launcher ships in its install folder, outside the user folder, so
// that the self-updater replaces them together with the launcher: the button prompt and mod packs
// of prompt_mods and the FSR 4 files of fsr4_v07. The emulator reads them from the user folder.
namespace BundledData {

/// Copies each pack of the install folder into the user folder when the copy there is missing or
/// has another pack.json, and the FSR 4 files into the default FSR 4 folder of the user folder
/// when that folder is missing or holds a copy of other files.
void Sync();

} // namespace BundledData
