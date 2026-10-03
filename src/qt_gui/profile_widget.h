// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <QWidget>

class QLabel;
class QPushButton;

// The Gravity Rush 2 profile of player 1: a round picture above the player name. A picture picked
// here is saved as <home>/<user id>/gr2_avatar.png, which the emulator sends to the server.
class ProfileWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfileWidget(QWidget* parent = nullptr);

    /// Shows the name and the picture of the user on controller port 1.
    void Refresh();

private:
    void OnAvatarClicked();

    QPushButton* m_avatar_button;
    QLabel* m_name_label;
    std::filesystem::path m_avatar_path;
};
