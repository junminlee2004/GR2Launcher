// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>
#include <QCheckBox>
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
#include "mods_page.h"

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
    layout->addWidget(m_gr2Group);

    m_grrGroup = new QGroupBox(tr("Gravity Rush Remastered"), this);
    auto* grrLayout = new QVBoxLayout(m_grrGroup);
    m_grrFsr3CheckBox = new QCheckBox(tr("Enable FSR 3.1"), m_grrGroup);
    m_grrFsr3CheckBox->installEventFilter(this);
    grrLayout->addWidget(m_grrFsr3CheckBox);
    m_grrFsr4CheckBox = new QCheckBox(tr("Enable FSR 4 (INT8, experimental)"), m_grrGroup);
    m_grrFsr4CheckBox->installEventFilter(this);
    grrLayout->addWidget(m_grrFsr4CheckBox);
    layout->addWidget(m_grrGroup);

    // Both games read the folder of config.json, so the FSR 4 box of Gravity Rush Remastered also
    // enables the row where the page shows it.
    const auto enableFsr4Dir = [this, fsr4DirRow] {
        fsr4DirRow->setEnabled(m_fsr4CheckBox->isChecked() ||
                               (!m_grrGroup->isHidden() && m_grrFsr4CheckBox->isChecked()));
    };
    connect(m_fsr4CheckBox, &QCheckBox::toggled, this, enableFsr4Dir);
    connect(m_grrFsr4CheckBox, &QCheckBox::toggled, this, enableFsr4Dir);

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
}

void ModsPage::Save(const std::string& serial) {
    const bool fsr3 = m_fsr3CheckBox->isChecked();
    const bool fsr4 = m_fsr4CheckBox->isChecked();
    const std::string fsr4Dir = m_fsr4DirEdit->text().trimmed().toStdString();
    const bool grrFsr3 = m_grrFsr3CheckBox->isChecked();
    const bool grrFsr4 = m_grrFsr4CheckBox->isChecked();
    if (serial.empty()) {
        Gr2Mods::WriteFsr3Enable(fsr3);
        Gr2Mods::WriteFsr4Enable(fsr4);
        Gr2Mods::WriteFsr4AssetDir(fsr4Dir.empty() ? std::nullopt : std::optional{fsr4Dir});
        Gr2Mods::WriteGrrFsr3Enable(grrFsr3);
        Gr2Mods::WriteGrrFsr4Enable(grrFsr4);
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
    } else if (Gr2Mods::IsGravityRushRemastered(serial)) {
        Gr2Mods::WriteGrrFsr3Enable(
            unlessGlobal(grrFsr3, Gr2Mods::ReadGrrFsr3Enable().value_or(false)), serial);
        Gr2Mods::WriteGrrFsr4Enable(
            unlessGlobal(grrFsr4, Gr2Mods::ReadGrrFsr4Enable().value_or(false)), serial);
    }
}

void ModsPage::SetDefaults() {
    m_fsr3CheckBox->setChecked(false);
    m_fsr4CheckBox->setChecked(false);
    m_fsr4DirEdit->clear();
    m_grrFsr3CheckBox->setChecked(false);
    m_grrFsr4CheckBox->setChecked(false);
}

bool ModsPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Enter) {
        // The filter is installed only on the check boxes and the folder row.
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
                   "upscaling, also turn on a patch under Resolution > FSR upscaling in Cheats / "
                   "Patches.\nA change takes effect the next time the game starts. FSR 4 model and "
                   "status: Ctrl+F10 > Display > FSR 3.1 (GR2).") +
                "\n\nGR2Fork.fsr4Enable");
        } else if (obj == m_grrFsr3CheckBox) {
            emit DescriptionChanged(
                m_grrFsr3CheckBox->text() + ":\n" +
                tr("Replaces the game's FXAA anti-aliasing with AMD FSR 3.1. Needs a GR2fork "
                   "build with the FSR upscaler for Gravity Rush Remastered. For upscaling, also "
                   "turn on a patch under Resolution > FSR upscaling in Cheats / Patches; without "
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
        } else {
            emit DescriptionChanged(
                tr("FSR 4 asset folder:") + "\n" +
                tr("The folder that directly holds the shader and model files of the experimental "
                   "source-v07 FSR 4 INT8 upscaler, which is not AMD FSR 4.1: "
                   "fsr4_model_v07_i8_*.spv and fsr4_model_v07_i8_*.bin. These files are not "
                   "included with GR2fork or this launcher. Empty selects the fsr4_v07 folder in "
                   "the emulator's user folder; a relative path starts from that user folder.\n"
                   "A change takes effect the next time the game starts.") +
                "\n\nGR2Fork.fsr4AssetDir");
        }
    } else if (event->type() == QEvent::Leave) {
        emit DescriptionChanged({});
    }
    return QWidget::eventFilter(obj, event);
}
