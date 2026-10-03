// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/emulator_settings.h"
#include "tuning_page.h"

namespace {
constexpr const char* Section = "GPU";
}

TuningPage::TuningPage(QWidget* parent) : QWidget(parent) {
    auto* page = new QVBoxLayout(this);
    auto* warning = new QLabel(
        tr("WARNING: These settings tune the renderer and the threads of the emulator and are "
           "meant for testing. A change takes effect the next time a game starts. Reset Tuning to "
           "Defaults shows the defaults of this build; Save or Apply writes them."));
    warning->setWordWrap(true);
    page->addWidget(warning);

    const nlohmann::json model = EmulatorSettings.GetGroupValues(Section);
    std::vector<Group> groups = Table();
    nlohmann::json unnamed = model;
    for (const Group& group : groups) {
        for (const Row& row : group.rows) {
            unnamed.erase(row.key);
        }
    }
    groups.push_back({tr("Other"), true, {}});
    for (const auto& [key, value] : unnamed.items()) {
        groups.back().rows.push_back(
            {key, QString::fromStdString(key), tr("This launcher has no description of it.")});
    }

    for (const Group& group : groups) {
        if (group.title.isEmpty()) {
            outside.insert(outside.end(), group.rows.begin(), group.rows.end());
            continue;
        }
        auto* content = new QWidget;
        auto* lines = new QVBoxLayout(content);
        const size_t shown = rows.size();
        for (Row row : group.rows) {
            if (!model.contains(row.key)) {
                continue;
            }
            const nlohmann::json& value = model.at(row.key);
            if (value.is_boolean()) {
                auto* check = new QCheckBox(row.label);
                connect(check, &QCheckBox::toggled, this, &TuningPage::UpdateEnabled);
                row.line = row.editor = check;
            } else if (value.is_number_unsigned()) {
                if (row.choices.empty()) {
                    auto* spin = new QSpinBox;
                    spin->setRange(0, row.max);
                    connect(spin, &QSpinBox::valueChanged, this, &TuningPage::UpdateEnabled);
                    row.editor = spin;
                } else {
                    auto* combo = new QComboBox;
                    for (const auto& [choice, text] : row.choices) {
                        combo->addItem(text, choice);
                    }
                    connect(combo, &QComboBox::currentIndexChanged, this,
                            &TuningPage::UpdateEnabled);
                    row.editor = combo;
                }
                row.line = new QWidget;
                auto* line = new QHBoxLayout(row.line);
                line->setContentsMargins(0, 0, 0, 0);
                line->addWidget(new QLabel(row.label));
                line->addWidget(row.editor);
                line->addStretch();
            } else {
                continue;
            }
            row.line->installEventFilter(this);
            lines->addWidget(row.line);
            rows.push_back(std::move(row));
        }
        if (rows.size() == shown) {
            delete content;
            continue;
        }
        auto* header = new QToolButton;
        header->setText(group.title);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setAutoRaise(true);
        header->setCheckable(true);
        header->setChecked(group.expanded);
        const auto expand = [header, content](bool expanded) {
            header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            content->setVisible(expanded);
        };
        expand(group.expanded);
        connect(header, &QToolButton::toggled, content, expand);
        page->addWidget(header);
        page->addWidget(content);
    }

    auto* reset = new QPushButton(tr("Reset Tuning to Defaults"));
    connect(reset, &QPushButton::clicked, this, [this] { Show(GPUSettings{}); });
    page->addWidget(reset, 0, Qt::AlignLeft);
    page->addStretch();
}

void TuningPage::Load() {
    Show(EmulatorSettings.GetGroupValues(Section));
}

void TuningPage::Save(bool specific) {
    nlohmann::json values = nlohmann::json::object();
    for (const Row& row : rows) {
        if (qobject_cast<QCheckBox*>(row.editor)) {
            values[row.key] = Value(row) != 0;
        } else {
            values[row.key] = Value(row);
        }
    }
    EmulatorSettings.SetGroupValues(Section, values, specific);
}

void TuningPage::SetOutside(const char* key, u32 value) {
    std::ranges::find(outside, key, &Row::key)->value = value;
    UpdateEnabled();
}

bool TuningPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Enter) {
        // The filter is installed only on the line of a row in rows.
        const Row& row = *std::ranges::find(rows, obj, &Row::line);
        QString text = row.label + ":\n" + row.description + "\n\n" + Section + "." +
                       QString::fromStdString(row.key);
        if (const Row* blocker = Blocker(row)) {
            text +=
                "\n" + tr("Has no effect with the current value of \"%1\".").arg(blocker->label);
        }
        emit DescriptionChanged(text);
    } else if (event->type() == QEvent::Leave) {
        emit DescriptionChanged({});
    }
    return QWidget::eventFilter(obj, event);
}

void TuningPage::Show(const nlohmann::json& values) {
    for (const Row& row : rows) {
        const nlohmann::json& value = values.at(row.key);
        if (auto* check = qobject_cast<QCheckBox*>(row.editor)) {
            check->setChecked(value.get<bool>());
        } else if (auto* spin = qobject_cast<QSpinBox*>(row.editor)) {
            spin->setValue(std::min<u32>(value.get<u32>(), spin->maximum()));
        } else {
            auto* combo = static_cast<QComboBox*>(row.editor);
            // A value the list does not name gets an entry of its own, so it is kept.
            if (combo->findData(value.get<u32>()) < 0) {
                combo->addItem(QString::number(value.get<u32>()), value.get<u32>());
            }
            combo->setCurrentIndex(combo->findData(value.get<u32>()));
        }
    }
    UpdateEnabled();
}

u32 TuningPage::Value(const Row& row) const {
    if (!row.editor) {
        return row.value;
    }
    if (auto* check = qobject_cast<QCheckBox*>(row.editor)) {
        return check->isChecked();
    }
    if (auto* spin = qobject_cast<QSpinBox*>(row.editor)) {
        return spin->value();
    }
    return static_cast<QComboBox*>(row.editor)->currentData().toUInt();
}

const TuningPage::Row* TuningPage::Blocker(const Row& row) const {
    for (const Need& need : row.needs) {
        // A prerequisite that is not a row of this page or a setting of another tab counts as met.
        auto it = std::ranges::find(rows, need.key, &Row::key);
        if (it == rows.end()) {
            it = std::ranges::find(outside, need.key, &Row::key);
            if (it == outside.end()) {
                continue;
            }
        }
        if (const u32 value = Value(*it); value < need.min || value > need.max) {
            return &*it;
        }
        if (const Row* blocker = Blocker(*it)) {
            return blocker;
        }
    }
    return nullptr;
}

void TuningPage::UpdateEnabled() {
    for (const Row& row : rows) {
        row.line->setEnabled(!Blocker(row));
    }
}
