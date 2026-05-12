// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "common/emulator_path.h"
#include "common/path_util.h"
#include "create_shortcut.h"

ShortcutDialog::ShortcutDialog(std::shared_ptr<gui_settings> settings, QWidget* parent)
    : QDialog(parent), m_gui_settings(std::move(settings)) {
    setupUI();
    resize(600, 50);

    this->setWindowTitle(tr("Select Version"));
}

void ShortcutDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* versionLabel =
        new QLabel(QString("<b>%1</b>").arg(tr("Select version for shortcut creation")));

    // Version manager removed — there is only one emulator: the one auto-
    // detected next to the launcher (filename begins with "shadPS4gr2fork").
    // The list widget is kept for layout familiarity but always contains the
    // single detected entry (or a placeholder when not found).
    listWidget = new QListWidget(this);
    const auto exe = EmulatorPath::FindGr2forkExecutable();
    if (!exe.empty()) {
        QString shown = QString::fromStdWString(exe.filename().wstring());
        new QListWidgetItem(shown, listWidget);
        listWidget->setCurrentRow(0);
    } else {
        new QListWidgetItem(tr("(no emulator found next to GR2launcher)"), listWidget);
    }

    QDialogButtonBox* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    mainLayout->addWidget(versionLabel);
    mainLayout->addWidget(listWidget);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QWidget::close);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ShortcutDialog::createShortcut);
}

void ShortcutDialog::createShortcut() {
    const auto exe = EmulatorPath::FindGr2forkExecutable();
    if (exe.empty()) {
        QMessageBox::critical(
            this, "GR2launcher",
            tr("exe not found: no file beginning with \"%1\" was found in the launcher's "
               "directory.")
                .arg(EmulatorPath::kGr2forkExecutablePrefix));
        return;
    }
    QString executablePath;
    Common::FS::PathToQString(executablePath, exe);
    emit shortcutRequested(executablePath);
    QWidget::close();
}

ShortcutDialog::~ShortcutDialog() {}
