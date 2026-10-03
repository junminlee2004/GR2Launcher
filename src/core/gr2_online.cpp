// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "common/logging/log.h"
#include "common/path_util.h"
#include "gr2_online.h"

namespace Gr2Online {

namespace {

constexpr const char* Section = "GR2Fork";
constexpr const char* HostKey = "httpHostOverride";
constexpr const char* PortKey = "httpHostOverridePort";

std::filesystem::path ConfigFile(const std::string& serial) {
    if (serial.empty()) {
        return Common::FS::GetUserPath(Common::FS::PathType::UserDir) / "config.json";
    }
    return Common::FS::GetUserPath(Common::FS::PathType::CustomConfigs) / (serial + ".json");
}

// A file that is missing or does not parse gives a discarded value.
nlohmann::json ParseFile(const std::filesystem::path& path) {
    std::ifstream in{path};
    return nlohmann::json::parse(in, nullptr, false);
}

bool IsPort(int port) {
    return port > 0 && port <= 65535;
}

// Keeps only the host and the port of a URL, and moves the port of host:port to the port, where it
// wins. An address with more than one colon is IPv6 and stays as it is.
void SplitHost(Server& server) {
    std::string& host = server.host;
    if (const auto scheme = host.find("://"); scheme != std::string::npos) {
        host.erase(0, scheme + 3);
    }
    if (const auto slash = host.find('/'); slash != std::string::npos) {
        host.resize(slash);
    }
    if (const auto colon = host.rfind(':'); colon != std::string::npos && host.find(':') == colon) {
        const char* end = host.data() + host.size();
        int port = 0;
        if (std::from_chars(host.data() + colon + 1, end, port).ec == std::errc{} && IsPort(port)) {
            server.port = port;
        }
        host.resize(colon);
    }
}

} // Anonymous namespace

Server ReadServer(const std::string& serial) {
    Server server;
    const auto root = ParseFile(ConfigFile(serial));
    if (!root.is_object() || !root.contains(Section) || !root.at(Section).is_object()) {
        return server;
    }
    const auto& section = root.at(Section);
    if (section.contains(HostKey) && section.at(HostKey).is_string()) {
        server.host = section.at(HostKey).get<std::string>();
    }
    if (section.contains(PortKey) && section.at(PortKey).is_number_integer() &&
        IsPort(section.at(PortKey).get<int>())) {
        server.port = section.at(PortKey).get<int>();
    }
    SplitHost(server);
    return server;
}

void WriteServer(Server server, const std::string& serial) {
    SplitHost(server);
    const auto path = ConfigFile(serial);
    auto root = ParseFile(path);
    if (!root.is_object()) {
        LOG_ERROR(Config, "Cannot store the Gravity Rush 2 server: {} is not a settings file",
                  path.string());
        return;
    }
    auto& section = root[Section];
    if (!section.is_object()) {
        section = nlohmann::json::object();
    }
    if (server.host.empty()) {
        section.erase(HostKey);
    } else {
        section[HostKey] = server.host;
    }
    if (IsPort(server.port)) {
        section[PortKey] = server.port;
    } else {
        section.erase(PortKey);
    }
    if (section.empty()) {
        root.erase(Section);
    }
    std::ofstream out{path};
    out << std::setw(2) << root;
}

} // namespace Gr2Online
