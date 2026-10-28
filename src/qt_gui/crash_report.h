// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <QCoreApplication>
#include <QString>

class QWidget;
class gui_settings;

// Reports a game session whose log ends without the shutdown lines of the emulator as an issue of
// GR2fork on GitHub.
class CrashReport {
    Q_DECLARE_TR_FUNCTIONS(CrashReport)

public:
    /**
     * Reads the log of a game that closed. Returns a report when none of its last lines is the
     * play time or "Cache dumped" line of a shutdown. A log without the startup line, or at its
     * size limit, gives none: the emulator then leaves those lines out.
     */
    static std::optional<CrashReport> Check(const QString& log_path, qint64 size_limit,
                                            const QString& game);

    /// Asks whether to submit the log and posts it as an issue after a GitHub sign-in.
    void Offer(QWidget* parent, gui_settings& settings) const;

private:
    QString file_name;
    QString game;
    QString content; ///< The log, without its middle when it is long.
    QString tail;    ///< The last lines of the log.
    QString summary; ///< Version, game and hardware lines of the log.
};
