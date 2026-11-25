// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file printer_driver.h
/// @brief The settings of the driver of a printer: its paper sources and qualities, the
///        window of the driver (Printer Properties...) and its settings for a print

#ifndef KALAHARI_GUI_PRINTER_DRIVER_H
#define KALAHARI_GUI_PRINTER_DRIVER_H

#include "kalahari/gui/dialogs/print_book_dialog.h"

#include <memory>
#include <optional>

class QPrinter;
class QWidget;

namespace kalahari::gui::printer_driver {

/// @brief Whether the system lets the program show the window of the driver of a printer and
///        give its settings to the print (Windows)
[[nodiscard]] bool hasDriverWindow();

/// @brief The paper sources and qualities of @p printer as its driver lists them, with those
///        it takes at first; nothing where the system does not say
void addSourcesAndQualities(dialogs::PrintBookDialog::Printer& printer);

/// @brief Show the window of the driver of the printer of @p choice over @p parent, with the
///        choices of the window: what it chose, or nothing when the writer cancelled it or
///        the system has no such window
[[nodiscard]] std::optional<dialogs::PrintBookDialog::DriverChoice> showDriverWindow(
    const dialogs::PrintBookDialog::Choice& choice, QWidget* parent);

/// @brief The settings of the driver given to a printer for a print, held while the printer
///        lives
///
/// Declared before the QPrinter, so that the printer goes first: the settings are the
/// printer's for as long as it lives. The setters of the printer called after apply() change
/// them (its paper, sides, color, copies).
class PrintSettings {
public:
    /// @brief Give @p printer the settings of the window of the driver of @p choice, else
    ///        those of its driver at first, with the paper source and the quality of
    ///        @p choice; false: there was nothing to give, or the system does not let it
    bool apply(QPrinter& printer, const dialogs::PrintBookDialog::Choice& choice);

private:
    /// @brief Frees the memory of the settings (on Windows an HGLOBAL)
    struct Release {
        void operator()(void* memory) const;
    };

    std::unique_ptr<void, Release> m_memory;  ///< The settings given to the printer
};

}  // namespace kalahari::gui::printer_driver

#endif  // KALAHARI_GUI_PRINTER_DRIVER_H
