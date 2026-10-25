// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>
#include <QCheckBox>
#include <QMessageBox>
#include <QStringList>

#include "core/gr2_mods.h"
#include "core/prompt_packs.h"
#include "gui_settings.h"
#include "input/input.h"
#include "prompt_pack_offer.h"

namespace {

// Entries "GR2:xbox", "GRR:switch" and so on: the games and pad types the player answered "no,
// don't ask again" for.
constexpr const char* DismissedName = "promptPackDismissed";

} // Anonymous namespace

void PromptPackOffer::Run(QWidget* parent, const std::string& serial, gui_settings& settings) {
    const bool gr2 = Gr2Mods::IsGravityRush2(serial);
    if (!gr2 && !Gr2Mods::IsGravityRushRemastered(serial)) {
        return;
    }
    const std::string pad = PromptPacks::ConnectedPadLayout(GamepadSelect::GetSelectedGamepad());
    if (pad.empty()) {
        return;
    }
    const auto read = gr2 ? Gr2Mods::ReadGr2PromptLayout : Gr2Mods::ReadGrrPromptLayout;
    const auto write = gr2 ? Gr2Mods::WriteGr2PromptLayout : Gr2Mods::WriteGrrPromptLayout;
    const std::optional<std::string> perGame = read(serial);
    if (perGame.value_or(read("").value_or("off")) == pad) {
        return;
    }
    const QString dismissKey = QString(gr2 ? "GR2:" : "GRR:") + QString::fromStdString(pad);
    QStringList dismissed =
        settings.GetValue(gui::general_settings, DismissedName, QStringList()).toStringList();
    if (dismissed.contains(dismissKey)) {
        return;
    }

    const bool xbox = pad == "xbox";
    const QString layoutName = xbox ? tr("Xbox") : tr("Nintendo Switch Pro");
    QString details = tr("Menus, tutorials, the controls page and button icons in text then show "
                         "%1 buttons. The game's controls stay on the same buttons.")
                          .arg(layoutName);
    if (gr2) {
        details += " " + tr("Touchpad swipes are done by holding L3 and pressing a face button.");
    }
    if (xbox) {
        details += " " + tr("Motion controls are off with this layout.");
    }
    details += "\n\n" + tr("This can be changed at any time in Settings > Mods.");

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Button prompts"));
    box.setText((xbox ? tr("An Xbox controller is connected. Show Xbox button prompts in %1?")
                      : tr("A Nintendo Switch controller is connected. Show Nintendo Switch Pro "
                           "button prompts in %1?"))
                    .arg(gr2 ? QStringLiteral("Gravity Rush 2")
                             : QStringLiteral("Gravity Rush Remastered")));
    box.setInformativeText(details);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    auto* dontAsk = new QCheckBox(tr("Don't ask again for this controller type"), &box);
    box.setCheckBox(dontAsk);

    if (box.exec() == QMessageBox::Yes) {
        write(pad, "");
        // A per-game value would hide the one of config.json.
        if (perGame) {
            write(std::nullopt, serial);
        }
    } else if (dontAsk->isChecked()) {
        dismissed.append(dismissKey);
        settings.SetValue(gui::general_settings, DismissedName, dismissed);
    }
}
