// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file footnote_settings_dialog.h
/// @brief Format > Footnote Layout...: how the book numbers its notes and where its
///        endnotes are

#ifndef KALAHARI_GUI_DIALOGS_FOOTNOTE_SETTINGS_DIALOG_H
#define KALAHARI_GUI_DIALOGS_FOOTNOTE_SETTINGS_DIALOG_H

#include "kalahari/core/book_project.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QComboBox;

namespace kalahari::gui::dialogs {

/// @brief Format > Footnote Layout...: the numbering of the book's footnotes and endnotes
///        and the place of its endnotes
///
/// The format of the numbers (1, 2, 3; a, b, c; A, B, C; i, ii, iii; I, II, III; *, †, ‡) and
/// the numbering, anew in each chapter or through the whole book, for each kind; the endnotes
/// at the end of each chapter or of the book (the editor shows them after the last chapter).
/// The book keeps them in its file (core::BookFootnotes). The gear opens Settings > Editor >
/// General, where the words of the notes are counted.
class FootnoteSettingsDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param footnotes The book's settings now
    explicit FootnoteSettingsDialog(const core::BookFootnotes& footnotes,
                                    QWidget* parent = nullptr);

    /// @brief The settings as the writer left them (the fields the program does not know kept)
    [[nodiscard]] core::BookFootnotes footnotes() const;

    /// @brief The sample of a format of the numbers: "1, 2, 3", "*, †, ‡"
    [[nodiscard]] static QString formatSample(const QString& format);

private:
    core::BookFootnotes m_footnotes;
    QComboBox* m_footnoteFormat = nullptr;
    QComboBox* m_footnoteNumbering = nullptr;
    QComboBox* m_endnotePlace = nullptr;
    QComboBox* m_endnoteFormat = nullptr;
    QComboBox* m_endnoteNumbering = nullptr;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_FOOTNOTE_SETTINGS_DIALOG_H
