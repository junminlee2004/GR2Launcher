// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

// The server of Gravity Rush 2's online play: httpHostOverride and httpHostOverridePort in the
// "GR2Fork" section of config.json or of a per-game file. The emulator reads both keys itself, from
// the per-game file first; they are not part of the settings model.
namespace Gr2Online {

struct Server {
    std::string host; ///< Empty when none is set
    int port{};       ///< 0 when none is set
};

/// Reads the server from the per-game file of serial, or from config.json for an empty serial. A
/// host written as a URL or as host:port is split the way the emulator splits it.
Server ReadServer(const std::string& serial = "");

/// Writes the server into the same file and leaves the rest of the file as it is. The host is split
/// like ReadServer splits it; an empty host and a port outside 1-65535 remove their key.
void WriteServer(Server server, const std::string& serial = "");

} // namespace Gr2Online
