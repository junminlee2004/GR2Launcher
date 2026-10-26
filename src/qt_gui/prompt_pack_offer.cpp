// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>
#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
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

QString LayoutName(const std::string& layout) {
    if (layout == "xbox") {
        return PromptPackOffer::tr("Xbox");
    }
    if (layout == "switch") {
        return PromptPackOffer::tr("Nintendo Switch Pro");
    }
    if (layout == "steamdeck") {
        return PromptPackOffer::tr("Steam Deck");
    }
    return PromptPackOffer::tr("Steam Controller");
}

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
    // An Xbox pad can be Steam Input's virtual gamepad for a Steam Deck or a Steam Controller
    // whose own IDs SDL does not get, so the Steam layouts are offered with it.
    const std::vector<std::string> choices =
        pad == "xbox" ? std::vector<std::string>{"xbox", "steamdeck", "steamcontroller"}
                      : std::vector<std::string>{pad};
    const auto read = gr2 ? Gr2Mods::ReadGr2PromptLayout : Gr2Mods::ReadGrrPromptLayout;
    const auto write = gr2 ? Gr2Mods::WriteGr2PromptLayout : Gr2Mods::WriteGrrPromptLayout;
    const std::optional<std::string> perGame = read(serial);
    const std::string current = perGame.value_or(read("").value_or("off"));
    if (std::find(choices.begin(), choices.end(), current) != choices.end()) {
        return;
    }
    const QString dismissKey = QString(gr2 ? "GR2:" : "GRR:") + QString::fromStdString(pad);
    QStringList dismissed =
        settings.GetValue(gui::general_settings, DismissedName, QStringList()).toStringList();
    if (dismissed.contains(dismissKey)) {
        return;
    }

    const bool xbox = pad == "xbox";
    QString question;
    QString details;
    if (xbox) {
        question = tr("An Xbox controller is connected. Which button prompts should %1 show?");
        details = tr("Menus, tutorials, the controls page and button icons in text then show the "
                     "buttons of the chosen controller. A Steam Deck or a Steam Controller used "
                     "through Steam Input can show up as an Xbox controller.");
    } else {
        if (pad == "switch") {
            question = tr("A Nintendo Switch controller is connected. Show Nintendo Switch Pro "
                          "button prompts in %1?");
        } else if (pad == "steamdeck") {
            question = tr("Steam Deck controls are in use. Show Steam Deck button prompts in %1?");
        } else {
            question =
                tr("A Steam Controller is connected. Show Steam Controller button prompts in %1?");
        }
        details = tr("Menus, tutorials, the controls page and button icons in text then show %1 "
                     "buttons.")
                      .arg(LayoutName(pad));
    }
    details += " " + tr("The game's controls stay on the same buttons.");
    if (gr2) {
        details += " " + tr("Touchpad swipes are done by holding L3 and pressing a face button.");
    }
    details += "\n\n" + tr("This can be changed at any time in Settings > Mods.");

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Button prompts"));
    box.setText(question.arg(gr2 ? QStringLiteral("Gravity Rush 2")
                                 : QStringLiteral("Gravity Rush Remastered")));
    box.setInformativeText(details);
    std::vector<std::pair<QPushButton*, std::string>> buttons;
    if (xbox) {
        for (const std::string& layout : choices) {
            buttons.emplace_back(box.addButton(LayoutName(layout), QMessageBox::YesRole), layout);
        }
    } else {
        buttons.emplace_back(box.addButton(QMessageBox::Yes), pad);
    }
    box.setEscapeButton(box.addButton(QMessageBox::No));
    box.setDefaultButton(buttons.front().first);
    auto* dontAsk = new QCheckBox(tr("Don't ask again for this controller type"), &box);
    box.setCheckBox(dontAsk);

    box.exec();
    const auto chosen = std::find_if(buttons.begin(), buttons.end(), [&box](const auto& button) {
        return button.first == box.clickedButton();
    });
    if (chosen != buttons.end()) {
        write(chosen->second, "");
        // A per-game value would hide the one of config.json.
        if (perGame) {
            write(std::nullopt, serial);
        }
    } else if (dontAsk->isChecked()) {
        dismissed.append(dismissKey);
        settings.SetValue(gui::general_settings, DismissedName, dismissed);
    }
}
