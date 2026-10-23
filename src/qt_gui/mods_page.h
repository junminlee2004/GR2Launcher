// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <QWidget>

class QCheckBox;
class QGroupBox;

// The mod switches of the GR2fork emulator, one section per game. The values are keys of the
// "GR2Fork" section of config.json or of a per-game file, read and written by Gr2Mods.
class ModsPage : public QWidget {
    Q_OBJECT
public:
    explicit ModsPage(QWidget* parent = nullptr);

    /// Whether the page has a section for the game of serial. An empty serial, the global
    /// settings, has all of them.
    static bool HasSection(const std::string& serial);

    /// Shows the sections of serial and its values: those its per-game file sets, else those of
    /// config.json.
    void Load(const std::string& serial);
    /// Writes the shown values: into config.json for an empty serial, else into the per-game file,
    /// which drops a value equal to the one of config.json.
    void Save(const std::string& serial);
    /// Shows the default values.
    void SetDefaults();

signals:
    /// The description of the switch under the pointer, or an empty text when it leaves one.
    void DescriptionChanged(const QString& text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QGroupBox* m_gr2Group{};
    QGroupBox* m_grrGroup{};
    QCheckBox* m_fsr3CheckBox{};
};
