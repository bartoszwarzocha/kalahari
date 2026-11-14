// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnote_settings_dialog.h
/// @brief Insert > Footnote Layout...: how the book numbers its notes and where its
///        endnotes are

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

class QComboBox;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Insert > Footnote Layout...: the numbering of the book's footnotes and endnotes
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

} // namespace dialogs
} // namespace gui
} // namespace kalahari
