// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <string>
#include <system_error>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

#include "common/path_util.h"
#include "core/emulator_settings.h"
#include "core/user_settings.h"
#include "profile_widget.h"

namespace {
constexpr int Diameter = 36;
} // namespace

ProfileWidget::ProfileWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 12, 6, 4);
    layout->setSpacing(3);

    m_avatar_button = new QPushButton(this);
    m_avatar_button->setFlat(true);
    m_avatar_button->setCursor(Qt::PointingHandCursor);
    m_avatar_button->setFixedSize(Diameter, Diameter);
    m_avatar_button->setIconSize(QSize(Diameter, Diameter));
    m_avatar_button->setStyleSheet(
        QStringLiteral("QPushButton { border: none; background: transparent; }"));
    connect(m_avatar_button, &QPushButton::clicked, this, &ProfileWidget::OnAvatarClicked);

    m_name_label = new QLabel(this);

    layout->addWidget(m_avatar_button, 0, Qt::AlignHCenter);
    layout->addWidget(m_name_label, 0, Qt::AlignHCenter);

    if (parent) {
        parent->window()->installEventFilter(this);
    }
    Refresh();
}

bool ProfileWidget::eventFilter(QObject* obj, QEvent* event) {
    // A game's settings dialog edits the account and closes back into this window.
    if (event->type() == QEvent::WindowActivate) {
        Refresh();
    }
    return QWidget::eventFilter(obj, event);
}

void ProfileWidget::Refresh() {
    // The emulator uploads the picture of the same user from the same file.
    const User* player = UserManagement.GetUserByPlayerIndex(1);
    QImage picture;
    if (player) {
        m_avatar_path =
            EmulatorSettings.GetHomeDir() / std::to_string(player->user_id) / "gr2_avatar.png";
        QString path;
        Common::FS::PathToQString(path, m_avatar_path);
        picture.load(path);
        // Gravity Rush 2 names the player by the Online ID once signed in, otherwise by user name.
        const bool signs_in = EmulatorSettings.IsShadNetEnabled() && player->shadnet_enabled &&
                              !player->shadnet_npid.empty() && !player->shadnet_password.empty();
        m_name_label->setText(
            QString::fromStdString(signs_in ? player->shadnet_npid : player->user_name));
        m_avatar_button->setToolTip(
            tr("Click to change your profile picture.\nGravity Rush 2 sends it to its server the "
               "next time the game goes online signed in to shadNet."));
    } else {
        m_name_label->setText(tr("No player 1"));
        m_avatar_button->setToolTip(tr("No user is on controller port 1."));
    }
    m_avatar_button->setEnabled(player != nullptr);
    if (picture.isNull()) {
        picture.load(QStringLiteral(":images/avatar_default.png"));
    }

    // Drawn at the size of the saved picture; the icon scales it down for each screen.
    const QSize size(128, 128);
    const QImage scaled =
        picture.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap circle(size);
    circle.fill(Qt::transparent);
    QPainter painter(&circle);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(QRectF(QPointF(), size));
    painter.setClipPath(clip);
    painter.drawImage((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2,
                      scaled);
    painter.end();
    m_avatar_button->setIcon(circle);
}

void ProfileWidget::OnAvatarClicked() {
    const QString source =
        QFileDialog::getOpenFileName(this, tr("Select Profile Picture"), QDir::homePath(),
                                     tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (source.isEmpty()) {
        return;
    }
    QImageReader reader(source);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Profile Picture"),
                             tr("The image could not be loaded:\n%1").arg(reader.errorString()));
        return;
    }

    // The center square at 128x128 in 8-bit RGBA, as the Gravity Rush 2 server stores it. The file
    // must be a PNG: a server without Pillow stores the upload as sent and serves only PNG files.
    const int side = std::min(image.width(), image.height());
    const QImage avatar =
        image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
            .scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32);
    QString target;
    Common::FS::PathToQString(target, m_avatar_path);
    std::error_code ec;
    std::filesystem::create_directories(m_avatar_path.parent_path(), ec);
    // Replaces the old picture only once the new one is complete.
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || !avatar.save(&file, "PNG") || !file.commit()) {
        QMessageBox::warning(this, tr("Profile Picture"),
                             tr("Failed to save the profile picture to:\n%1").arg(target));
        return;
    }
    Refresh();
}
