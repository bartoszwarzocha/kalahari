// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file rule_dialog.h
/// @brief The window "Spelling Rule" of the book's dictionary: how the author wants a word
///        written in the book

#ifndef KALAHARI_GUI_DIALOGS_RULE_DIALOG_H
#define KALAHARI_GUI_DIALOGS_RULE_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/book_dictionary.h>

#include <QStringList>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace kalahari::gui::dialogs {

/// @brief A spelling rule of the book: the right spelling, the wrong ones, whether its forms
///        count too, and the author's note
///
/// "Write" is the right spelling; a word the spelling dictionary does not know ("półelf")
/// becomes right in the book with it. "Instead of" has the spellings that are wrong in the
/// book: the program adds those that suggest themselves (the other case of a capital letter,
/// without the hyphen, with it or apart) as the right spelling is written, and the author
/// adds others and takes out those not wanted. The rule holds also where the spelling
/// dictionary takes a wrong spelling for right. "Also in their forms" covers every form of the
/// word, from the spelling dictionary, or for a word it does not know from the word it ends
/// with ("półelf" like "elf"); the window shows them. The note tells why, in the context menu
/// and in the Spelling and Grammar panel.
class RuleDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Whether a word is right (the spelling dictionary), to split a word made of two
    using WordCheck = std::function<bool(const QString&)>;

    /// @brief The window of a rule
    /// @param rule The rule to change; a new one has no id
    /// @param forms The forms of the words of the spelling dictionary (none: the rule covers
    ///        the spellings it has alone)
    /// @param isWord Whether a word is right (none: no word is split)
    /// @param parent Parent widget
    RuleDialog(const core::SpellingRule& rule, const core::AffixExpander* forms, WordCheck isWord,
               QWidget* parent = nullptr);

    /// @brief The rule as the window has it, with its forms (core::fillRuleForms())
    [[nodiscard]] core::SpellingRule rule() const;

    /// @brief The wrong spellings listed, in their order
    [[nodiscard]] QStringList wrongSpellings() const;

    /// @brief Add a wrong spelling the author wrote; false when it is empty, the right one, or
    ///        listed
    bool addWrongSpelling(const QString& wrong);

    /// @brief Take a wrong spelling out; a suggested one does not come back
    void removeWrongSpelling(const QString& wrong);

protected:
    /// @brief Enter in the field of a new wrong spelling adds it, and does not close the window
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The fields of the window, filled from @p rule
    void createFields(const core::SpellingRule& rule);

    /// @brief The right spelling was changed: the suggested wrong spellings follow it
    void onRightEdited();

    /// @brief List a wrong spelling, suggested by the program or written by the author
    void listWrong(const QString& wrong, bool suggested);

    /// @brief What the window says of the forms the rule covers
    void updateForms();

    /// @brief The buttons that can do something now
    void updateButtons();

    core::SpellingRule m_rule;              ///< Its id, as the rule given
    const core::AffixExpander* m_expander;  ///< The forms of the words (not owned)
    WordCheck m_isWord;                     ///< Whether a word is right
    QStringList m_suggested;                ///< The wrong spellings suggested for the right one
    QStringList m_dismissed;                ///< Those the author took out

    QLineEdit* m_right = nullptr;
    QTreeWidget* m_wrong = nullptr;
    QLineEdit* m_newWrong = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QCheckBox* m_allForms = nullptr;
    QLabel* m_forms = nullptr;
    QLineEdit* m_note = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_RULE_DIALOG_H
