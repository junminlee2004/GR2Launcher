// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QTemporaryDir>

#include "common/path_util.h"
#include "dlss_download.h"

namespace {

// NVIDIA Streamline 2.14.16 with DLSS 310.9.16, the release the emulator is built for.
constexpr const char* ReleaseUrl = "https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16";
constexpr const char* ZipUrl =
    "https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.16/"
    "streamline-sdk-v2.14.16.zip";
constexpr qint64 ZipSize = 276016170;
constexpr const char* ZipSha256 =
    "32041e51db0cab709a271b72a352281b690c2abbd669a827809401a5dd23956b";
constexpr std::array<const char*, 4> FileNames = {"sl.interposer.dll", "sl.common.dll",
                                                  "sl.dlss.dll", "nvngx_dlss.dll"};
constexpr const char* StreamlineLicenseUrl =
    "https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/license.txt";
constexpr const char* DlssLicenseUrl = "https://github.com/NVIDIA/DLSS/blob/v310.9.16/LICENSE.txt";

// Extracts the files of bin/x64 named by the arguments from streamline.zip next to the script.
constexpr const char* ExtractScript = R"(
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead("$PSScriptRoot\streamline.zip")
foreach ($name in $args) {
    $entry = $zip.GetEntry("bin/x64/$name")
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, "$PSScriptRoot\$name")
}
)";

// Downloads the release into the file zipPath. Returns the reason of a failure, or an empty text.
QString Download(const QString& zipPath, QWidget* parent) {
    QFile zip(zipPath);
    if (!zip.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        return zip.errorString();
    }
    QProgressDialog dialog(DlssDownload::tr("Downloading NVIDIA's Streamline 2.14.16 files..."),
                           DlssDownload::tr("Cancel"), 0, static_cast<int>(ZipSize), parent);
    dialog.setWindowTitle(DlssDownload::tr("DLSS"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumDuration(0);
    dialog.setValue(0);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(QNetworkRequest(QUrl(ZipUrl)));
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&] {
        const QByteArray data = reply->readAll();
        if (zip.write(data) != data.size()) {
            reply->abort();
        }
        hash.addData(data);
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, &dialog,
                     [&dialog](qint64 received) { dialog.setValue(static_cast<int>(received)); });
    QObject::connect(&dialog, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (zip.error() != QFileDevice::NoError) {
        return zip.errorString();
    }
    if (reply->error() != QNetworkReply::NoError) {
        return reply->errorString();
    }
    if (hash.result().toHex() != ZipSha256) {
        return DlssDownload::tr("The download does not match NVIDIA's release (HTTP status %1, %2 "
                                "of %3 bytes).")
            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
            .arg(zip.size())
            .arg(ZipSize);
    }
    return {};
}

// Extracts the files from the release downloaded into temp and moves them into dir. Returns the
// reason of a failure, or an empty text.
QString Extract(const QTemporaryDir& temp, const QDir& dir) {
    QFile script(temp.filePath("extract.ps1"));
    if (!script.open(QIODevice::WriteOnly)) {
        return script.errorString();
    }
    script.write(ExtractScript);
    script.close();

    QStringList args{"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script.fileName()};
    for (const char* name : FileNames) {
        args << name;
    }
    QProcess process;
    process.start("powershell.exe", args);
    if (!process.waitForFinished() || process.exitCode() != 0) {
        const QString error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return error.isEmpty() ? process.errorString() : error;
    }
    for (const char* name : FileNames) {
        QFile::remove(dir.filePath(name));
        QFile file(temp.filePath(name));
        if (!file.rename(dir.filePath(name))) {
            return file.errorString();
        }
    }
    return {};
}

} // Anonymous namespace

bool DlssDownload::EnsureFiles(QWidget* parent) {
    QString path;
    Common::FS::PathToQString(path,
                              Common::FS::GetUserPath(Common::FS::PathType::UserDir) / "dlss");
    const QDir dir(path);
    if (std::ranges::all_of(FileNames, [&dir](const char* name) { return dir.exists(name); })) {
        return true;
    }
    const QString question =
        tr("<p>DLSS needs four files from NVIDIA's Streamline 2.14.16 release in the dlss folder "
           "of the user folder, and some of them are missing. Download them now?</p>"
           "<p>This downloads %1 MiB from NVIDIA's GitHub page, <a href=\"%2\">%2</a>, and keeps "
           "only the four files.</p>"
           "<p>Streamline license: <a href=\"%3\">%3</a><br>"
           "DLSS license: <a href=\"%4\">%4</a></p>"
           "<p>By downloading you accept NVIDIA's license terms.</p>")
            .arg(ZipSize / (1024 * 1024))
            .arg(ReleaseUrl)
            .arg(StreamlineLicenseUrl)
            .arg(DlssLicenseUrl);
    if (QMessageBox::question(parent, tr("DLSS"), question) != QMessageBox::Yes) {
        return false;
    }

    // The download and the extracted files stay in a temporary folder, removed on return, until
    // each file is complete.
    dir.mkpath(".");
    const QTemporaryDir temp(dir.filePath("download-XXXXXX"));
    QString error =
        temp.isValid() ? Download(temp.filePath("streamline.zip"), parent) : temp.errorString();
    if (error.isEmpty()) {
        error = Extract(temp, dir);
    }
    if (error.isEmpty()) {
        return true;
    }
    QMessageBox::warning(parent, tr("DLSS"),
                         tr("The DLSS files were not installed: %1").arg(error));
    return false;
}
