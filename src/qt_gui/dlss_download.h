// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCoreApplication>

class QWidget;

// Downloads the NVIDIA files the emulator loads for DLSS in Gravity Rush 2 and Gravity Rush
// Remastered.
class DlssDownload {
    Q_DECLARE_TR_FUNCTIONS(DlssDownload)

public:
    /// Whether the dlss folder of the user folder holds the four Streamline and DLSS files. When
    /// one is missing, offers to download them from NVIDIA and shows the reason when that fails.
    static bool EnsureFiles(QWidget* parent);
};
