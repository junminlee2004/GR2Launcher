// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "check_update.h"
#include "common/path_util.h"
#include "common/scm_rev.h"
#include "version_dialog.h"

using namespace Common::FS;

CheckUpdate::CheckUpdate(std::shared_ptr<gui_settings> gui_settings, const bool showMessage,
                         QWidget* parent)
    : QDialog(parent), m_gui_settings(std::move(gui_settings)),
      networkManager(new QNetworkAccessManager(this)) {
    setWindowTitle(tr("Auto Updater - GUI"));
    setFixedSize(0, 0);
    CheckForUpdates(showMessage);
}

CheckUpdate::~CheckUpdate() {}

void CheckUpdate::CheckForUpdates(const bool showMessage) {
    QUrl url;

    bool checkName = true;
    while (checkName) {
        url = QUrl("https://api.github.com/repos/junminlee2004/GR2Launcher/releases");
        checkName = false;
    }

    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, showMessage]() {
        if (reply->error() != QNetworkReply::NoError) {
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 403) {
                QString response = reply->readAll();
                if (response.startsWith("{\"message\":\"API rate limit exceeded for")) {
                    QMessageBox::warning(
                        this, tr("Auto Updater"),
                        // clang-format off
tr("The Auto Updater allows up to 60 update checks per hour.\\nYou have reached this limit. Please try again later.").replace("\\n", "\n"));
                    // clang-format on
                } else {
                    QMessageBox::warning(
                        this, tr("Error"),
                        QString(tr("Network error:") + "\n" + reply->errorString()));
                }
            } else {
                QMessageBox::warning(this, tr("Error"),
                                     QString(tr("Network error:") + "\n" + reply->errorString()));
            }
            reply->deleteLater();
            return;
        }

        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc(QJsonDocument::fromJson(response));

        if (jsonDoc.isNull()) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to parse update information."));
            reply->deleteLater();
            return;
        }

        QString downloadUrl;
        QString latestVersion;
        QString latestRev;
        QString latestDate;
        QString platformString;

#ifdef Q_OS_WIN
        platformString = "win64-qt";
#elif defined(Q_OS_LINUX)
        platformString = "linux-qt";
#elif defined(Q_OS_MAC)
        platformString = "macos-qt";
#endif

        QJsonObject jsonObj;
        QJsonArray jsonArray = jsonDoc.array();
        for (const QJsonValue& value : jsonArray) {
            jsonObj = value.toObject();
            if (jsonObj.contains("prerelease") && jsonObj["prerelease"].toBool()) {
                break;
            }
        }
        if (!jsonObj.isEmpty()) {
            latestVersion = jsonObj["tag_name"].toString();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("No pre-releases found."));
            reply->deleteLater();
            return;
        }

        latestRev = latestVersion.right(40);
        latestDate = jsonObj["published_at"].toString();

        QJsonArray assets = jsonObj["assets"].toArray();
        bool found = false;

        for (const QJsonValue& assetValue : assets) {
            QJsonObject assetObj = assetValue.toObject();
            if (assetObj["name"].toString().contains(platformString)) {
                downloadUrl = assetObj["browser_download_url"].toString();
                found = true;
                break;
            }
        }

        if (!found) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("No download URL found for the specified asset."));
            reply->deleteLater();
            return;
        }

        QString currentRev = QString::fromStdString(Common::g_scm_rev);
        QString currentDate = Common::g_scm_date;

        QDateTime dateTime = QDateTime::fromString(latestDate, Qt::ISODate);
        latestDate = dateTime.isValid() ? dateTime.toString("yyyy-MM-dd HH:mm:ss") : "Unknown date";

        const bool launcherUpdate = latestRev != currentRev;
        const auto finish = [this, showMessage, launcherUpdate, downloadUrl, latestDate, latestRev,
                             currentDate, currentRev]() {
            if (!launcherUpdate && emulatorTag.isEmpty()) {
                if (showMessage) {
                    QMessageBox::information(this, tr("Auto Updater"),
                                             tr("Your version is already up to date!"));
                }
                close();
                return;
            }
            setupUI(launcherUpdate, downloadUrl, latestDate, latestRev, currentDate, currentRev);
        };
        if (m_gui_settings->GetValue(gui::gen_updateEmulator).toBool()) {
            CheckEmulatorUpdate(finish);
        } else {
            finish();
        }
        reply->deleteLater();
    });
}

// Sets emulatorTag when a GR2fork-EXODUS pre-release newer than the installed one exists.
void CheckUpdate::CheckEmulatorUpdate(const std::function<void()>& done) {
    const QString versionPath = m_gui_settings->GetValue(gui::vm_versionPath).toString();
    emulatorLocalHash = VersionDialog::InstalledPreReleaseHash();
    if (versionPath.isEmpty() || !QDir(versionPath).exists() || emulatorLocalHash.isEmpty()) {
        done();
        return;
    }

    QNetworkRequest request(QString("https://api.github.com/repos/%1/releases").arg(EMULATOR_REPO));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, done]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            for (const QJsonValue& value : QJsonDocument::fromJson(reply->readAll()).array()) {
                const QString tag = value.toObject()["tag_name"].toString();
                if (tag.startsWith(RELEASE_PREFIX)) {
                    const QString hash = tag.mid(tag.lastIndexOf('-') + 1);
                    if (hash != emulatorLocalHash) {
                        emulatorTag = tag;
                        emulatorLatestHash = hash;
                    }
                    break;
                }
            }
        }
        done();
    });
}

void CheckUpdate::setupUI(bool launcherUpdate, const QString& downloadUrl,
                          const QString& latestDate, const QString& latestRev,
                          const QString& currentDate, const QString& currentRev) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* titleLayout = new QHBoxLayout();

    QLabel* imageLabel = new QLabel(this);
    QPixmap pixmap(":/images/shadps4.png");
    imageLabel->setPixmap(pixmap);
    imageLabel->setScaledContents(true);
    imageLabel->setFixedSize(50, 50);

    QString title = tr("Update Available - GUI");
    if (!emulatorTag.isEmpty()) {
        title = launcherUpdate ? tr("Updates Available") : tr("Update Available - Emulator");
    }
    QLabel* titleLabel = new QLabel("<h1>" + title + "</h1>", this);
    titleLayout->addWidget(imageLabel);
    titleLayout->addWidget(titleLabel);
    layout->addLayout(titleLayout);

    QString updateText;
    if (launcherUpdate) {
        updateText = QString("<p>"
                             "<table><tr>"
                             "<td><b>" +
                             tr("Current Version") +
                             ":</b></td>"
                             "<td>%1</td>"
                             "<td>(%2)</td>"
                             "</tr><tr>"
                             "<td><b>" +
                             tr("Latest Version") +
                             ":</b></td>"
                             "<td>%3</td>"
                             "<td>(%4)</td>"
                             "</tr></table></p>")
                         .arg(currentRev.left(7), currentDate, latestRev.left(7), latestDate);
    }
    if (!emulatorTag.isEmpty()) {
        updateText += QString("<p><table>"
                              "<tr><td><b>%1:</b></td><td>%2</td></tr>"
                              "<tr><td><b>%3:</b></td><td>%4</td></tr>"
                              "</table></p>")
                          .arg(tr("Current Emulator"), emulatorLocalHash.left(7),
                               tr("Latest Emulator"), emulatorLatestHash.left(7));
    }

    QLabel* updateLabel = new QLabel(updateText, this);
    layout->addWidget(updateLabel);

    // Setup bottom layout with action buttons
    autoUpdateCheckBox = new QCheckBox(tr("Check for Updates at Startup"), this);
    layout->addWidget(autoUpdateCheckBox);
    updateEmulatorCheckBox =
        new QCheckBox(tr("Also Update the Emulator (Latest Pre-release)"), this);
    layout->addWidget(updateEmulatorCheckBox);

    QHBoxLayout* updatePromptLayout = new QHBoxLayout();
    QLabel* updatePromptLabel = new QLabel(tr("Do you want to update?"), this);
    updatePromptLayout->addWidget(updatePromptLabel);

    yesButton = new QPushButton(tr("Update"), this);
    noButton = new QPushButton(tr("No"), this);
    yesButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    noButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    QSpacerItem* spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    updatePromptLayout->addItem(spacer);
    updatePromptLayout->addWidget(yesButton);
    updatePromptLayout->addWidget(noButton);

    layout->addLayout(updatePromptLayout);

    // The changelog covers the launcher. It is not shown without a launcher update, or when the
    // current version is a pre-release and the version to be downloaded is a release.
    bool current_isWIP = currentRev.endsWith("WIP", Qt::CaseInsensitive);
    bool latest_isWIP = latestRev.endsWith("WIP", Qt::CaseInsensitive);
    if (!launcherUpdate || (current_isWIP && !latest_isWIP)) {
    } else {
        QTextBrowser* textField = new QTextBrowser(this);
        textField->setReadOnly(true);
        textField->setFixedWidth(500);
        textField->setFixedHeight(200);
        textField->setVisible(false);
        layout->addWidget(textField);

        QPushButton* toggleButton = new QPushButton(tr("Show Changelog"), this);
        layout->addWidget(toggleButton);

        connect(toggleButton, &QPushButton::clicked,
                [this, textField, toggleButton, currentRev, latestRev, downloadUrl, latestDate,
                 currentDate]() {
                    if (!textField->isVisible()) {
                        requestChangelog(currentRev, latestRev, downloadUrl, latestDate,
                                         currentDate);
                        textField->setVisible(true);
                        toggleButton->setText(tr("Hide Changelog"));
                        adjustSize();
                        textField->setFixedWidth(textField->width() + 20);
                    } else {
                        textField->setVisible(false);
                        toggleButton->setText(tr("Show Changelog"));
                        adjustSize();
                    }
                });

        if (m_gui_settings->GetValue(gui::gen_showChangeLog).toBool()) {
            requestChangelog(currentRev, latestRev, downloadUrl, latestDate, currentDate);
            textField->setVisible(true);
            toggleButton->setText(tr("Hide Changelog"));
            adjustSize();
            textField->setFixedWidth(textField->width() + 20);
        }
    }

    connect(yesButton, &QPushButton::clicked, this, [this, downloadUrl, launcherUpdate]() {
        yesButton->setEnabled(false);
        noButton->setEnabled(false);
        // The launcher update ends by restarting the launcher, so the emulator is installed first.
        const auto updateLauncher = [this, downloadUrl, launcherUpdate]() {
            if (launcherUpdate) {
                DownloadUpdate(downloadUrl);
            } else {
                close();
            }
        };
        if (emulatorTag.isEmpty() || !updateEmulatorCheckBox->isChecked()) {
            updateLauncher();
            return;
        }
        auto* versionDialog = new VersionDialog(m_gui_settings, this);
        connect(versionDialog, &VersionDialog::PreReleaseInstalled, this, updateLauncher);
        connect(versionDialog, &VersionDialog::PreReleaseInstallFailed, this, updateLauncher);
        versionDialog->installPreReleaseByTag(emulatorTag);
    });

    connect(noButton, &QPushButton::clicked, this, [this]() { close(); });

    autoUpdateCheckBox->setChecked(m_gui_settings->GetValue(gui::gen_checkForUpdates).toBool());
    connect(autoUpdateCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        const auto user_dir = Common::FS::GetUserPath(Common::FS::PathType::UserDir);
        m_gui_settings->SetValue(gui::gen_checkForUpdates, (state == Qt::Checked));
    });
    updateEmulatorCheckBox->setChecked(m_gui_settings->GetValue(gui::gen_updateEmulator).toBool());
    connect(updateEmulatorCheckBox, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState state) {
                m_gui_settings->SetValue(gui::gen_updateEmulator, state == Qt::Checked);
            });

    setLayout(layout);
}

void CheckUpdate::requestChangelog(const QString& currentRev, const QString& latestRev,
                                   const QString& downloadUrl, const QString& latestDate,
                                   const QString& currentDate) {
    QString compareUrlString =
        QString("https://api.github.com/repos/junminlee2004/GR2Launcher/compare/%1...%2")
            .arg(currentRev)
            .arg(latestRev);

    QUrl compareUrl(compareUrlString);
    QNetworkRequest compareRequest(compareUrl);
    QNetworkReply* compareReply = networkManager->get(compareRequest);

    connect(compareReply, &QNetworkReply::finished, this,
            [this, compareReply, downloadUrl, latestDate, latestRev, currentDate, currentRev]() {
                if (compareReply->error() != QNetworkReply::NoError) {
                    QMessageBox::warning(
                        this, tr("Error"),
                        QString(tr("Network error:") + "\n%1").arg(compareReply->errorString()));
                    compareReply->deleteLater();
                    return;
                }

                QByteArray compareResponse = compareReply->readAll();
                QJsonDocument compareJsonDoc(QJsonDocument::fromJson(compareResponse));
                QJsonObject compareJsonObj = compareJsonDoc.object();
                QJsonArray commits = compareJsonObj["commits"].toArray();

                QString changes;
                for (const QJsonValue& commitValue : commits) {
                    QJsonObject commitObj = commitValue.toObject();
                    QString message = commitObj["commit"].toObject()["message"].toString();

                    // Remove texts after first line break, if any, to make it cleaner
                    int newlineIndex = message.indexOf('\n');
                    if (newlineIndex != -1) {
                        message = message.left(newlineIndex);
                    }
                    if (!changes.isEmpty()) {
                        changes += "<br>";
                    }
                    changes += "&nbsp;&nbsp;&nbsp;&nbsp;• " + message;
                }

                // Update the text field with the changelog
                QTextBrowser* textField = findChild<QTextBrowser*>();
                if (textField) {
                    QRegularExpression re("\\(\\#(\\d+)\\)");
                    QString newChanges;
                    int lastIndex = 0;
                    QRegularExpressionMatchIterator i = re.globalMatch(changes);
                    while (i.hasNext()) {
                        QRegularExpressionMatch match = i.next();
                        newChanges += changes.mid(lastIndex, match.capturedStart() - lastIndex);
                        QString num = match.captured(1);
                        newChanges += QString("(<a "
                                              "href=\"https://github.com/shadps4-emu/"
                                              "shadps4-qtlauncher/pull/%1\">#%1</a>)")
                                          .arg(num);
                        lastIndex = match.capturedEnd();
                    }

                    newChanges += changes.mid(lastIndex);
                    changes = newChanges;

                    textField->setOpenExternalLinks(true);
                    textField->setHtml("<h2>" + tr("Changes") + ":</h2>" + changes);
                }

                compareReply->deleteLater();
            });
}

void CheckUpdate::DownloadUpdate(const QString& url) {
    QProgressBar* progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(true);
    progressBar->setValue(0);

    layout()->addWidget(progressBar);

    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [progressBar](qint64 bytesReceived, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
                    progressBar->setValue(percentage);
                }
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, progressBar, url]() {
        progressBar->setValue(100);
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Network error occurred while trying to access the URL") +
                                     ":\n" + url + "\n" + reply->errorString());
            reply->deleteLater();
            progressBar->deleteLater();
            return;
        }

        QString userPath;
        Common::FS::PathToQString(userPath, Common::FS::GetUserPath(Common::FS::PathType::UserDir));
#ifdef Q_OS_WIN
        QString tempDownloadPath =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
            "/Temp/temp_download_update";
#else
        QString tempDownloadPath = userPath + "/temp_download_update";
#endif
        QDir dir(tempDownloadPath);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        QString downloadPath = tempDownloadPath + "/temp_download_update.zip";
        QFile file(downloadPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            QMessageBox::information(this, tr("Download Complete"),
                                     tr("The update has been downloaded, press OK to install."));
            Install();
        } else {
            QMessageBox::warning(
                this, tr("Error"),
                QString(tr("Failed to save the update file at") + ":\n" + downloadPath));
        }

        reply->deleteLater();
        progressBar->deleteLater();
    });
}

void CheckUpdate::Install() {
    QString userPath;
    Common::FS::PathToQString(userPath, Common::FS::GetUserPath(Common::FS::PathType::UserDir));

    QString rootPath = QCoreApplication::applicationDirPath();
#ifdef Q_OS_LINUX
    const char* appimage_path = std::getenv("APPIMAGE");
    if (appimage_path) {
        rootPath = QString(std::filesystem::path(appimage_path).parent_path().c_str());
    }
#endif
#ifdef Q_OS_MACOS
    Common::FS::PathToQString(rootPath, std::filesystem::current_path());
#endif

    QString tempDirPath = userPath + "/temp_download_update";
    QString startingUpdate = tr("Starting Update...");

    QString binaryStartingUpdate;
    for (QChar c : startingUpdate) {
        binaryStartingUpdate.append(QString::number(c.unicode(), 2).rightJustified(16, '0'));
    }

    QString scriptContent;
    QString scriptFileName;
    QStringList arguments;
    QString processCommand;

#ifdef Q_OS_WIN
    // On windows, overwrite tempDirPath with AppData/Roaming/shadps4/Temp folder
    // due to PowerShell Expand-Archive not being able to handle correctly
    // paths in square brackets (ie: ./[shadps4])
    tempDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                  "/Temp/temp_download_update";

    // Windows Batch Script
    scriptFileName = tempDirPath + "/update.ps1";
    scriptContent = QStringLiteral(
        "Set-ExecutionPolicy Bypass -Scope Process -Force\n"
        "$binaryStartingUpdate = '%1'\n"
        "$chars = @()\n"
        "for ($i = 0; $i -lt $binaryStartingUpdate.Length; $i += 16) {\n"
        "    $chars += [char]([convert]::ToInt32($binaryStartingUpdate.Substring($i, 16), 2))\n"
        "}\n"
        "$startingUpdate = -join $chars\n"
        "Write-Output $startingUpdate\n"
        "Expand-Archive -Path '%2\\temp_download_update.zip' -DestinationPath '%2' -Force\n"
        "Start-Sleep -Seconds 3\n"
        "foreach ($dir in 'user', 'launcher') {\n"
        "    $path = Join-Path '%2' $dir\n"
        "    if ((Test-Path -LiteralPath $path) -and -not (Get-ChildItem -LiteralPath $path "
        "-Force)) {\n"
        "        Remove-Item -LiteralPath $path -Force\n"
        "    }\n"
        "}\n"
        "Copy-Item -Recurse -Force '%2\\*' '%3\\'\n"
        "Start-Sleep -Seconds 2\n"
        "Remove-Item -Force -LiteralPath '%3\\update.ps1'\n"
        "Remove-Item -Force -LiteralPath '%3\\temp_download_update.zip'\n"
        "Remove-Item -Recurse -Force '%2'\n"
        "Start-Process -FilePath '%3\\GR2Launcher.exe' "
        "-WorkingDirectory ([WildcardPattern]::Escape('%3'))\n");
    arguments << "-ExecutionPolicy"
              << "Bypass"
              << "-File" << scriptFileName;
    processCommand = "powershell.exe";

#elif defined(Q_OS_LINUX)
    // Linux Shell Script
    scriptFileName = tempDirPath + "/update.sh";
    scriptContent = QStringLiteral(
        "#!/bin/bash\n"
        "check_unzip() {\n"
        "    if ! command -v unzip &> /dev/null && ! command -v 7z &> /dev/null; then\n"
        "        echo \"Neither 'unzip' nor '7z' is installed.\"\n"
        "        read -p \"Would you like to install 'unzip'? (y/n): \" response\n"
        "        if [[ \"$response\" == \"y\" || \"$response\" == \"Y\" ]]; then\n"
        "            if [[ -f /etc/os-release ]]; then\n"
        "                . /etc/os-release\n"
        "                case \"$ID\" in\n"
        "                    ubuntu|debian)\n"
        "                        sudo apt-get install unzip -y\n"
        "                        ;;\n"
        "                    fedora|redhat)\n"
        "                        sudo dnf install unzip -y\n"
        "                        ;;\n"
        "                    *)\n"
        "                        echo \"Unsupported distribution for automatic installation.\"\n"
        "                        exit 1\n"
        "                        ;;\n"
        "                esac\n"
        "            else\n"
        "                echo \"Could not identify the distribution.\"\n"
        "                exit 1\n"
        "            fi\n"
        "        else\n"
        "            echo \"At least one of 'unzip' or '7z' is required to continue. The process "
        "will be terminated.\"\n"
        "            exit 1\n"
        "        fi\n"
        "    fi\n"
        "}\n"
        "extract_file() {\n"
        "    if command -v unzip &> /dev/null; then\n"
        "        unzip -o \"%2/temp_download_update.zip\" -d \"%2/\"\n"
        "    elif command -v 7z &> /dev/null; then\n"
        "        7z x \"%2/temp_download_update.zip\" -o\"%2/\" -y\n"
        "    else\n"
        "        echo \"No suitable extraction tool found.\"\n"
        "        exit 1\n"
        "    fi\n"
        "}\n"
        "main() {\n"
        "    check_unzip\n"
        "    echo \"%1\"\n"
        "    sleep 2\n"
        "    extract_file\n"
        "    sleep 2\n"
        "    rmdir \"%2/user\" \"%2/launcher\" 2> /dev/null\n"
        "    if pgrep -f \"GR2Launcher.AppImage\" > /dev/null; then\n"
        "        pkill -f \"GR2Launcher.AppImage\"\n"
        "        sleep 2\n"
        "    fi\n"
        "    cp -r \"%2/\"* \"%3/\"\n"
        "    sleep 2\n"
        "    rm \"%3/update.sh\"\n"
        "    rm \"%3/temp_download_update.zip\"\n"
        "    chmod +x \"%3/GR2Launcher.AppImage\"\n"
        "    rm -r \"%2\"\n"
        "    cd \"%3\" && ./GR2Launcher.AppImage\n"
        "}\n"
        "main\n");
    arguments << scriptFileName;
    processCommand = "bash";

#elif defined(Q_OS_MAC)
    // macOS Shell Script
    scriptFileName = tempDirPath + "/update.sh";
    scriptContent = QStringLiteral(
        "#!/bin/bash\n"
        "check_tools() {\n"
        "    if ! command -v unzip &> /dev/null && ! command -v tar &> /dev/null; then\n"
        "        echo \"Neither 'unzip' nor 'tar' is installed.\"\n"
        "        read -p \"Would you like to install 'unzip'? (y/n): \" response\n"
        "        if [[ \"$response\" == \"y\" || \"$response\" == \"Y\" ]]; then\n"
        "            echo \"Please install 'unzip' using Homebrew or another package manager.\"\n"
        "            exit 1\n"
        "        else\n"
        "            echo \"At least one of 'unzip' or 'tar' is required to continue. The process "
        "will be terminated.\"\n"
        "            exit 1\n"
        "        fi\n"
        "    fi\n"
        "}\n"
        "check_tools\n"
        "echo \"%1\"\n"
        "sleep 2\n"
        "unzip -o \"%2/temp_download_update.zip\" -d \"%2/\"\n"
        "sleep 2\n"
        "tar -xzf \"%2/GR2Launcher-macos-qt.tar.gz\" -C \"%3\"\n"
        "sleep 2\n"
        "chmod +x \"%3/GR2Launcher.app/Contents/MacOS/GR2Launcher\"\n"
        "open \"%3/GR2Launcher.app\"\n"
        "rm -r \"%2\"\n");

    arguments << scriptFileName;
    processCommand = "bash";

#else
    QMessageBox::warning(this, tr("Error"), "Unsupported operating system.");
    return;
#endif

    QFile scriptFile(scriptFileName);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
#ifndef Q_OS_MAC
        scriptFile.write("\xEF\xBB\xBF");
#endif
#ifdef Q_OS_WIN
        out << scriptContent.arg(binaryStartingUpdate).arg(tempDirPath).arg(rootPath);
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
        out << scriptContent.arg(startingUpdate).arg(tempDirPath).arg(rootPath);
#endif
        scriptFile.close();

// Make the script executable on Unix-like systems
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
        scriptFile.setPermissions(QFileDevice::ExeOwner | QFileDevice::ReadOwner |
                                  QFileDevice::WriteOwner);
#endif

        QProcess::startDetached(processCommand, arguments);

        exit(EXIT_SUCCESS);
    } else {
        QMessageBox::warning(
            this, tr("Error"),
            QString(tr("Failed to create the update script file") + ":\n" + scriptFileName));
    }
}
