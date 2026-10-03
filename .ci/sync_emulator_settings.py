#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Copies src/core/emulator_settings.h and .cpp from an emulator checkout and re-applies the
# launcher's one change, which logs a settings file that fails to load instead of aborting.
# Also compares the User struct in the two src/core/user_manager.h files, kept in step by hand.
#
# Usage: .ci/sync_emulator_settings.py <emulator checkout> [--check]
# --check writes nothing and exits with 1 when the copy would change a file. Either mode exits
# with 1 when the User structs differ.

import re
import sys
from pathlib import Path

LAUNCHER = Path(__file__).resolve().parent.parent
PAIR = ("src/core/emulator_settings.cpp", "src/core/emulator_settings.h")
ABORT = b'UNREACHABLE_MSG("Error loading settings: {}", e.what());'
LOG = b'LOG_ERROR(Config, "Error loading settings: {}", e.what());'


def user_struct(root):
    text = (root / "src/core/user_manager.h").read_text(encoding="utf-8")
    text = re.sub(r"//[^\n]*", "", text)
    fields = re.search(r"struct User \{(.*?)\};", text, re.S).group(1).split(";")
    macro = re.search(r"NLOHMANN_DEFINE_TYPE\w*\(User,[^)]*\)", text).group(0)
    return [" ".join(f.split()) for f in fields if f.strip()], " ".join(macro.split())


args = [a for a in sys.argv[1:] if a != "--check"]
if len(args) != 1:
    sys.exit("usage: sync_emulator_settings.py <emulator checkout> [--check]")
emulator, check, failed = Path(args[0]), "--check" in sys.argv, False
for path in PAIR:
    data = (emulator / path).read_bytes()
    if path.endswith(".cpp"):
        if data.count(ABORT) != 1:
            sys.exit(f"{path}: the line to patch is not in the emulator's file exactly once")
        data = data.replace(ABORT, LOG)
    if (LAUNCHER / path).read_bytes() == data:
        continue
    if check:
        print(f"{path} differs from the emulator's copy")
        failed = True
    else:
        (LAUNCHER / path).write_bytes(data)
        print(f"updated {path}")
if user_struct(emulator) != user_struct(LAUNCHER):
    print("src/core/user_manager.h: the User struct differs from the emulator's")
    failed = True
sys.exit(1 if failed else 0)
