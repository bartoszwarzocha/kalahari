// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file new_quote_dialog.h
/// @brief The window of New Quote from the text selected in the preview of a PDF file

#ifndef KALAHARI_GUI_DIALOGS_NEW_QUOTE_DIALOG_H
#define KALAHARI_GUI_DIALOGS_NEW_QUOTE_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace kalahari::gui::dialogs {

/// @brief New Quote: the words of a quote from a source, its page and what it is linked with
///
/// Its words and its page come from the selection and can be changed; the source is the file
/// shown. Create makes the quote, Create and Insert into Chapter also puts its words with the
/// reference to the source into the chapter open last; both need words. Esc gives up.
class NewQuoteDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the new quote is linked with
    enum class Link {
        Chapter,  ///< The chapter open last
        None,     ///< Nothing
        Other     ///< What the writer chooses in the Link window after it is made
    };

    /// @brief The window for a quote with words @p text from page @p page of source @p source
    /// ("Atlas pustyń świata – Wiśniewska, 2011") in file @p file
    /// @param chapter The title of the chapter open last; empty: none, so the quote can be
    /// linked with no chapter and inserted into none
    /// @param canInsert Whether that chapter can take the quote (it is not read-only)
    /// @param parent Parent widget
    NewQuoteDialog(const QString& text, const QString& page, const QString& source,
                   const QString& file, const QString& chapter, bool canInsert,
                   QWidget* parent = nullptr);

    /// @brief The words of the quote, without the spaces around them
    [[nodiscard]] QString text() const;

    /// @brief Its page, without the spaces around it
    [[nodiscard]] QString page() const;

    /// @brief What it is linked with
    [[nodiscard]] Link link() const;

    /// @brief Whether the writer chose Create and Insert into Chapter
    [[nodiscard]] bool insertIntoChapter() const { return m_insert; }

    // The parts, for the tests
    [[nodiscard]] QPlainTextEdit* textEdit() const { return m_textEdit; }
    [[nodiscard]] QLineEdit* pageEdit() const { return m_pageEdit; }
    [[nodiscard]] QComboBox* linkBox() const { return m_linkBox; }
    [[nodiscard]] QPushButton* insertButton() const { return m_insertButton; }

private:
    /// @brief The buttons need words, and inserting a chapter that can take them
    void updateButtons();

    QPlainTextEdit* m_textEdit = nullptr;
    QLineEdit* m_pageEdit = nullptr;
    QComboBox* m_linkBox = nullptr;
    QPushButton* m_insertButton = nullptr;
    bool m_canInsert = false;
    bool m_insert = false;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_NEW_QUOTE_DIALOG_H
