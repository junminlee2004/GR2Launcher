// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include "common/scm_rev.h"

namespace Common {

constexpr char g_version[]  = "0.13.0";
constexpr bool g_is_release = true;

constexpr char g_scm_rev[]         = "a1babe383555388608be653374573dcd9b9d16ec";
constexpr char g_scm_branch[]      = "gr2fork-main";
constexpr char g_scm_desc[]        = "v.0.13.0-1-ga1babe38";
constexpr char g_scm_remote_name[] = "origin";
constexpr char g_scm_remote_url[]  = "https://github.com/junminlee2004/GR2fork.git";
constexpr char g_scm_date[]        = "2026-05-11 16:04:27";

const std::string GetRemoteNameFromLink() {
    std::string remote_url(Common::g_scm_remote_url);
    std::string remote_host;
    try {
        if (remote_url.starts_with("http")) {
            if (*remote_url.rbegin() == '/') {
                remote_url.pop_back();
            }
            remote_host = remote_url.substr(19, remote_url.rfind('/') - 19);
        } else if (remote_url.starts_with("git@")) {
            auto after_comma_pos = remote_url.find(':') + 1, slash_pos = remote_url.find('/');
            remote_host = remote_url.substr(after_comma_pos, slash_pos - after_comma_pos);
        } else {
            remote_host = "unknown";
        }
    } catch (...) {
        remote_host = "unknown";
    }
    return remote_host;
}

} // namespace

