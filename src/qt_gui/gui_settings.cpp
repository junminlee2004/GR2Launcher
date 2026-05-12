// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/emulator_path.h"
#include "common/path_util.h"
#include "gui_settings.h"

gui_settings::gui_settings(QObject* parent) : settings(parent) {
    m_settings = std::make_unique<QSettings>(ComputeSettingsDir() + "qt_ui.ini",
                                             QSettings::Format::IniFormat, parent);
}

QString gui_settings::GetVersionExecutablePath(const QString& /*versionName*/) const {
    // Version manager has been stripped. The launcher auto-detects a single
    // executable next to itself whose name begins with "shadPS4gr2fork", so
    // the version name parameter is ignored — there is only one version.
    const auto exe = EmulatorPath::FindGr2forkExecutable();
    if (exe.empty()) {
        return QString();
    }
    QString result;
    Common::FS::PathToQString(result, exe);
    return result;
}
