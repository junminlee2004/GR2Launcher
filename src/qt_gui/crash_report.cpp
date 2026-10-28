// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "common/scm_rev.h"
#include "crash_report.h"
#include "gui_settings.h"

namespace {

// GitHub App of the reports. Its user tokens write the issues of the repositories it is installed
// on and the gists of the user who signed in.
constexpr const char* ClientId = "Iv23li5mOfrUyIQgFvKL";
constexpr const char* Repository = "junminlee2004/GR2fork";

// The emulator logs these at shutdown, and other threads may log a few lines after them.
constexpr std::array<const char*, 2> ShutdownLines = {"UpdatePlayTime: Playing time for",
                                                      "Close: Cache dumped"};
constexpr qsizetype ShutdownSearchLines = 20;
constexpr const char* StartupLine = "Run: Starting shadps4 emulator";

// Header lines the issue lists, found by the text before their value.
constexpr std::array<const char*, 9> SummaryLines = {
    "Run: Starting shadps4 emulator",
    "Run: Revision",
    "Run: Branch",
    "Run: Game id:",
    "Run: Operating System:",
    "Run: CPU Model:",
    "Run: Total RAM:",
    "CollectDeviceParameters: GPU_Model:",
    "CollectDeviceParameters: GPU_Vulkan_Driver:",
};

constexpr qint64 HeadBytes = 256 * 1024;
constexpr qint64 EndBytes = 64 * 1024;
// A longer log goes into the gist as its first HeadBytes and its end.
constexpr qint64 MaxUploadBytes = 4 * 1024 * 1024;
constexpr qsizetype IssueTailLines = 100;
// The body of an issue holds at most 65536 characters.
constexpr qsizetype IssueTailChars = 40000;

struct Token {
    QString access;
    QString refresh;
    qint64 expires_at{}; ///< Seconds since the epoch, 0 for a token that does not expire.
};

struct Response {
    int status{}; ///< HTTP status, 0 when no answer arrived.
    QJsonObject json;
    QString error; ///< Why no answer arrived.
};

QString ReadAt(QFile& file, qint64 offset, qint64 size) {
    file.seek(offset);
    return QString::fromUtf8(file.read(size));
}

// Replaces the home folder, which holds the user name, in text.
QString Scrub(QString text) {
    const QString home = QDir::homePath();
    for (const QString& path : {QDir::toNativeSeparators(home), home}) {
        text.replace(path, QStringLiteral("<home>"), Qt::CaseInsensitive);
    }
    return text;
}

Token LoadToken(const gui_settings& settings) {
    return {settings.GetValue(gui::gen_githubToken).toString(),
            settings.GetValue(gui::gen_githubRefreshToken).toString(),
            settings.GetValue(gui::gen_githubTokenExpiry).toLongLong()};
}

void StoreToken(const gui_settings& settings, const Token& token) {
    settings.SetValue(gui::gen_githubToken, token.access, false);
    settings.SetValue(gui::gen_githubRefreshToken, token.refresh, false);
    settings.SetValue(gui::gen_githubTokenExpiry, token.expires_at);
}

// The token of an access_token answer; access is empty when the answer holds none.
Token TokenOf(const QJsonObject& json) {
    const qint64 expires_in = json.value("expires_in").toInteger();
    return {json.value("access_token").toString(), json.value("refresh_token").toString(),
            expires_in > 0 ? QDateTime::currentSecsSinceEpoch() + expires_in : 0};
}

QNetworkRequest Request(const QString& url) {
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GR2Launcher"));
    return request;
}

// GitHub's sign-in endpoints read form fields and answer in JSON.
QNetworkRequest OAuthRequest(const QString& path) {
    QNetworkRequest request = Request("https://github.com/login/" + path);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Accept", "application/json");
    return request;
}

QNetworkRequest ApiRequest(const QString& path, const Token& token) {
    QNetworkRequest request = Request("https://api.github.com/" + path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("Authorization", "Bearer " + token.access.toUtf8());
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    return request;
}

QByteArray Form(std::initializer_list<std::pair<QString, QString>> fields) {
    QUrlQuery query;
    for (const auto& [name, value] : fields) {
        query.addQueryItem(name, value);
    }
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

Response ResponseOf(QNetworkReply* reply) {
    Response response;
    response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.json = QJsonDocument::fromJson(reply->readAll()).object();
    if (response.status == 0) {
        response.error = reply->errorString();
    }
    return response;
}

// Posts body and waits for the answer. Cancelling progress aborts the request.
Response Post(QNetworkAccessManager& manager, const QNetworkRequest& request,
              const QByteArray& body, QProgressDialog* progress) {
    QNetworkReply* reply = manager.post(request, body);
    if (progress) {
        QObject::connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    }
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const Response response = ResponseOf(reply);
    reply->deleteLater();
    return response;
}

QString ErrorOf(const Response& response) {
    if (response.status == 0) {
        return response.error;
    }
    QString message = response.json.value("message").toString();
    if (message.isEmpty()) {
        message = response.json.value("error_description").toString();
    }
    if (message.isEmpty()) {
        message = response.json.value("error").toString();
    }
    return message.isEmpty()
               ? QStringLiteral("HTTP %1").arg(response.status)
               : QStringLiteral("%1 (HTTP %2)").arg(message, QString::number(response.status));
}

// Trades the refresh token for a new token; access is empty when GitHub refuses it.
Token Refresh(QNetworkAccessManager& manager, const Token& token) {
    const Response response = Post(manager, OAuthRequest("oauth/access_token"),
                                   Form({{"client_id", ClientId},
                                         {"grant_type", "refresh_token"},
                                         {"refresh_token", token.refresh}}),
                                   nullptr);
    return TokenOf(response.json);
}

// Signs the user in with GitHub's device flow. Returns the token, or an empty access token with
// error set; error stays empty when the user cancels.
Token SignIn(QWidget* parent, QNetworkAccessManager& manager, QString& error) {
    const Response device =
        Post(manager, OAuthRequest("device/code"), Form({{"client_id", ClientId}}), nullptr);
    const QString user_code = device.json.value("user_code").toString();
    const QString device_code = device.json.value("device_code").toString();
    const QUrl verification_url(device.json.value("verification_uri").toString());
    if (user_code.isEmpty() || device_code.isEmpty()) {
        error = ErrorOf(device);
        return {};
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(CrashReport::tr("Sign in to GitHub"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QLabel(CrashReport::tr("To send the report, sign in to GitHub on the page "
                                            "that opened, %1, and enter this code. The code is "
                                            "also on the clipboard.")
                                .arg(verification_url.toString()),
                            &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);
    auto* code = new QLabel(user_code, &dialog);
    code->setStyleSheet(QStringLiteral("font-size: 20pt; font-weight: bold;"));
    code->setAlignment(Qt::AlignCenter);
    code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(code);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QPushButton* open =
        buttons->addButton(CrashReport::tr("Open GitHub"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(open, &QPushButton::clicked, &dialog,
                     [verification_url] { QDesktopServices::openUrl(verification_url); });

    // GitHub answers authorization_pending until the user enters the code, and slow_down with a
    // longer interval when polled too often.
    Token token;
    int interval = std::max(device.json.value("interval").toInt(5), 1);
    QTimer timer;
    timer.setSingleShot(true);
    QPointer<QNetworkReply> pending;
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] {
        QNetworkReply* reply =
            manager.post(OAuthRequest("oauth/access_token"),
                         Form({{"client_id", ClientId},
                               {"device_code", device_code},
                               {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}}));
        pending = reply;
        QObject::connect(reply, &QNetworkReply::finished, &dialog, [&, reply] {
            reply->deleteLater();
            const Response response = ResponseOf(reply);
            token = TokenOf(response.json);
            const QString state = response.json.value("error").toString();
            if (!token.access.isEmpty()) {
                dialog.accept();
            } else if (state == "authorization_pending" || state == "slow_down") {
                interval = response.json.value("interval").toInt(interval);
                timer.start(interval * 1000);
            } else {
                error = ErrorOf(response);
                dialog.reject();
            }
        });
    });
    QGuiApplication::clipboard()->setText(user_code);
    QDesktopServices::openUrl(verification_url);
    timer.start(interval * 1000);
    dialog.exec();
    timer.stop();
    if (pending) {
        pending->disconnect(&dialog);
        pending->abort();
    }
    return token;
}

} // Anonymous namespace

std::optional<CrashReport> CrashReport::Check(const QString& log_path, qint64 size_limit,
                                              const QString& game) {
    QFile file(log_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() >= size_limit) {
        return std::nullopt;
    }
    const qint64 size = file.size();
    const QString head = ReadAt(file, 0, std::min(size, HeadBytes));
    if (!head.contains(QLatin1String(StartupLine))) {
        return std::nullopt;
    }
    const QString end = ReadAt(file, std::max<qint64>(size - EndBytes, 0), EndBytes);
    const QStringList end_lines = end.split('\n', Qt::SkipEmptyParts);
    for (qsizetype i = std::max<qsizetype>(end_lines.size() - ShutdownSearchLines, 0);
         i < end_lines.size(); ++i) {
        for (const char* line : ShutdownLines) {
            if (end_lines[i].contains(QLatin1String(line))) {
                return std::nullopt;
            }
        }
    }

    CrashReport report;
    report.file_name = QFileInfo(log_path).fileName();
    report.game = game;
    if (size <= MaxUploadBytes) {
        report.content = Scrub(ReadAt(file, 0, size));
    } else {
        const qint64 end_size = MaxUploadBytes - HeadBytes;
        report.content = Scrub(head +
                               QStringLiteral("\n[... %1 MiB of the log left out ...]\n")
                                   .arg((size - MaxUploadBytes) / (1024 * 1024)) +
                               ReadAt(file, size - end_size, end_size));
    }

    QStringList tail;
    qsizetype chars = 0;
    for (qsizetype i = end_lines.size() - 1; i >= 0 && tail.size() < IssueTailLines; --i) {
        chars += end_lines[i].size() + 1;
        if (chars > IssueTailChars) {
            break;
        }
        tail.prepend(end_lines[i]);
    }
    report.tail = Scrub(tail.join('\n'));

    const QStringList head_lines = head.split('\n');
    QStringList summary;
    for (const char* marker : SummaryLines) {
        const QString key = QString::fromLatin1(marker);
        for (const QString& line : head_lines) {
            if (const qsizetype index = line.indexOf(key); index >= 0) {
                summary << line.mid(index + key.indexOf(' ') + 1).trimmed();
                break;
            }
        }
    }
    report.summary = summary.join('\n');
    return report;
}

void CrashReport::Offer(QWidget* parent, gui_settings& settings) const {
    const QString issues_url = QStringLiteral("https://github.com/%1/issues").arg(Repository);
    const QString question =
        tr("<p>Based on your log, your game may have ended abruptly. Would you like to submit "
           "your log to GitHub?</p>"
           "<p>The log is posted in a public issue at <a href=\"%1\">%1</a>. The first time, you "
           "sign in with your GitHub account.</p>")
            .arg(issues_url);
    if (QMessageBox::question(parent, tr("Game ended abruptly"), question) != QMessageBox::Yes) {
        return;
    }

    QNetworkAccessManager manager;
    Token token = LoadToken(settings);
    if (token.expires_at != 0 && QDateTime::currentSecsSinceEpoch() + 60 >= token.expires_at) {
        token = token.refresh.isEmpty() ? Token{} : Refresh(manager, token);
        if (!token.access.isEmpty()) {
            StoreToken(settings, token);
        }
    }

    // A token GitHub no longer accepts is replaced by a new sign-in once.
    QString error;
    QString issue_url;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (token.access.isEmpty()) {
            error.clear();
            token = SignIn(parent, manager, error);
            if (token.access.isEmpty()) {
                break;
            }
            StoreToken(settings, token);
        }

        QProgressDialog progress(tr("Sending the report..."), tr("Cancel"), 0, 0, parent);
        progress.setWindowTitle(tr("Game ended abruptly"));
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.show();

        const QJsonObject gist{
            {"description", QStringLiteral("%1 log").arg(game)},
            {"public", false},
            {"files", QJsonObject{{file_name, QJsonObject{{"content", content}}}}},
        };
        const Response gist_response =
            Post(manager, ApiRequest("gists", token),
                 QJsonDocument(gist).toJson(QJsonDocument::Compact), &progress);
        if (progress.wasCanceled()) {
            return;
        }
        if (gist_response.status == 401) {
            error = ErrorOf(gist_response);
            token = {};
            StoreToken(settings, token);
            continue;
        }
        const QString gist_url = gist_response.json.value("html_url").toString();

        QString body = QStringLiteral("The launcher found none of the emulator's shutdown lines "
                                      "at the end of this log, so the game may have ended "
                                      "abruptly.\n\nLauncher: GR2Launcher v%1 %2 %3\n\n")
                           .arg(QString::fromUtf8(Common::g_scm_app_version),
                                QString::fromUtf8(Common::g_scm_branch),
                                QString::fromUtf8(Common::g_scm_desc));
        body += QStringLiteral("~~~\n%1\n~~~\n\n").arg(summary);
        body += gist_url.isEmpty() ? QStringLiteral("The full log could not be uploaded: %1\n\n")
                                         .arg(ErrorOf(gist_response))
                                   : QStringLiteral("Full log: %1\n\n").arg(gist_url);
        body += QStringLiteral("<details><summary>Last lines of %1</summary>\n\n~~~\n%2\n~~~\n\n"
                               "</details>\n")
                    .arg(file_name, tail);
        const QJsonObject issue{
            {"title", QStringLiteral("Game ended abruptly: %1").arg(game)},
            {"body", body},
        };
        const Response issue_response =
            Post(manager, ApiRequest(QStringLiteral("repos/%1/issues").arg(Repository), token),
                 QJsonDocument(issue).toJson(QJsonDocument::Compact), &progress);
        if (progress.wasCanceled()) {
            return;
        }
        if (issue_response.status == 401) {
            error = ErrorOf(issue_response);
            token = {};
            StoreToken(settings, token);
            continue;
        }
        issue_url = issue_response.json.value("html_url").toString();
        error = issue_url.isEmpty() ? ErrorOf(issue_response) : QString();
        break;
    }

    if (!issue_url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(issue_url));
        QMessageBox::information(
            parent, tr("Game ended abruptly"),
            tr("<p>Thank you. Your report is at <a href=\"%1\">%1</a>.</p>").arg(issue_url));
    } else if (!error.isEmpty()) {
        QMessageBox::warning(parent, tr("Game ended abruptly"),
                             tr("The report was not sent: %1").arg(error));
    }
}
