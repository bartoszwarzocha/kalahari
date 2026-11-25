// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file version_name_dialog.h
/// @brief The small window of Save Version (Alt+Shift+S), with the description of the new
/// version, and of Change Description in the window of the versions

#ifndef KALAHARI_GUI_DIALOGS_VERSION_NAME_DIALOG_H
#define KALAHARI_GUI_DIALOGS_VERSION_NAME_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/kind_words.h>

#include <QString>

class QLineEdit;

namespace kalahari::gui::dialogs {

/// @brief The description of a version: of a new one, or a new description of one kept
///
/// The field may stay empty: a version without a description has only its date and time.
/// Enter saves, also with the field empty; Esc gives up.
class VersionNameDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window of Save Version
    /// @param title The title of the text the version keeps
    /// @param kind The words of the kind of the text ("chapter")
    /// @param parent Parent widget
    VersionNameDialog(const QString& title, const core::KindWords& kind,
                      QWidget* parent = nullptr);

    /// @brief The window of Change Description
    /// @param when When the version was kept (versionTime())
    /// @param origin Where it comes from (versionOrigin())
    /// @param name Its description now; the field starts with it, selected
    /// @param parent Parent widget
    VersionNameDialog(const QString& when, const QString& origin, const QString& name,
                      QWidget* parent = nullptr);

    /// @brief The description, without spaces around it; empty: none
    [[nodiscard]] QString name() const;

private:
    /// @brief The field of the description, starting with @p name, and @p hintText under it
    void addNameField(const QString& name, const QString& hintText);

    QLineEdit* m_nameEdit = nullptr;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_VERSION_NAME_DIALOG_H
