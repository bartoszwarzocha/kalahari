// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_lookup_dialog.h
/// @brief The window of From the Web by ISBN or DOI...: the number of a source, and the
/// description a catalog on the web gives for it

#ifndef KALAHARI_GUI_DIALOGS_SOURCE_LOOKUP_DIALOG_H
#define KALAHARI_GUI_DIALOGS_SOURCE_LOOKUP_DIALOG_H

#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/source_lookup.h"

#include <QString>

#include <optional>

class QLabel;
class QLineEdit;

namespace kalahari::gui::dialogs {

/// @brief From the Web by ISBN or DOI...: the writer types or keeps the number, Look Up asks
/// the catalog, and the window closes with the description it found
///
/// The window says what goes where before anything goes: only the number, an ISBN to the
/// catalog of the Settings, a DOI to doi.org. While the catalog answers the field waits; a
/// lookup that brings nothing says why under it and the window stays. Esc gives up, also
/// while the catalog answers.
class SourceLookupDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param number The number the description has: its DOI, else its ISBN; empty: none
    /// @param parent Parent widget
    explicit SourceLookupDialog(const QString& number, QWidget* parent = nullptr);

    /// @brief The description found; nullopt until one comes
    [[nodiscard]] const std::optional<core::SourceDescription>& found() const { return m_found; }

    /// @brief The catalog it came from (core::SourceCatalogs::GOOGLE_BOOKS...)
    [[nodiscard]] QString foundIn() const { return m_catalog; }

    /// @brief Look Up: ask the catalog for the number typed
    void accept() override;

    /// @brief Give up, also while the catalog answers
    void reject() override;

    // The parts, for the tests
    /// @brief The field of the number
    [[nodiscard]] QLineEdit* numberEdit() const { return m_numberEdit; }
    /// @brief What the lookup did: asks, did not find
    [[nodiscard]] QLabel* statusLabel() const { return m_statusLabel; }
    /// @brief The lookup it makes
    [[nodiscard]] SourceLookup* lookup() const { return m_lookup; }

private:
    /// @brief The lookup ended with @p result: the window closes with what it found, or says
    /// why nothing came
    void take(const SourceLookup::Result& result);

    /// @brief Say @p text under the field
    void showStatus(const QString& text);

    /// @brief While a lookup is on its way the field and Look Up wait
    void setWaiting(bool waiting);

    QLineEdit* m_numberEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    SourceLookup* m_lookup = nullptr;
    std::optional<core::SourceDescription> m_found;
    QString m_catalog;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_SOURCE_LOOKUP_DIALOG_H
