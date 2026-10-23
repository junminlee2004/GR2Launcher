// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>
#include <QCheckBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "core/gr2_mods.h"
#include "mods_page.h"

ModsPage::ModsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    m_gr2Group = new QGroupBox(tr("Gravity Rush 2"), this);
    auto* gr2Layout = new QVBoxLayout(m_gr2Group);
    m_fsr3CheckBox = new QCheckBox(tr("Enable FSR 3.1"), m_gr2Group);
    m_fsr3CheckBox->installEventFilter(this);
    gr2Layout->addWidget(m_fsr3CheckBox);
    layout->addWidget(m_gr2Group);

    m_grrGroup = new QGroupBox(tr("Gravity Rush Remastered"), this);
    auto* grrLayout = new QVBoxLayout(m_grrGroup);
    grrLayout->addWidget(new QLabel(tr("No mods yet."), m_grrGroup));
    layout->addWidget(m_grrGroup);

    layout->addStretch();
}

bool ModsPage::HasSection(const std::string& serial) {
    return serial.empty() || Gr2Mods::IsGravityRush2(serial) ||
           Gr2Mods::IsGravityRushRemastered(serial);
}

void ModsPage::Load(const std::string& serial) {
    const bool global = serial.empty();
    m_gr2Group->setVisible(global || Gr2Mods::IsGravityRush2(serial));
    m_grrGroup->setVisible(global || Gr2Mods::IsGravityRushRemastered(serial));

    std::optional<bool> fsr3 = global ? std::nullopt : Gr2Mods::ReadFsr3Enable(serial);
    if (!fsr3) {
        fsr3 = Gr2Mods::ReadFsr3Enable();
    }
    m_fsr3CheckBox->setChecked(fsr3.value_or(false));
}

void ModsPage::Save(const std::string& serial) {
    const bool fsr3 = m_fsr3CheckBox->isChecked();
    if (serial.empty()) {
        Gr2Mods::WriteFsr3Enable(fsr3);
        return;
    }
    if (Gr2Mods::IsGravityRush2(serial)) {
        const bool global = Gr2Mods::ReadFsr3Enable().value_or(false);
        Gr2Mods::WriteFsr3Enable(fsr3 == global ? std::nullopt : std::optional{fsr3}, serial);
    }
}

void ModsPage::SetDefaults() {
    m_fsr3CheckBox->setChecked(false);
}

bool ModsPage::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_fsr3CheckBox && event->type() == QEvent::Enter) {
        emit DescriptionChanged(
            m_fsr3CheckBox->text() + ":\n" +
            tr("Replaces the game's anti-aliasing with AMD FSR 3.1. Needs a GR2fork build with "
               "FSR 3.1. For upscaling, also turn on one of the patches under Resolution > FSR "
               "upscaling in Cheats / Patches; without one, FSR 3.1 runs at the game's resolution "
               "as anti-aliasing.\nA change takes effect the next time the game starts. The other "
               "FSR 3.1 settings are in the debug menu: Ctrl+F10 > Display > FSR 3.1 (GR2). Its "
               "Save button saves the values the game runs with, this switch included.") +
            "\n\nGR2Fork.fsr3Enable");
    } else if (obj == m_fsr3CheckBox && event->type() == QEvent::Leave) {
        emit DescriptionChanged({});
    }
    return QWidget::eventFilter(obj, event);
}
