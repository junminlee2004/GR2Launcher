// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <optional>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "common/path_util.h"
#include "core/gr2_mods.h"
#ifdef _WIN32
#include "dlss_download.h"
#endif
#include "mods_page.h"

namespace {

// A row with a label and a combo box of the button prompt layouts, the values of gr2PromptLayout
// and grrPromptLayout.
QComboBox* AddPromptRow(QWidget* group, QVBoxLayout* layout, QObject* filter) {
    auto* row = new QWidget(group);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(new QLabel(ModsPage::tr("Button prompts:"), row));
    auto* combo = new QComboBox(row);
    combo->addItem(ModsPage::tr("PlayStation"), QStringLiteral("off"));
    combo->addItem(ModsPage::tr("Xbox"), QStringLiteral("xbox"));
    combo->addItem(ModsPage::tr("Nintendo Switch Pro"), QStringLiteral("switch"));
    combo->addItem(ModsPage::tr("Steam Deck"), QStringLiteral("steamdeck"));
    combo->addItem(ModsPage::tr("Steam Controller"), QStringLiteral("steamcontroller"));
    combo->installEventFilter(filter);
    rowLayout->addWidget(combo);
    rowLayout->addStretch();
    layout->addWidget(row);
    return combo;
}

void SelectLayout(QComboBox* combo, const std::string& layout) {
    combo->setCurrentIndex(std::max(combo->findData(QString::fromStdString(layout)), 0));
}

std::string SelectedLayout(const QComboBox* combo) {
    return combo->currentData().toString().toStdString();
}

} // Anonymous namespace

ModsPage::ModsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    m_gr2Group = new QGroupBox(tr("Gravity Rush 2"), this);
    auto* gr2Layout = new QVBoxLayout(m_gr2Group);
    m_fsr3CheckBox = new QCheckBox(tr("Enable FSR 3.1"), m_gr2Group);
    m_fsr3CheckBox->installEventFilter(this);
    gr2Layout->addWidget(m_fsr3CheckBox);
    m_fsr4CheckBox = new QCheckBox(tr("Enable FSR 4 (INT8, experimental)"), m_gr2Group);
    m_fsr4CheckBox->installEventFilter(this);
    gr2Layout->addWidget(m_fsr4CheckBox);

    auto* fsr4DirRow = new QWidget(m_gr2Group);
    fsr4DirRow->setEnabled(false);
    fsr4DirRow->installEventFilter(this);
    auto* fsr4DirLayout = new QHBoxLayout(fsr4DirRow);
    // Lines the folder row up with the text of the FSR 4 check box.
    fsr4DirLayout->setContentsMargins(
        style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, m_fsr4CheckBox) +
            style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, m_fsr4CheckBox),
        0, 0, 0);
    fsr4DirLayout->addWidget(new QLabel(tr("FSR 4 asset folder:"), fsr4DirRow));
    m_fsr4DirEdit = new QLineEdit(fsr4DirRow);
    QString defaultDir;
    Common::FS::PathToQString(defaultDir, Gr2Mods::ResolveFsr4AssetDir(""));
    m_fsr4DirEdit->setPlaceholderText(defaultDir);
    fsr4DirLayout->addWidget(m_fsr4DirEdit, 1);
    auto* browseButton = new QPushButton(tr("Browse"), fsr4DirRow);
    connect(browseButton, &QPushButton::clicked, this, [this] {
        QString initial;
        Common::FS::PathToQString(
            initial, Gr2Mods::ResolveFsr4AssetDir(m_fsr4DirEdit->text().trimmed().toStdString()));
        const QString dir =
            QFileDialog::getExistingDirectory(this, tr("Select the FSR 4 asset folder"), initial);
        if (!dir.isEmpty()) {
            m_fsr4DirEdit->setText(dir);
        }
    });
    fsr4DirLayout->addWidget(browseButton);
    gr2Layout->addWidget(fsr4DirRow);
#ifdef _WIN32
    m_dlssCheckBox = new QCheckBox(tr("Enable DLSS (NVIDIA RTX)"), m_gr2Group);
    m_dlssCheckBox->installEventFilter(this);
    gr2Layout->addWidget(m_dlssCheckBox);
#endif
    m_gr2PromptCombo = AddPromptRow(m_gr2Group, gr2Layout, this);
    m_halloweenCheckBox = new QCheckBox(tr("Halloween photo items"), m_gr2Group);
    m_halloweenCheckBox->installEventFilter(this);
    gr2Layout->addWidget(m_halloweenCheckBox);
    layout->addWidget(m_gr2Group);

    m_grrGroup = new QGroupBox(tr("Gravity Rush Remastered"), this);
    auto* grrLayout = new QVBoxLayout(m_grrGroup);
    m_grrFsr3CheckBox = new QCheckBox(tr("Enable FSR 3.1"), m_grrGroup);
    m_grrFsr3CheckBox->installEventFilter(this);
    grrLayout->addWidget(m_grrFsr3CheckBox);
    m_grrFsr4CheckBox = new QCheckBox(tr("Enable FSR 4 (INT8, experimental)"), m_grrGroup);
    m_grrFsr4CheckBox->installEventFilter(this);
    grrLayout->addWidget(m_grrFsr4CheckBox);
#ifdef _WIN32
    m_grrDlssCheckBox = new QCheckBox(tr("Enable DLSS (NVIDIA RTX)"), m_grrGroup);
    m_grrDlssCheckBox->installEventFilter(this);
    grrLayout->addWidget(m_grrDlssCheckBox);
#endif
    m_grrPromptCombo = AddPromptRow(m_grrGroup, grrLayout, this);
    layout->addWidget(m_grrGroup);

    // Both games read the folder of config.json, so the FSR 4 box of Gravity Rush Remastered also
    // enables the row where the page shows it.
    const auto enableFsr4Dir = [this, fsr4DirRow] {
        fsr4DirRow->setEnabled(m_fsr4CheckBox->isChecked() ||
                               (!m_grrGroup->isHidden() && m_grrFsr4CheckBox->isChecked()));
    };
    connect(m_fsr4CheckBox, &QCheckBox::toggled, this, enableFsr4Dir);
    connect(m_grrFsr4CheckBox, &QCheckBox::toggled, this, enableFsr4Dir);

#ifdef _WIN32
    // Only a tick by the user emits clicked, so only that tick starts a download.
    for (QCheckBox* box : {m_dlssCheckBox, m_grrDlssCheckBox}) {
        connect(box, &QCheckBox::clicked, this, [this, box](bool checked) {
            if (checked && !DlssDownload::EnsureFiles(this)) {
                box->setChecked(false);
            }
        });
    }
#endif

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

    // The value of the per-game file, else the one of config.json.
    const auto read = [&](auto reader) {
        const auto value = global ? std::nullopt : reader(serial);
        return value ? value : reader("");
    };
    m_fsr3CheckBox->setChecked(read(Gr2Mods::ReadFsr3Enable).value_or(false));
    m_fsr4CheckBox->setChecked(read(Gr2Mods::ReadFsr4Enable).value_or(false));
    m_fsr4DirEdit->setText(QString::fromStdString(read(Gr2Mods::ReadFsr4AssetDir).value_or("")));
    m_grrFsr3CheckBox->setChecked(read(Gr2Mods::ReadGrrFsr3Enable).value_or(false));
    m_grrFsr4CheckBox->setChecked(read(Gr2Mods::ReadGrrFsr4Enable).value_or(false));
#ifdef _WIN32
    m_dlssCheckBox->setChecked(read(Gr2Mods::ReadDlssEnable).value_or(false));
    m_grrDlssCheckBox->setChecked(read(Gr2Mods::ReadGrrDlssEnable).value_or(false));
#endif
    SelectLayout(m_gr2PromptCombo, read(Gr2Mods::ReadGr2PromptLayout).value_or("off"));
    SelectLayout(m_grrPromptCombo, read(Gr2Mods::ReadGrrPromptLayout).value_or("off"));
    m_halloweenCheckBox->setChecked(read(Gr2Mods::ReadGr2HalloweenItems).value_or(true));
}

void ModsPage::Save(const std::string& serial) {
    const bool fsr3 = m_fsr3CheckBox->isChecked();
    const bool fsr4 = m_fsr4CheckBox->isChecked();
    const std::string fsr4Dir = m_fsr4DirEdit->text().trimmed().toStdString();
    const bool grrFsr3 = m_grrFsr3CheckBox->isChecked();
    const bool grrFsr4 = m_grrFsr4CheckBox->isChecked();
    const std::string gr2Prompts = SelectedLayout(m_gr2PromptCombo);
    const std::string grrPrompts = SelectedLayout(m_grrPromptCombo);
    const bool halloween = m_halloweenCheckBox->isChecked();
    if (serial.empty()) {
        Gr2Mods::WriteFsr3Enable(fsr3);
        Gr2Mods::WriteFsr4Enable(fsr4);
        Gr2Mods::WriteFsr4AssetDir(fsr4Dir.empty() ? std::nullopt : std::optional{fsr4Dir});
        Gr2Mods::WriteGrrFsr3Enable(grrFsr3);
        Gr2Mods::WriteGrrFsr4Enable(grrFsr4);
#ifdef _WIN32
        Gr2Mods::WriteDlssEnable(m_dlssCheckBox->isChecked());
        Gr2Mods::WriteGrrDlssEnable(m_grrDlssCheckBox->isChecked());
#endif
        // A missing key also selects the PlayStation buttons.
        Gr2Mods::WriteGr2PromptLayout(gr2Prompts == "off" ? std::nullopt
                                                          : std::optional{gr2Prompts});
        Gr2Mods::WriteGrrPromptLayout(grrPrompts == "off" ? std::nullopt
                                                          : std::optional{grrPrompts});
        // A missing key also turns the Halloween photo items on.
        Gr2Mods::WriteGr2HalloweenItems(halloween ? std::nullopt : std::optional{false});
        return;
    }
    // The per-game file keeps only the values that differ from the ones of config.json.
    const auto unlessGlobal = [](const auto& value, const auto& global) {
        return value == global ? std::nullopt : std::optional{value};
    };
    if (Gr2Mods::IsGravityRush2(serial)) {
        Gr2Mods::WriteFsr3Enable(unlessGlobal(fsr3, Gr2Mods::ReadFsr3Enable().value_or(false)),
                                 serial);
        Gr2Mods::WriteFsr4Enable(unlessGlobal(fsr4, Gr2Mods::ReadFsr4Enable().value_or(false)),
                                 serial);
        Gr2Mods::WriteFsr4AssetDir(unlessGlobal(fsr4Dir, Gr2Mods::ReadFsr4AssetDir().value_or("")),
                                   serial);
#ifdef _WIN32
        Gr2Mods::WriteDlssEnable(
            unlessGlobal(m_dlssCheckBox->isChecked(), Gr2Mods::ReadDlssEnable().value_or(false)),
            serial);
#endif
        Gr2Mods::WriteGr2PromptLayout(
            unlessGlobal(gr2Prompts, Gr2Mods::ReadGr2PromptLayout().value_or("off")), serial);
        Gr2Mods::WriteGr2HalloweenItems(
            unlessGlobal(halloween, Gr2Mods::ReadGr2HalloweenItems().value_or(true)), serial);
    } else if (Gr2Mods::IsGravityRushRemastered(serial)) {
        Gr2Mods::WriteGrrFsr3Enable(
            unlessGlobal(grrFsr3, Gr2Mods::ReadGrrFsr3Enable().value_or(false)), serial);
        Gr2Mods::WriteGrrFsr4Enable(
            unlessGlobal(grrFsr4, Gr2Mods::ReadGrrFsr4Enable().value_or(false)), serial);
#ifdef _WIN32
        Gr2Mods::WriteGrrDlssEnable(unlessGlobal(m_grrDlssCheckBox->isChecked(),
                                                 Gr2Mods::ReadGrrDlssEnable().value_or(false)),
                                    serial);
#endif
        Gr2Mods::WriteGrrPromptLayout(
            unlessGlobal(grrPrompts, Gr2Mods::ReadGrrPromptLayout().value_or("off")), serial);
    }
}

void ModsPage::SetDefaults() {
    m_fsr3CheckBox->setChecked(false);
    m_fsr4CheckBox->setChecked(false);
    m_fsr4DirEdit->clear();
    m_grrFsr3CheckBox->setChecked(false);
    m_grrFsr4CheckBox->setChecked(false);
#ifdef _WIN32
    m_dlssCheckBox->setChecked(false);
    m_grrDlssCheckBox->setChecked(false);
#endif
    m_gr2PromptCombo->setCurrentIndex(0);
    m_grrPromptCombo->setCurrentIndex(0);
    m_halloweenCheckBox->setChecked(true);
}

bool ModsPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Enter) {
        // The filter is installed only on the check boxes, the folder row and the combo boxes.
        if (obj == m_fsr3CheckBox) {
            emit DescriptionChanged(
                m_fsr3CheckBox->text() + ":\n" +
                tr("Replaces the game's anti-aliasing with AMD FSR 3.1. Needs a GR2fork build with "
                   "FSR 3.1. For upscaling, also turn on one of the patches under Resolution > FSR "
                   "upscaling in Cheats / Patches; without one, FSR 3.1 runs at the game's "
                   "resolution as anti-aliasing.\nA change takes effect the next time the game "
                   "starts. The other FSR 3.1 settings are in the debug menu: Ctrl+F10 > Display > "
                   "FSR 3.1 (GR2). Its Save button saves the values the game runs with, this "
                   "switch included.") +
                "\n\nGR2Fork.fsr3Enable");
        } else if (obj == m_fsr4CheckBox) {
            emit DescriptionChanged(
                m_fsr4CheckBox->text() + ":\n" +
                tr("Replaces the game's anti-aliasing with the experimental source-v07 FSR 4 INT8 "
                   "upscaler, which is not AMD FSR 4.1 and can also run on RDNA2, RDNA3 and other "
                   "GPUs. Its shader and model files are not included with GR2fork or this "
                   "launcher; without them or driver support, FSR 3.1 runs if it is on. For "
                   "upscaling, also turn on a patch under Resolution > Upscaling in Cheats / "
                   "Patches.\nA change takes effect the next time the game starts. FSR 4 model and "
                   "status: Ctrl+F10 > Display > FSR 3.1 (GR2).") +
                "\n\nGR2Fork.fsr4Enable");
        } else if (obj == m_grrFsr3CheckBox) {
            emit DescriptionChanged(
                m_grrFsr3CheckBox->text() + ":\n" +
                tr("Replaces the game's FXAA anti-aliasing with AMD FSR 3.1. Needs a GR2fork "
                   "build with the FSR upscaler for Gravity Rush Remastered. For upscaling, also "
                   "turn on a patch under Resolution > Upscaling in Cheats / Patches; without "
                   "one, FSR 3.1 runs at the game's resolution as anti-aliasing. Menus and comic "
                   "scenes keep the game's FXAA.\nA change takes effect the next time the game "
                   "starts. The other FSR 3.1 settings are in the debug menu: Ctrl+F10 > Display "
                   "> FSR 3.1 (GRR). Its Save button saves the values the game runs with, this "
                   "switch included.") +
                "\n\nGR2Fork.grrFsr3Enable");
        } else if (obj == m_grrFsr4CheckBox) {
            emit DescriptionChanged(
                m_grrFsr4CheckBox->text() + ":\n" +
                tr("Replaces the game's FXAA anti-aliasing with the experimental source-v07 FSR 4 "
                   "INT8 upscaler, which is not AMD FSR 4.1 and can also run on RDNA2, RDNA3 and "
                   "other GPUs. Needs a GR2fork build with the FSR upscaler for Gravity Rush "
                   "Remastered. For upscaling, also turn on a patch under Resolution > FSR "
                   "upscaling in Cheats / Patches. Its shader and model files are not "
                   "included with GR2fork or this launcher; it reads them from the same FSR 4 "
                   "asset folder as Gravity Rush 2, set in the global settings. Without them or "
                   "driver support, FSR 3.1 runs if it is on.\nA change takes effect the next "
                   "time the game starts. FSR 4 model and status: Ctrl+F10 > Display > FSR 3.1 "
                   "(GRR).") +
                "\n\nGR2Fork.grrFsr4Enable");
#ifdef _WIN32
        } else if (obj == m_dlssCheckBox || obj == m_grrDlssCheckBox) {
            const bool gr2 = obj == m_dlssCheckBox;
            emit DescriptionChanged(
                static_cast<QCheckBox*>(obj)->text() + ":\n" +
                tr("Replaces the game's anti-aliasing with NVIDIA DLSS Super Resolution, or DLAA "
                   "at the game's resolution. Needs an NVIDIA GeForce RTX GPU and four files of "
                   "NVIDIA's Streamline 2.14.16 release in the dlss folder of the user folder: "
                   "sl.interposer.dll, sl.common.dll, sl.dlss.dll and nvngx_dlss.dll. When one is "
                   "missing, ticking the box offers to download the release (263 MiB) from "
                   "NVIDIA's GitHub page and keeps only these files. When DLSS cannot run, FSR 4 "
                   "or FSR 3.1 runs if it is on. For upscaling, also turn on a "
                   "patch under Resolution > Upscaling in Cheats / Patches.\nA change takes "
                   "effect the next time the game starts. Preset and status: Ctrl+F10 > %1.")
                    .arg(gr2 ? QStringLiteral("DLSS (GR2)") : QStringLiteral("DLSS (GRR)")) +
                (gr2 ? "\n\nGR2Fork.dlssEnable" : "\n\nGR2Fork.grrDlssEnable"));
#endif
        } else if (obj == m_halloweenCheckBox) {
            emit DescriptionChanged(
                m_halloweenCheckBox->text() + ":\n" +
                tr("Adds two photo items to Photo Mode: a Small Jack-o'-Lantern, unlocked at 1,000 "
                   "Dusty Tokens, and a Large Jack-o'-Lantern, unlocked at 8,000. The unlock comes "
                   "quietly in free roam, also when the tokens were earned before, with a gift "
                   "announcement for each lantern, and the game saves right after it. The Dusty "
                   "Token checklist in Statistics lists both items. The unlocks are kept in two "
                   "photo item flags of the save that the game leaves unused, and the game files "
                   "stay as they are. "
                   "Works with every button prompt layout. Needs game version 1.11. The mod pack "
                   "ships with this launcher and needs a GR2fork build that reads it.\nA change "
                   "takes effect the next time the game starts.") +
                "\n\nGR2Fork.gr2HalloweenItems");
        } else if (obj == m_gr2PromptCombo || obj == m_grrPromptCombo) {
            const bool gr2 = obj == m_gr2PromptCombo;
            QString text =
                tr("Shows the buttons of an Xbox pad, a Nintendo Switch Pro controller, a Steam "
                   "Deck or a Steam Controller instead of PlayStation buttons in the game's menus, "
                   "tutorials, controls page and the button icons in its text. The game's controls "
                   "stay on the same buttons and "
                   "the game files stay as they are. The prompt packs ship with this launcher and "
                   "need a GR2fork build that reads them.");
            if (gr2) {
                text += " " + tr("With any of these prompts, touchpad swipes are done by "
                                 "holding L3 and pressing triangle (up), cross (down), square "
                                 "(left) or circle (right) in their place.");
            }
            text += "\n" + tr("A change takes effect the next time the game starts.");
            emit DescriptionChanged(
                tr("Button prompts:") + "\n" + text +
                (gr2 ? "\n\nGR2Fork.gr2PromptLayout" : "\n\nGR2Fork.grrPromptLayout"));
        } else {
            emit DescriptionChanged(
                tr("FSR 4 asset folder:") + "\n" +
                tr("The folder that directly holds the shader and model files of the experimental "
                   "source-v07 FSR 4 INT8 upscaler, which is not AMD FSR 4.1: "
                   "fsr4_model_v07_i8_*.spv and fsr4_model_v07_i8_*.bin. Empty selects the "
                   "fsr4_v07 folder in the emulator's user folder, which this launcher fills with "
                   "the files it ships and replaces when they change; a relative path starts from "
                   "that user folder.\n"
                   "A change takes effect the next time the game starts.") +
                "\n\nGR2Fork.fsr4AssetDir");
        }
    } else if (event->type() == QEvent::Leave) {
        emit DescriptionChanged({});
    }
    return QWidget::eventFilter(obj, event);
}
