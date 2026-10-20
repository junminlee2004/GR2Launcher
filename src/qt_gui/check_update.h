// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CHECKUPDATE_H
#define CHECKUPDATE_H

#include <functional>
#include <QCheckBox>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QPushButton>
#include "gui_settings.h"

class CheckUpdate : public QDialog {
    Q_OBJECT

public:
    explicit CheckUpdate(std::shared_ptr<gui_settings> gui_settings, const bool showMessage,
                         QWidget* parent = nullptr);
    ~CheckUpdate();

private slots:
    void CheckForUpdates(const bool showMessage);
    void DownloadUpdate(const QString& url);
    void Install();

private:
    void CheckEmulatorUpdate(const std::function<void()>& done);
    void setupUI(bool launcherUpdate, const QString& downloadUrl, const QString& latestDate,
                 const QString& latestRev, const QString& currentDate, const QString& currentRev);

    // Lists the commits between two revisions of the launcher or, if emulator is true, of the
    // emulator in the changelog.
    void requestChangelog(bool emulator, const QString& currentRev, const QString& latestRev);
    void showChangelog();

    QCheckBox* autoUpdateCheckBox;
    QCheckBox* updateEmulatorCheckBox;
    QPushButton* yesButton;
    QPushButton* noButton;
    QString updateDownloadUrl;
    QString emulatorTag;
    QString emulatorLocalHash;
    QString emulatorLatestHash;
    QString launcherChanges;
    QString emulatorChanges;

    QNetworkAccessManager* networkManager;
    std::shared_ptr<gui_settings> m_gui_settings;
};

#endif // CHECKUPDATE_H
