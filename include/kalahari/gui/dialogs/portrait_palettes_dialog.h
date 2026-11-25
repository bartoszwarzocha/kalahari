// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file portrait_palettes_dialog.h
/// @brief The window Portrait Palettes: the colors the window Portrait chooses from

#ifndef KALAHARI_GUI_DIALOGS_PORTRAIT_PALETTES_DIALOG_H
#define KALAHARI_GUI_DIALOGS_PORTRAIT_PALETTES_DIALOG_H

#include "kalahari/gui/avatar.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QString>

#include <optional>

class QPushButton;
class QWidget;

namespace kalahari::gui {
class PaletteEdit;
}  // namespace kalahari::gui

namespace kalahari::gui::dialogs {

/// @brief The window Portrait Palettes (Settings > Cards): each palette of the portraits of
/// parts as a row of swatches, in the order of AvatarPalettes::names()
///
/// A click on a swatch changes the color, Delete takes it out, "+" or Insert adds one; Restore
/// Defaults gives each palette the colors of the program. The palettes change on OK.
class PortraitPalettesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window with @p palettes
    /// @param palettes The palettes shown
    /// @param parent Parent widget
    explicit PortraitPalettesDialog(const AvatarPalettes& palettes, QWidget* parent = nullptr);

    /// @brief The palettes as the writer left them
    [[nodiscard]] AvatarPalettes palettes() const;

    /// @brief The swatches of palette @p name; nullptr for another name
    [[nodiscard]] PaletteEdit* paletteEdit(const QString& name) const;

    /// @brief Restore Defaults: the colors of the program in every palette
    [[nodiscard]] QPushButton* restoreButton() const { return m_restore; }

private:
    /// @brief Show @p palettes
    void showPalettes(const AvatarPalettes& palettes);

    QList<PaletteEdit*> m_edits;  ///< In the order of AvatarPalettes::names()
    QPushButton* m_restore = nullptr;
};

/// @brief Open Portrait Palettes for @p palettes
/// @return The palettes as the writer changed them; nullopt when the window was closed
[[nodiscard]] std::optional<AvatarPalettes> editPortraitPalettes(const AvatarPalettes& palettes,
                                                                 QWidget* parent);

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_PORTRAIT_PALETTES_DIALOG_H
