// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QTextBrowser>
#include <QTreeWidget>

#include "gui_settings.h"

// Every GR2fork-EXODUS release has a tag and a name starting with RELEASE_PREFIX. LATEST_LABEL
// is the download row and the installed entry that track the newest of them.
constexpr auto EMULATOR_REPO = "junminlee2004/GR2fork";
constexpr auto RELEASE_PREFIX = "Pre-release-GR2fork-EXODUS-";
constexpr auto LATEST_LABEL = "Pre-release (GR2fork-EXODUS)";

namespace Ui {
class VersionDialog;
}

class VersionDialog : public QDialog {
    Q_OBJECT
signals:
    void WindowResized(QResizeEvent* event);
    void PreReleaseInstalled();
    void PreReleaseInstallFailed();

public:
    explicit VersionDialog(std::shared_ptr<gui_settings> gui_settings, QWidget* parent = nullptr);
    ~VersionDialog();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void checkUpdatePre(const bool showMessage);
    void InstallLatestPreRelease();
    void installPreReleaseByTag(const QString& tagName);
    // Commit hash of the installed pre-release entry, empty when there is none.
    static QString InstalledPreReleaseHash();
    void DownloadListVersion();
    void InstallSelectedVersion();
    void addExecutableFromDrop(const QString& exePath);

private Q_SLOTS:
    void HandleResize(QResizeEvent* event);

private:
    Ui::VersionDialog* ui;
    std::shared_ptr<gui_settings> m_gui_settings;
    QNetworkAccessManager* networkManager;
    QString m_pendingExecutablePath;

    void LoadInstalledList();
    QStringList LoadDownloadCache();
    void SaveDownloadCache(const QStringList& versions);
    void PopulateDownloadTree(const QStringList& versions);
    void showPreReleaseUpdateDialog(const QString& localHash, const QString& latestHash,
                                    const QString& latestTag);
    void requestChangelog(const QString& localHash, const QString& latestHash,
                          const QString& latestTag, QTextBrowser* outputView);
    void showDownloadDialog(const QString& tagName, const QString& downloadUrl);
    void AddCustomExecutable(const QString& exePath);
    void ShowOutsideLauncherDir();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
