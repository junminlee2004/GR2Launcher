// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

namespace EmulatorPath {

// Scans the launcher's own directory for an emulator executable whose filename
// (case-insensitively) begins with "shadPS4gr2fork".
//   - Windows: requires a `.exe` extension.
//   - Linux/macOS: accepts any regular file matching the prefix (e.g.
//     `shadPS4gr2fork`, `shadPS4gr2fork.AppImage`).
//
// Returns an empty path if nothing is found. Errors during iteration are
// swallowed silently — callers should treat an empty result as "not found".
std::filesystem::path FindGr2forkExecutable();

// Convenience: the literal prefix the launcher looks for. Exposed so error
// messages can stay in sync with the matcher.
constexpr const char* kGr2forkExecutablePrefix = "shadPS4gr2fork";

} // namespace EmulatorPath
