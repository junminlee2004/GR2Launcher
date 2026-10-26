// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <QCoreApplication>

class QWidget;
class gui_settings;

// Offers the button prompt pack of the connected pad before Gravity Rush 2 or Gravity Rush
// Remastered starts.
class PromptPackOffer {
    Q_DECLARE_TR_FUNCTIONS(PromptPackOffer)

public:
    /// Asks whether the game of serial shows the buttons of the connected pad that has a layout
    /// (for an Xbox pad: the Xbox, Steam Deck or Steam Controller buttons), unless it already does
    /// or the player turned the question off for that pad type. The chosen layout is stored in
    /// config.json.
    static void Run(QWidget* parent, const std::string& serial, gui_settings& settings);
};
