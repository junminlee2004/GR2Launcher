// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <utility>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStringListModel>
#include <QTabWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "cheats_patches.h"
#include "common/logging/log.h"
#include "common/memory_patcher.h"
#include "common/path_util.h"
#include "core/emulator_state.h"
#include "ui_cheats_patches.h"

namespace {

// Separates the levels of a Category path: "Resolution/Upscaling/16:9" is a section inside a
// section inside a section.
constexpr QChar CategorySeparator = u'/';

// A collapsible group of patches: a header button that folds the body, which holds the patch
// checkboxes and the nested groups.
struct PatchSection {
    QToolButton* header{};
    QWidget* body{};
    QString title; ///< The last level of the Category path
};

PatchSection addPatchSection(QVBoxLayout* list, const QString& title) {
    PatchSection section{new QToolButton, new QWidget, title};
    section.header->setCheckable(true);
    section.header->setArrowType(Qt::RightArrow);
    section.header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    section.header->setAutoRaise(true);
    QFont font = section.header->font();
    font.setBold(true);
    section.header->setFont(font);

    auto* bodyLayout = new QVBoxLayout(section.body);
    bodyLayout->setContentsMargins(20, 0, 0, 4);
    section.body->setVisible(false);

    QObject::connect(section.header, &QToolButton::toggled, section.body,
                     [header = section.header, body = section.body](bool expanded) {
                         header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
                         body->setVisible(expanded);
                     });
    list->addWidget(section.header, 0, Qt::AlignLeft);
    list->addWidget(section.body);
    return section;
}

qsizetype countCheckedPatches(const PatchSection& section) {
    const QList<QCheckBox*> boxes = section.body->findChildren<QCheckBox*>();
    return std::count_if(boxes.begin(), boxes.end(),
                         [](const QCheckBox* box) { return box->isChecked(); });
}

void updatePatchSectionHeader(const PatchSection& section) {
    // "&&" shows a literal '&' instead of marking a shortcut mnemonic.
    const QString title = QString(section.title).replace("&", "&&");
    const qsizetype total = section.body->findChildren<QCheckBox*>().size();
    section.header->setText(
        CheatsPatches::tr("%1 (%2 of %3 on)")
            .arg(title, QString::number(countCheckedPatches(section)), QString::number(total)));
}

// Returns the sections of a Category path from the outermost one, creating the missing ones at the
// end of their parent. Empty levels are skipped; a path without levels gives no section.
QList<PatchSection> findOrAddSections(QMap<QString, PatchSection>& sections, QVBoxLayout* list,
                                      const QString& path) {
    QList<PatchSection> chain;
    QString key;
    QVBoxLayout* parent = list;
    for (const QString& level : path.split(CategorySeparator, Qt::SkipEmptyParts)) {
        const QString title = level.trimmed();
        if (title.isEmpty()) {
            continue;
        }
        key = key.isEmpty() ? title : key + CategorySeparator + title;
        auto found = sections.find(key);
        if (found == sections.end()) {
            found = sections.insert(key, addPatchSection(parent, title));
        }
        chain << *found;
        parent = qobject_cast<QVBoxLayout*>(found->body->layout());
    }
    return chain;
}

void addPatchToSections(const QList<PatchSection>& chain, QCheckBox* patchCheckBox) {
    chain.last().body->layout()->addWidget(patchCheckBox);
    QObject::connect(patchCheckBox, &QCheckBox::toggled, chain.last().body, [chain]() {
        for (const PatchSection& section : chain) {
            updatePatchSectionHeader(section);
        }
    });
}

// The headers and checkboxes of a patch list in screen order: a section body is a plain widget
// with a layout, the buttons have none.
void collectPatchFocusOrder(QLayout* layout, QList<QWidget*>& order) {
    for (int i = 0; i < layout->count(); ++i) {
        QWidget* widget = layout->itemAt(i)->widget();
        if (!widget) {
            continue;
        }
        if (QLayout* inner = widget->layout()) {
            collectPatchFocusOrder(inner, order);
        } else {
            order << widget;
        }
    }
}

} // namespace

CheatsPatches::CheatsPatches(std::shared_ptr<gui_settings> gui_settings,
                             std::shared_ptr<IpcClient> ipc_client, const QString& gameName,
                             const QString& gameSerial, const QString& gameVersion,
                             const QString& gameSize, const QPixmap& gameImage, QWidget* parent)
    : QWidget(parent), m_gameName(gameName), m_gameSerial(gameSerial), m_gameVersion(gameVersion),
      m_gameSize(gameSize), m_gameImage(gameImage), m_gui_settings(std::move(gui_settings)),
      m_ipc_client(std::move(ipc_client)), manager(new QNetworkAccessManager(this)),
      ui(std::make_unique<Ui::CheatsPatches>()) {
    ui->setupUi(this);

    setupUI();
    setWindowTitle(tr("Cheats / Patches for ") + m_gameName);
}

CheatsPatches::~CheatsPatches() {}

void CheatsPatches::setupUI() {

    // clang-format off
    defaultTextEdit_MSG = tr("Cheats/Patches are experimental.\\nUse with caution.\\n\\nDownload cheats individually by selecting the repository and clicking the download button.\\nIn the Patches tab, you can download all patches at once, choose which ones you want to use, and save your selection.\\n\\nSince we do not develop the Cheats/Patches,\\nplease report issues to the cheat author.\\n\\nCreated a new cheat? Visit:\\n").replace("\\n", "\n")+"https://github.com/shadps4-emu/ps4_cheats";
    CheatsNotFound_MSG = tr("No Cheats found for this game in this version of the selected repository,try another repository or a different version of the game.");
    CheatsDownloadedSuccessfully_MSG = tr("You have successfully downloaded the cheats for this version of the game from the selected repository. You can try downloading from another repository, if it is available it will also be possible to use it by selecting the file from the list.");
    DownloadComplete_MSG = tr("Patches Downloaded Successfully! All Patches available for all games have been downloaded, there is no need to download them individually for each game as happens in Cheats. If the patch does not appear, it may be that it does not exist for the specific serial and version of the game.");
    // clang-format on

    QString cheatsDir;
    Common::FS::PathToQString(cheatsDir, Common::FS::GetUserPath(Common::FS::PathType::CheatsDir));

    QString patchesDir;
    Common::FS::PathToQString(patchesDir,
                              Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));

    QString nameCheatJson = m_gameSerial + "_" + m_gameVersion + ".json";

    m_cheatFilePath = cheatsDir + "/" + nameCheatJson;

    ui->gameInfoGroupBox->setLayout(ui->gameInfoLayout);
    ui->gameInfoLayout->setAlignment(Qt::AlignTop);

    if (!m_gameImage.isNull()) {
        ui->gameImageLabel->setPixmap(
            m_gameImage.scaled(275, 275, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->gameImageLabel->setText(tr("No Image Available"));
    }

    ui->gameImageLabel->setAlignment(Qt::AlignCenter);

    ui->gameNameLabel->setText(m_gameName);
    ui->gameNameLabel->setAlignment(Qt::AlignLeft);
    ui->gameNameLabel->setWordWrap(true);

    ui->gameSerialLabel->setText(tr("Serial: ") + m_gameSerial);
    ui->gameSerialLabel->setAlignment(Qt::AlignLeft);

    ui->gameVersionLabel->setText(tr("Version: ") + m_gameVersion);
    ui->gameVersionLabel->setAlignment(Qt::AlignLeft);

    ui->gameSizeLabel->setText(tr("Size: ") + m_gameSize);
    ui->gameSizeLabel->setAlignment(Qt::AlignLeft);

    ui->gameSizeLabel->setVisible(
        m_gui_settings->GetValue(gui::glc_showLoadGameSizeEnabled).toBool());

    ui->instructionsTextEdit->setText(defaultTextEdit_MSG);
    ui->instructionsTextEdit->setReadOnly(true);
    ui->instructionsTextEdit->setFixedHeight(290);

    // Cheats
    ui->rightLayout->setAlignment(Qt::AlignTop);
    ui->listView_selectFile->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listView_selectFile->setEditTriggers(QAbstractItemView::NoEditTriggers);
    populateFileListCheats();

    // Cheat repository
    ui->downloadComboBox->addItem("GoldHEN", "GoldHEN");
    ui->downloadComboBox->addItem("shadPS4", "shadPS4");

    // Download cheats
    connect(ui->downloadButton, &QPushButton::clicked, this, [this]() {
        const QString source = ui->downloadComboBox->currentData().toString();
        downloadCheats(source, m_gameSerial, m_gameVersion, true);
    });

    // Delete cheat file
    connect(ui->deleteCheatButton, &QPushButton::clicked, this, [this, cheatsDir]() {
        QStringListModel* model = qobject_cast<QStringListModel*>(ui->listView_selectFile->model());

        if (!model) {
            return;
        }
        QItemSelectionModel* selectionModel = ui->listView_selectFile->selectionModel();
        if (!selectionModel) {
            return;
        }
        QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
        if (selectedIndexes.isEmpty()) {
            QMessageBox::warning(
                this, tr("Delete File"),
                tr("No files selected.") + "\n" +
                    tr("You can delete the cheats you don't want after downloading them."));
            return;
        }
        QModelIndex selectedIndex = selectedIndexes.first();
        QString selectedFileName = model->data(selectedIndex).toString();

        int ret = QMessageBox::warning(this, tr("Delete File"),
                                       QString(tr("Do you want to delete the selected file?\\n%1"))
                                           .replace("\\n", "\n")
                                           .arg(selectedFileName),
                                       QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes) {
            QString filePath = cheatsDir + "/" + selectedFileName;
            QFile::remove(filePath);
            populateFileListCheats();
        }
    });

    // Close
    connect(ui->closeButton, &QPushButton::clicked, this, [this]() { QWidget::close(); });

    // Patches
    ui->patchesGroupBoxLayout->setAlignment(Qt::AlignTop);
    ui->patchesListView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->patchesListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QStringListModel* patchesModel = new QStringListModel(this);
    ui->patchesListView->setModel(patchesModel);

    // Patch repository
    ui->patchesComboBox->addItem("shadPS4", "shadPS4");
    ui->patchesComboBox->addItem("GoldHEN (GR2 Fork)", "GoldHEN");

    // Download patches
    connect(ui->patchesButton, &QPushButton::clicked, this, [this]() {
        QString selectedOption = ui->patchesComboBox->currentData().toString();
        downloadPatches(selectedOption, true);
    });

    // Register local patches
    connect(ui->registerPatchesButton, &QPushButton::clicked, this, [this]() {
        const int folders = registerLocalPatches();
        populateFileListPatches();
        QMessageBox::information(this, tr("Register Patches"),
                                 tr("Patch files registered from %1 folders. Nothing was "
                                    "downloaded or replaced.")
                                     .arg(folders));
    });

    // Delete patch
    connect(ui->deletePatchButton, &QPushButton::clicked, this, [this, patchesDir]() {
        QStringListModel* model = qobject_cast<QStringListModel*>(ui->patchesListView->model());

        if (!model) {
            return;
        }
        QItemSelectionModel* selectionModel = ui->patchesListView->selectionModel();
        if (!selectionModel) {
            return;
        }
        QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
        if (selectedIndexes.isEmpty()) {
            QMessageBox::warning(this, tr("Delete File"), tr("No files selected."));
            return;
        }
        QModelIndex selectedIndex = selectedIndexes.first();
        QString selectedFileName = model->data(selectedIndex).toString();
        int ret = QMessageBox::warning(this, tr("Delete File"),
                                       QString(tr("Do you want to delete the selected file?\\n%1"))
                                           .replace("\\n", "\n")
                                           .arg(selectedFileName),
                                       QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes) {
            QString fileName = selectedFileName.split('|').first().trimmed();
            QString directoryName = selectedFileName.split('|').last().trimmed();
            QString filePath = patchesDir + "/" + directoryName + "/" + fileName;

            QFile::remove(filePath);
            createFilesJson(directoryName);
            populateFileListPatches();
        }
    });

    // Save
    connect(ui->saveButton, &QPushButton::clicked, this, &CheatsPatches::onSaveButtonClicked);

    // When switching to Patches, refresh the list.
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) {
            populateFileListPatches();
        }
    });
}

// Get the name of the selected folder in the patchesListView
void CheatsPatches::onSaveButtonClicked() {
    QString selectedPatchName;
    QModelIndexList selectedIndexes = ui->patchesListView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No patch selected."));
        return;
    }
    selectedPatchName = ui->patchesListView->model()->data(selectedIndexes.first()).toString();
    int separatorIndex = selectedPatchName.indexOf(" | ");
    selectedPatchName = selectedPatchName.mid(separatorIndex + 3);

    QString patchDir;
    Common::FS::PathToQString(patchDir, Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    patchDir += "/" + selectedPatchName;

    QString filesJsonPath = patchDir + "/files.json";
    QFile jsonFile(filesJsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Unable to open files.json for reading."));
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = jsonDoc.object();

    QString selectedFileName;
    QString serial = m_gameSerial;

    for (auto it = jsonObject.constBegin(); it != jsonObject.constEnd(); ++it) {
        QString filePath = it.key();
        QJsonArray idsArray = it.value().toArray();

        if (idsArray.contains(QJsonValue(serial))) {
            selectedFileName = filePath;
            break;
        }
    }

    if (selectedFileName.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("No patch file found for the current serial."));
        return;
    }

    QString filePath = patchDir + "/" + selectedFileName;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Unable to open the file for reading."));
        return;
    }

    QByteArray xmlData = file.readAll();
    file.close();

    QString newXmlData;
    QXmlStreamWriter xmlWriter(&newXmlData);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();

    QXmlStreamReader xmlReader(xmlData);

    while (!xmlReader.atEnd()) {
        xmlReader.readNext();

        if (xmlReader.isStartElement()) {
            if (xmlReader.name() == QStringLiteral("Metadata")) {
                xmlWriter.writeStartElement(xmlReader.name().toString());

                QString name = xmlReader.attributes().value("Name").toString();
                QString version = xmlReader.attributes().value("AppVer").toString();

                bool versionMatch = version == m_gameVersion;
                bool isEnabled = false;
                bool foundPatchInfo = false;

                // Check and update the isEnabled attribute
                for (const QXmlStreamAttribute& attr : xmlReader.attributes()) {
                    if (attr.name() == QStringLiteral("isEnabled"))
                        continue;
                    xmlWriter.writeAttribute(attr.name().toString(), attr.value().toString());
                }

                auto it = m_patchInfos.find(name);
                if (it != m_patchInfos.end()) {
                    QCheckBox* checkBox = findCheckBoxByName(it->name);
                    if (checkBox) {
                        foundPatchInfo = true;
                        isEnabled = checkBox->isChecked();
                    }
                }
                if (!foundPatchInfo) {
                    auto maskIt = m_patchInfos.find(name + " (any version)");
                    if (maskIt != m_patchInfos.end()) {
                        QCheckBox* checkBox = findCheckBoxByName(maskIt->name);
                        if (checkBox) {
                            versionMatch = true;
                            foundPatchInfo = true;
                            isEnabled = checkBox->isChecked();
                        }
                    }
                }
                if (foundPatchInfo) {
                    xmlWriter.writeAttribute("isEnabled",
                                             (isEnabled && versionMatch) ? "true" : "false");
                }
            } else {
                xmlWriter.writeStartElement(xmlReader.name().toString());
                for (const QXmlStreamAttribute& attr : xmlReader.attributes()) {
                    xmlWriter.writeAttribute(attr.name().toString(), attr.value().toString());
                }
            }
        } else if (xmlReader.isEndElement()) {
            xmlWriter.writeEndElement();
        } else if (xmlReader.isCharacters() && !xmlReader.isWhitespace()) {
            xmlWriter.writeCharacters(xmlReader.text().toString());
        }
    }

    xmlWriter.writeEndDocument();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Unable to open the file for writing."));
        return;
    }

    QTextStream textStream(&file);
    textStream << newXmlData;
    file.close();

    if (xmlReader.hasError()) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to parse XML: ") + "\n" + xmlReader.errorString());
    } else {
        QMessageBox::information(this, tr("Success"), tr("Options saved successfully."));
    }

    QWidget::close();
}

QCheckBox* CheatsPatches::findCheckBoxByName(const QString& name) {
    return m_patchCheckBoxes.value(name, nullptr);
}

void CheatsPatches::downloadCheats(const QString& source, const QString& gameSerial,
                                   const QString& gameVersion, const bool showMessageBox) {
    QDir dir(Common::FS::GetUserPath(Common::FS::PathType::CheatsDir));
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString url;
    if (source == "GoldHEN") {
        url = "https://raw.githubusercontent.com/GoldHEN/GoldHEN_Cheat_Repository/main/json.txt";
    } else if (source == "shadPS4") {
        url = "https://raw.githubusercontent.com/shadps4-emu/ps4_cheats/main/CHEATS_JSON.txt";
    } else {
        QMessageBox::warning(this, tr("Invalid Source"),
                             QString(tr("The selected source is invalid.") + "\n%1").arg(source));
        return;
    }

    QNetworkRequest request(url);
    QNetworkReply* reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray jsonData = reply->readAll();
            bool foundFiles = false;

            if (source == "GoldHEN" || source == "shadPS4") {
                QString textContent(jsonData);
                QRegularExpression regex(
                    QString("%1_%2[^=]*\\.json").arg(gameSerial).arg(gameVersion));
                QRegularExpressionMatchIterator matches = regex.globalMatch(textContent);
                QString baseUrl;

                if (source == "GoldHEN") {
                    baseUrl = "https://raw.githubusercontent.com/GoldHEN/GoldHEN_Cheat_Repository/"
                              "main/json/";
                } else {
                    baseUrl =
                        "https://raw.githubusercontent.com/shadps4-emu/ps4_cheats/main/CHEATS/";
                }

                while (matches.hasNext()) {
                    QRegularExpressionMatch match = matches.next();
                    QString fileName = match.captured(0);

                    if (!fileName.isEmpty()) {
                        QString newFileName = fileName;
                        int dotIndex = newFileName.lastIndexOf('.');
                        if (dotIndex != -1) {

                            if (source == "GoldHEN") {
                                newFileName.insert(dotIndex, "_GoldHEN");
                            } else {
                                newFileName.insert(dotIndex, "_shadPS4");
                            }
                        }
                        QString fileUrl = baseUrl + fileName;
                        QString localFilePath = dir.filePath(newFileName);

                        if (QFile::exists(localFilePath) && showMessageBox) {
                            QMessageBox::StandardButton answer;
                            answer = QMessageBox::question(
                                this, tr("File Exists"),
                                tr("File already exists. Do you want to replace it?") + "\n" +
                                    newFileName,
                                QMessageBox::Yes | QMessageBox::No);
                            if (answer == QMessageBox::No) {
                                continue;
                            }
                        }
                        QNetworkRequest fileRequest(fileUrl);
                        QNetworkReply* fileReply = manager->get(fileRequest);

                        connect(fileReply, &QNetworkReply::finished, this, [=, this]() {
                            if (fileReply->error() == QNetworkReply::NoError) {
                                QByteArray fileData = fileReply->readAll();
                                QFile localFile(localFilePath);
                                if (localFile.open(QIODevice::WriteOnly)) {
                                    localFile.write(fileData);
                                    localFile.close();
                                } else {
                                    QMessageBox::warning(
                                        this, tr("Error"),
                                        QString(tr("Failed to save file:") + "\n%1")
                                            .arg(localFilePath));
                                }
                            } else {
                                QMessageBox::warning(this, tr("Error"),
                                                     QString(tr("Failed to download file:") +
                                                             "%1\n\n" + tr("Error") + ":%2")
                                                         .arg(fileUrl)
                                                         .arg(fileReply->errorString()));
                            }
                            fileReply->deleteLater();
                        });
                        foundFiles = true;
                    }
                }
                if (!foundFiles && showMessageBox) {
                    QMessageBox::warning(this, tr("Cheats Not Found"), CheatsNotFound_MSG);
                }
            }
            if (foundFiles && showMessageBox) {
                QMessageBox::information(this, tr("Cheats Downloaded Successfully"),
                                         CheatsDownloadedSuccessfully_MSG);
                populateFileListCheats();
            }
        } else {
            if (showMessageBox) {
                QMessageBox::warning(this, tr("Cheats Not Found"), CheatsNotFound_MSG);
            }
        }
        reply->deleteLater();
        emit downloadFinished();
    });

    // connect(reply, &QNetworkReply::errorOccurred, [=](QNetworkReply::NetworkError code) {
    //     if (showMessageBox)
    //         QMessageBox::warning(this, "Download Error",
    //                              QString("Error in response: %1").arg(reply->errorString()));
    // });
}

void CheatsPatches::populateFileListPatches() {
    QLayoutItem* item;
    while ((item = ui->patchesGroupBoxLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_patchInfos.clear();
    m_patchCheckBoxes.clear();

    QString patchesDir;
    Common::FS::PathToQString(patchesDir,
                              Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    QDir dir(patchesDir);

    QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList matchingFiles;
    QString shadPS4entry = "";

    for (QString const& folder : folders) {
        QString folderPath = dir.filePath(folder);
        QDir subDir(folderPath);

        QString filesJsonPath = subDir.filePath("files.json");
        QFile file(filesJsonPath);

        if (file.open(QIODevice::ReadOnly)) {
            QByteArray fileData = file.readAll();
            file.close();

            QJsonDocument jsonDoc(QJsonDocument::fromJson(fileData));
            QJsonObject jsonObj = jsonDoc.object();

            for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it) {
                QString fileName = it.key();
                QJsonArray serials = it.value().toArray();

                if (serials.contains(QJsonValue(m_gameSerial))) {
                    QString fileEntry = fileName + " | " + folder;
                    if (!matchingFiles.contains(fileEntry)) {
                        if (folder == "shadPS4") {
                            shadPS4entry = fileEntry;
                        }
                        matchingFiles << fileEntry;
                    }
                }
            }
        }
    }
    QStringListModel* model = new QStringListModel(matchingFiles, this);
    if (shadPS4entry != "") {
        QModelIndexList matches = model->match(model->index(0, 0), Qt::DisplayRole, shadPS4entry, 1,
                                               Qt::MatchExactly | Qt::MatchCaseSensitive);
        if (!matches.isEmpty()) {
            QModelIndex shadPS4Index = matches.first();
            model->moveRow(QModelIndex(), shadPS4Index.row(), QModelIndex(), 0);
        }
    }

    ui->patchesListView->setModel(model);

    connect(ui->patchesListView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() {
                QModelIndexList selectedIndexes =
                    ui->patchesListView->selectionModel()->selectedIndexes();
                if (!selectedIndexes.isEmpty()) {
                    QString selectedText = selectedIndexes.first().data().toString();
                    addPatchesToLayout(selectedText);
                }
            });

    if (!matchingFiles.isEmpty()) {
        QModelIndex firstIndex = model->index(0, 0);
        ui->patchesListView->selectionModel()->select(firstIndex, QItemSelectionModel::Select |
                                                                      QItemSelectionModel::Rows);
        ui->patchesListView->setCurrentIndex(firstIndex);
    }
}

void CheatsPatches::downloadPatches(const QString repository, const bool showMessageBox) {
    QString url;
    if (repository == "shadPS4") {
        url = "https://api.github.com/repos/shadps4-emu/ps4_cheats/contents/PATCHES";
    }
    if (repository == "GoldHEN") {
        url =
            "https://api.github.com/repos/junminlee2004/GR2fork_GoldHEN_Patch_Repository/contents/"
            "patches/xml";
    }
    QNetworkAccessManager* downloadManager = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    QNetworkReply* reply = downloadManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray jsonData = reply->readAll();
            reply->deleteLater();

            QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
            QJsonArray itemsArray = jsonDoc.array();

            if (itemsArray.isEmpty()) {
                if (showMessageBox) {
                    QMessageBox::warning(this, tr("Error"),
                                         tr("Failed to parse JSON data from HTML."));
                }
                return;
            }

            QDir dir(Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
            QString fullPath = dir.filePath(repository);
            if (!dir.exists(fullPath)) {
                dir.mkpath(fullPath);
            }
            dir.setPath(fullPath);

            for (QJsonValue const& value : itemsArray) {
                QJsonObject fileObj = value.toObject();
                QString fileName = fileObj["name"].toString();
                QString downloadUrl = fileObj["download_url"].toString();

                if (fileName.endsWith(".xml")) {
                    QNetworkRequest fileRequest(downloadUrl);
                    QNetworkReply* fileReply = downloadManager->get(fileRequest);

                    connect(fileReply, &QNetworkReply::finished, this, [=, this]() {
                        if (fileReply->error() == QNetworkReply::NoError) {
                            QByteArray fileData = fileReply->readAll();
                            QFile localFile(dir.filePath(fileName));
                            if (localFile.open(QIODevice::WriteOnly)) {
                                localFile.write(fileData);
                                localFile.close();
                            } else {
                                if (showMessageBox) {
                                    QMessageBox::warning(
                                        this, tr("Error"),
                                        QString(tr("Failed to save:") + "\n%1").arg(fileName));
                                }
                            }
                        } else {
                            if (showMessageBox) {
                                QMessageBox::warning(
                                    this, tr("Error"),
                                    QString(tr("Failed to download:") + "\n%1").arg(downloadUrl));
                            }
                        }
                        fileReply->deleteLater();
                    });
                }
            }
            if (showMessageBox) {
                QMessageBox::information(this, tr("Download Complete"),
                                         QString(DownloadComplete_MSG));
            }
            // Create the files.json file with the identification of which file to open
            createFilesJson(repository);
            populateFileListPatches();
            compatibleVersionNotice(repository);
        } else {
            if (showMessageBox) {
                QMessageBox::warning(this, tr("Error"),
                                     QString(tr("Failed to retrieve HTML page.") + "\n%1")
                                         .arg(reply->errorString()));
            }
        }
        emit downloadFinished();
    });
}

void CheatsPatches::compatibleVersionNotice(const QString repository) {
    QDir patchesDir(Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    QDir dir = patchesDir.filePath(repository);

    QStringList xmlFiles = dir.entryList(QStringList() << "*.xml", QDir::Files);
    QStringList incompatMessages;

    for (QString const& xmlFile : xmlFiles) {
        QFile file(dir.filePath(xmlFile));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Error"),
                                 QString(tr("Failed to open file:") + "\n%1").arg(xmlFile));
            continue;
        }

        QXmlStreamReader xmlReader(&file);
        QSet<QString> appVersionsSet;
        bool foundMatchingID = false;

        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();
            if (token == QXmlStreamReader::StartElement) {
                if (xmlReader.name() == QStringLiteral("ID")) {
                    QString id = xmlReader.readElementText();
                    if (id == m_gameSerial) {
                        foundMatchingID = true;
                    }
                } else if (xmlReader.name() == QStringLiteral("Metadata")) {
                    if (foundMatchingID) {
                        QString appVer = xmlReader.attributes().value("AppVer").toString();
                        if (!appVer.isEmpty()) {
                            appVersionsSet.insert(appVer);
                        }
                    }
                }
            }
        }

        if (xmlReader.hasError()) {
            QMessageBox::warning(this, tr("Error"),
                                 QString(tr("XML ERROR:") + "\n%1").arg(xmlReader.errorString()));
        }

        if (!foundMatchingID) {
            continue;
        }

        for (const QString& appVer : appVersionsSet) {
            if (appVer == QLatin1String("mask") || appVer == m_gameVersion) {
                return;
            }
        }

        if (!appVersionsSet.isEmpty()) {
            QStringList versionsList;
            for (const QString& version : appVersionsSet) {
                versionsList << version;
            }
            QString versions = versionsList.join(", ");
            QString message =
                QString(tr("The game is in version: %1")).arg(m_gameVersion) + "\n" +
                QString(tr("The downloaded patch only works on version: %1")).arg(versions) +
                QString("\n" + tr("You may need to update your game."));
            incompatMessages << message;
        }
    }

    if (!incompatMessages.isEmpty()) {
        QString finalMsg = incompatMessages.join("\n\n---\n\n");
        QMessageBox::information(this, tr("Incompatibility Notice"), finalMsg);
    }
}

void CheatsPatches::createFilesJson(const QString& repository) {

    QDir dir(Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    QString fullPath = dir.filePath(repository);
    if (!dir.exists(fullPath)) {
        dir.mkpath(fullPath);
    }
    dir.setPath(fullPath);

    QJsonObject filesObject;
    QStringList xmlFiles = dir.entryList(QStringList() << "*.xml", QDir::Files);

    for (QString const& xmlFile : xmlFiles) {
        QFile file(dir.filePath(xmlFile));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Error"),
                                 QString(tr("Failed to open file:") + "\n%1").arg(xmlFile));
            continue;
        }

        QXmlStreamReader xmlReader(&file);
        QJsonArray titleIdsArray;

        while (!xmlReader.atEnd() && !xmlReader.hasError()) {
            QXmlStreamReader::TokenType token = xmlReader.readNext();
            if (token == QXmlStreamReader::StartElement) {
                if (xmlReader.name() == QStringLiteral("ID")) {
                    titleIdsArray.append(xmlReader.readElementText());
                }
            }
        }

        if (xmlReader.hasError()) {
            QMessageBox::warning(this, tr("Error"),
                                 QString(tr("XML ERROR:") + "\n%1").arg(xmlReader.errorString()));
        }
        filesObject[xmlFile] = titleIdsArray;
    }

    QFile jsonFile(dir.absolutePath() + "/files.json");
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open files.json for writing"));
        return;
    }

    QJsonDocument jsonDoc(filesObject);
    jsonFile.write(jsonDoc.toJson());
    jsonFile.close();
}

int CheatsPatches::registerLocalPatches() {
    // A folder with no XML file is skipped unless it holds a files.json, which then gets emptied.
    QDir patchesDir(Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    int processed = 0;
    for (const QString& folder : patchesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir subDir(patchesDir.filePath(folder));
        if (subDir.entryList(QStringList() << "*.xml", QDir::Files).isEmpty() &&
            !subDir.exists("files.json")) {
            continue;
        }
        createFilesJson(folder);
        ++processed;
    }
    return processed;
}

void CheatsPatches::clearListCheats() {
    QLayoutItem* item;
    while ((item = ui->rightLayout->takeAt(0)) != nullptr) {
        QWidget* widget = item->widget();
        if (widget) {
            delete widget;
        } else {
            QLayout* layout = item->layout();
            if (layout) {
                QLayoutItem* innerItem;
                while ((innerItem = layout->takeAt(0)) != nullptr) {
                    QWidget* innerWidget = innerItem->widget();
                    if (innerWidget) {
                        delete innerWidget;
                    }
                    delete innerItem;
                }
                delete layout;
            }
        }
        delete item;
    }
    m_cheats.clear();
    m_cheatCheckBoxes.clear();
}

void CheatsPatches::addCheatsToLayout(const QJsonArray& modsArray, const QJsonArray& creditsArray) {
    clearListCheats();
    int maxWidthButton = 0;

    for (const QJsonValue& modValue : modsArray) {
        QJsonObject modObject = modValue.toObject();
        QString modName = modObject["name"].toString();
        QString modType = modObject["type"].toString();

        Cheat cheat;
        cheat.name = modName;
        cheat.type = modType;

        QJsonArray memoryArray = modObject["memory"].toArray();
        for (const QJsonValue& memoryValue : memoryArray) {
            QJsonObject memoryObject = memoryValue.toObject();
            MemoryMod memoryMod;
            memoryMod.offset = memoryObject["offset"].toString();
            memoryMod.on = memoryObject["on"].toString();
            memoryMod.off = memoryObject["off"].toString();
            cheat.memoryMods.append(memoryMod);
        }

        // Check for the presence of 'hint' field
        cheat.hasHint = modObject.contains("hint");

        m_cheats[modName] = cheat;

        if (modType == "checkbox") {
            QCheckBox* cheatCheckBox = new QCheckBox(modName);
            ui->rightLayout->addWidget(cheatCheckBox);
            m_cheatCheckBoxes.append(cheatCheckBox);
            connect(cheatCheckBox, &QCheckBox::toggled, this,
                    [this, modName](bool checked) { applyCheat(modName, checked); });
        } else if (modType == "button") {
            QPushButton* cheatButton = new QPushButton(modName);
            cheatButton->adjustSize();
            int buttonWidth = cheatButton->sizeHint().width();
            if (buttonWidth > maxWidthButton) {
                maxWidthButton = buttonWidth;
            }

            // Create a horizontal layout for buttons
            QHBoxLayout* buttonLayout = new QHBoxLayout();
            buttonLayout->setContentsMargins(0, 0, 0, 0);
            buttonLayout->addWidget(cheatButton);
            buttonLayout->addStretch();

            ui->rightLayout->addLayout(buttonLayout);
            connect(cheatButton, &QPushButton::clicked, this,
                    [this, modName]() { applyCheat(modName, true); });
        }
    }

    // Set minimum and fixed size for all buttons + 20
    for (int i = 0; i < ui->rightLayout->count(); ++i) {
        QLayoutItem* layoutItem = ui->rightLayout->itemAt(i);
        QWidget* widget = layoutItem->widget();
        if (widget) {
            QPushButton* button = qobject_cast<QPushButton*>(widget);
            if (button) {
                button->setMinimumWidth(maxWidthButton);
                button->setFixedWidth(maxWidthButton + 20);
            }
        } else {
            QLayout* layout = layoutItem->layout();
            if (layout) {
                for (int j = 0; j < layout->count(); ++j) {
                    QLayoutItem* innerItem = layout->itemAt(j);
                    QWidget* innerWidget = innerItem->widget();
                    if (innerWidget) {
                        QPushButton* button = qobject_cast<QPushButton*>(innerWidget);
                        if (button) {
                            button->setMinimumWidth(maxWidthButton);
                            button->setFixedWidth(maxWidthButton + 20);
                        }
                    }
                }
            }
        }
    }

    // Set credits label
    QLabel* creditsLabel = new QLabel();
    QString creditsText = tr("Author: ");
    if (!creditsArray.isEmpty()) {
        QStringList authors;
        for (const QJsonValue& credit : creditsArray) {
            authors << credit.toString();
        }
        creditsText += authors.join(", ");
    }
    creditsLabel->setText(creditsText);
    creditsLabel->setAlignment(Qt::AlignLeft);
    ui->rightLayout->addWidget(creditsLabel);
}

void CheatsPatches::populateFileListCheats() {
    clearListCheats();

    QString cheatsDir;
    Common::FS::PathToQString(cheatsDir, Common::FS::GetUserPath(Common::FS::PathType::CheatsDir));

    QString fullGameVersion = m_gameVersion;
    QString modifiedGameVersion = m_gameVersion.mid(1);

    QString patternWithFirstChar = m_gameSerial + "_" + fullGameVersion + "*.json";
    QString patternWithoutFirstChar = m_gameSerial + "_" + modifiedGameVersion + "*.json";

    QDir dir(cheatsDir);
    QStringList filters;
    filters << patternWithFirstChar << patternWithoutFirstChar;
    dir.setNameFilters(filters);

    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
    QStringList fileNames;

    for (const QFileInfo& fileInfo : fileList) {
        fileNames << fileInfo.fileName();
    }

    QStringListModel* model = new QStringListModel(fileNames, this);
    ui->listView_selectFile->setModel(model);

    connect(ui->listView_selectFile->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() {
                QModelIndexList selectedIndexes =
                    ui->listView_selectFile->selectionModel()->selectedIndexes();
                if (!selectedIndexes.isEmpty()) {

                    QString selectedFileName = selectedIndexes.first().data().toString();
                    QString cheatsDir;
                    Common::FS::PathToQString(
                        cheatsDir, Common::FS::GetUserPath(Common::FS::PathType::CheatsDir));

                    QFile file(cheatsDir + "/" + selectedFileName);
                    if (file.open(QIODevice::ReadOnly)) {
                        QByteArray jsonData = file.readAll();
                        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
                        QJsonObject jsonObject = jsonDoc.object();
                        QJsonArray modsArray = jsonObject["mods"].toArray();
                        QJsonArray creditsArray = jsonObject["credits"].toArray();
                        addCheatsToLayout(modsArray, creditsArray);
                    }
                }
            });

    if (!fileNames.isEmpty()) {
        QModelIndex firstIndex = model->index(0, 0);
        ui->listView_selectFile->selectionModel()->select(
            firstIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        ui->listView_selectFile->setCurrentIndex(firstIndex);
    }
}

void CheatsPatches::addPatchesToLayout(const QString& filePath) {
    if (filePath == "") {
        return;
    }
    QString folderPath = filePath.section(" | ", 1, 1);

    // Clear existing layout items
    QLayoutItem* item;
    while ((item = ui->patchesGroupBoxLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_patchInfos.clear();
    m_patchCheckBoxes.clear();

    QDir dir(Common::FS::GetUserPath(Common::FS::PathType::PatchesDir));
    QString fullPath = dir.filePath(folderPath);

    if (!dir.exists(fullPath)) {
        QMessageBox::warning(this, tr("Error"),
                             QString(tr("Directory does not exist:") + "\n%1").arg(fullPath));
        return;
    }
    dir.setPath(fullPath);

    QString filesJsonPath = dir.filePath("files.json");

    QFile jsonFile(filesJsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open files.json for reading."));
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = jsonDoc.object();

    bool patchAdded = false;
    QStringList failedFiles;
    // Patches with a Category attribute go into collapsible sections, one per level of the path,
    // each placed where it first appears; the others stay in file order. Keyed by the path.
    QMap<QString, PatchSection> sections;

    // Iterate over each entry in the JSON file
    for (auto it = jsonObject.constBegin(); it != jsonObject.constEnd(); ++it) {
        QString xmlFileName = it.key();
        QJsonArray idsArray = it.value().toArray();

        // Check if the serial is in the ID list
        if (idsArray.contains(QJsonValue(m_gameSerial))) {
            QString xmlFilePath = dir.filePath(xmlFileName);
            QFile xmlFile(xmlFilePath);

            if (!xmlFile.open(QIODevice::ReadOnly)) {
                failedFiles << xmlFile.fileName();
                continue;
            }
            QXmlStreamReader xmlReader(&xmlFile);
            QString patchName;
            QString patchAuthor;
            QString patchNote;
            QString patchCategory;
            QJsonArray patchLines;
            bool isEnabled = false;

            while (!xmlReader.atEnd() && !xmlReader.hasError()) {
                xmlReader.readNext();

                if (xmlReader.tokenType() == QXmlStreamReader::StartElement) {
                    if (xmlReader.name() == QStringLiteral("Metadata")) {
                        QXmlStreamAttributes attributes = xmlReader.attributes();
                        QString appVer = attributes.value("AppVer").toString();
                        if (appVer == m_gameVersion) {
                            patchName = attributes.value("Name").toString();
                            patchAuthor = attributes.value("Author").toString();
                            patchNote = attributes.value("Note").toString();
                            patchCategory = attributes.value("Category").toString();
                            isEnabled =
                                attributes.value("isEnabled").toString() == QStringLiteral("true");
                        }
                        if (appVer == "mask") {
                            patchName = attributes.value("Name").toString() + " (any version)";
                            patchAuthor = attributes.value("Author").toString();
                            patchNote = attributes.value("Note").toString();
                            patchCategory = attributes.value("Category").toString();
                            isEnabled =
                                attributes.value("isEnabled").toString() == QStringLiteral("true");
                        }
                    } else if (xmlReader.name() == QStringLiteral("PatchList")) {
                        QJsonArray linesArray;
                        while (!xmlReader.atEnd() &&
                               !(xmlReader.tokenType() == QXmlStreamReader::EndElement &&
                                 xmlReader.name() == QStringLiteral("PatchList"))) {
                            xmlReader.readNext();
                            if (xmlReader.tokenType() == QXmlStreamReader::StartElement &&
                                xmlReader.name() == QStringLiteral("Line")) {
                                QXmlStreamAttributes attributes = xmlReader.attributes();
                                QJsonObject lineObject;
                                lineObject["Type"] = attributes.value("Type").toString();
                                lineObject["Address"] = attributes.value("Address").toString();
                                lineObject["Value"] = attributes.value("Value").toString();
                                linesArray.append(lineObject);
                            }
                        }
                        patchLines = linesArray;
                    }
                }

                if (!patchName.isEmpty() && !patchLines.isEmpty()) {
                    QCheckBox* patchCheckBox = new QCheckBox(patchName);
                    patchCheckBox->setProperty("patchName", patchName);
                    patchCheckBox->setChecked(isEnabled);
                    const QList<PatchSection> chain =
                        findOrAddSections(sections, ui->patchesGroupBoxLayout, patchCategory);
                    if (chain.isEmpty()) {
                        ui->patchesGroupBoxLayout->addWidget(patchCheckBox);
                    } else {
                        addPatchToSections(chain, patchCheckBox);
                    }
                    if (!m_patchCheckBoxes.contains(patchName)) {
                        m_patchCheckBoxes.insert(patchName, patchCheckBox);
                    }

                    PatchInfo patchInfo;
                    patchInfo.name = patchName;
                    patchInfo.author = patchAuthor;
                    patchInfo.note = patchNote;
                    patchInfo.linesArray = patchLines;
                    patchInfo.serial = m_gameSerial;
                    m_patchInfos[patchName] = patchInfo;

                    patchCheckBox->installEventFilter(this);

                    connect(patchCheckBox, &QCheckBox::toggled, this,
                            [this, patchName](bool checked) { applyPatch(patchName, checked); });

                    patchName.clear();
                    patchAuthor.clear();
                    patchNote.clear();
                    patchCategory.clear();
                    patchLines = QJsonArray();
                    patchAdded = true;
                }
            }
            xmlFile.close();
        }
    }

    // A section starts folded unless one of its patches, nested ones included, is enabled.
    for (const PatchSection& section : std::as_const(sections)) {
        updatePatchSectionHeader(section);
        section.header->setChecked(countCheckedPatches(section) > 0);
    }

    // Widgets join the end of the window's focus chain as they are added, in file order. Tab
    // follows the screen order instead, which differs when a section reappears later in the file.
    QList<QWidget*> focusOrder;
    collectPatchFocusOrder(ui->patchesGroupBoxLayout, focusOrder);
    for (qsizetype i = 1; i < focusOrder.size(); ++i) {
        QWidget::setTabOrder(focusOrder[i - 1], focusOrder[i]);
    }

    // Remove the item from the list view if no patches were added
    // (the game has patches, but not for the current version)
    if (!patchAdded) {
        QStringListModel* model = qobject_cast<QStringListModel*>(ui->patchesListView->model());

        if (model) {
            QStringList items = model->stringList();
            int index = items.indexOf(filePath);
            if (index != -1) {
                items.removeAt(index);
                model->setStringList(items);
            }
        }
    }

    // QMessageBox runs a nested event loop in which a finished download can rebuild the list, so
    // the warning waits until the section widgets are no longer used.
    if (!failedFiles.isEmpty()) {
        QMessageBox::warning(
            this, tr("Error"),
            QString(tr("Failed to open file:") + "\n%1").arg(failedFiles.join("\n")));
    }
}

void CheatsPatches::updateNoteTextEdit(const QString& patchName) {
    if (m_patchInfos.contains(patchName)) {
        const PatchInfo& patchInfo = m_patchInfos[patchName];
        QString text = QString(tr("Name:") + " %1\n" + tr("Author: ") + "%2\n\n%3")
                           .arg(patchInfo.name)
                           .arg(patchInfo.author)
                           .arg(patchInfo.note);

        for (QJsonValue const& value : patchInfo.linesArray) {
            QJsonObject lineObject = value.toObject();
            QString type = lineObject["Type"].toString();
            QString address = lineObject["Address"].toString();
            QString patchValue = lineObject["Value"].toString();
        }
        text.replace("\\n", "\n");
        ui->instructionsTextEdit->setText(text);
    }
}

bool showErrorMessage = true;
void CheatsPatches::uncheckAllCheatCheckBoxes() {
    for (auto& cheatCheckBox : m_cheatCheckBoxes) {
        cheatCheckBox->setChecked(false);
    }
    showErrorMessage = true;
}

void CheatsPatches::applyCheat(const QString& modName, bool enabled) {
    if (!m_cheats.contains(modName))
        return;

    if (!EmulatorState::GetInstance()->IsGameRunning() && enabled) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Can't apply cheats before the game is started"));
        uncheckAllCheatCheckBoxes();
        return;
    }

    Cheat cheat = m_cheats[modName];

    for (const MemoryMod& memoryMod : cheat.memoryMods) {
        QString value = enabled ? memoryMod.on : memoryMod.off;

        std::string modNameStr = modName.toStdString();
        std::string offsetStr = memoryMod.offset.toStdString();
        std::string valueStr = value.toStdString();

        if (!EmulatorState::GetInstance()->IsGameRunning())
            return;

        // Determine if the hint field is present
        bool isHintPresent = m_cheats[modName].hasHint;

        m_ipc_client->sendMemoryPatches(modNameStr, offsetStr, valueStr, "", "", !isHintPresent,
                                        false);
    }
}

void CheatsPatches::applyPatch(const QString& patchName, bool enabled) {
    if (!enabled)
        return;
    if (m_patchInfos.contains(patchName)) {
        const PatchInfo& patchInfo = m_patchInfos[patchName];

        for (QJsonValue const& value : patchInfo.linesArray) {
            QJsonObject lineObject = value.toObject();
            QString type = lineObject["Type"].toString();
            QString address = lineObject["Address"].toString();
            QString patchValue = lineObject["Value"].toString();
            QString maskOffsetStr = lineObject["Offset"].toString();

            patchValue = QString::fromStdString(
                MemoryPatcher::convertValueToHex(type.toStdString(), patchValue.toStdString()));

            bool littleEndian = false;

            if (type == "bytes16" || type == "bytes32" || type == "bytes64") {
                littleEndian = true;
            }

            MemoryPatcher::PatchMask patchMask = MemoryPatcher::PatchMask::None;
            int maskOffsetValue = 0;

            if (type == "mask")
                patchMask = MemoryPatcher::PatchMask::Mask;

            if (type == "mask_jump32")
                patchMask = MemoryPatcher::PatchMask::Mask_Jump32;

            if ((type == "mask" || type == "mask_jump32") && !maskOffsetStr.toStdString().empty()) {
                maskOffsetValue = std::stoi(maskOffsetStr.toStdString(), 0, 10);
            }

            /* if (MemoryPatcher::g_eboot_address == 0) {
                MemoryPatcher::patchInfo addingPatch;
                addingPatch.gameSerial = patchInfo.serial.toStdString();
                addingPatch.modNameStr = patchName.toStdString();
                addingPatch.offsetStr = address.toStdString();
                addingPatch.valueStr = patchValue.toStdString();
                addingPatch.isOffset = false;
                addingPatch.littleEndian = littleEndian;
                addingPatch.patchMask = patchMask;
                addingPatch.maskOffset = maskOffsetValue;

                MemoryPatcher::AddPatchToQueue(addingPatch);
                continue;
            }
            MemoryPatcher::PatchMemory(patchName.toStdString(), address.toStdString(),
                                       patchValue.toStdString(), "", "", false, littleEndian,
                                       patchMask); */
        }
    }
}

bool CheatsPatches::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(obj);
        if (checkBox) {
            bool hovered = event->type() == QEvent::HoverEnter;
            onPatchCheckBoxHovered(checkBox, hovered);
            return true;
        }
    }
    // Pass the event on to base class
    return QWidget::eventFilter(obj, event);
}

void CheatsPatches::onPatchCheckBoxHovered(QCheckBox* checkBox, bool hovered) {
    if (hovered) {
        const auto patchName = checkBox->property("patchName");
        if (patchName.isValid()) {
            updateNoteTextEdit(patchName.toString());
        }
    } else {
        ui->instructionsTextEdit->setText(defaultTextEdit_MSG);
    }
}
