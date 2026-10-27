// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <QWidget>
#include <nlohmann/json.hpp>

#include "common/types.h"

class QCheckBox;

// Edits the boolean and unsigned settings of the GPU section of the settings model. The rows are
// built from the keys the model has: Table() gives the label, description, range, choice list and
// prerequisites of the keys it knows, and a key it does not name is listed under "Other".
// Upstream Behavior sets the shown rows of the keys Table() names to 0, at which the emulator runs
// the code paths of upstream shadPS4, and locks them.
class TuningPage : public QWidget {
    Q_OBJECT
public:
    explicit TuningPage(QWidget* parent = nullptr);

    /// Shows the values the settings model holds.
    void Load();
    /// Writes the shown values to the settings model, as per-game overrides when specific is set.
    void Save(bool specific);
    /// Sets the value of a setting edited on another tab, for the prerequisites that name it.
    void SetOutside(const char* key, u32 value);

signals:
    /// The description of the setting under the pointer, or an empty text when it leaves one.
    void DescriptionChanged(const QString& text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// A setting acts only while the value of key, a check box counting as 0 or 1, is in
    /// [min, max].
    struct Need {
        const char* key;
        u32 min;
        u32 max;
    };
    struct Row {
        std::string key;
        QString label;
        QString description;
        std::vector<Need> needs;
        /// Value and text of each entry; a number with no choices gets a spin box.
        std::vector<std::pair<u32, QString>> choices;
        int max = std::numeric_limits<int>::max();
        QWidget* line{}; ///< Hovered for the description, and disabled while a need is unmet
        QWidget* editor{};
        u32 value{}; ///< Value of a setting edited on another tab, which has no editor here
        bool fork{}; ///< Shown, and named by Table(), so a setting upstream shadPS4 lacks
    };
    struct Group {
        /// Empty for the keys that have their control on another tab, which are not shown.
        QString title;
        bool expanded;
        std::vector<Row> rows;
    };
    static std::vector<Group> Table();

    void Show(const nlohmann::json& values);
    nlohmann::json Values() const;
    u32 Value(const Row& row) const;
    /// Checks or unchecks Upstream Behavior and keeps the shown values.
    void SetUpstream(bool checked);
    /// The row whose value keeps row from acting, following prerequisites of prerequisites.
    const Row* Blocker(const Row& row) const;
    void UpdateEnabled();

    std::vector<Row> rows;
    std::vector<Row> outside; ///< The rows of the group without a title
    QCheckBox* upstream{};
    nlohmann::json restore; ///< Shown when Upstream Behavior is unchecked
};
