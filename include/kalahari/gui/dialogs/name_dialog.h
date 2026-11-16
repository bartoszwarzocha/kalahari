// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_dialog.h
/// @brief The window "Name" of the book's dictionary: a name of the book with its forms

#ifndef KALAHARI_GUI_DIALOGS_NAME_DIALOG_H
#define KALAHARI_GUI_DIALOGS_NAME_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/book_dictionary.h>

#include <QList>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QTreeWidget;

namespace kalahari::gui {
class NameFormsTable;
}  // namespace kalahari::gui

namespace kalahari::gui::dialogs {

/// @brief A name of the book's dictionary: the name itself and its other names, each with the
///        model of declension of its words and its forms by case
///
/// The list at the left has the name itself first, then its other names: for a character the
/// first name and the surname alone, which the program adds, and the diminutives and the
/// nicknames the author adds. The fields at the right are those of the one chosen in the list:
/// its text, the model of each of its words (by its ending the program picks one; the
/// spelling dictionary, when it knows the word, confirms its forms) and the table of its forms,
/// which the program fills and the author corrects. A cell the author corrects keeps its forms
/// when a model changes; "Suggest the Forms Again" gives every cell the forms of the models.
/// The kind of the name decides its group in Quick Insert, and the language of the book its
/// cases: Polish has seven, English the singular and the plural only, and a language without
/// models has the forms the author writes.
class NameDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window of a name
    /// @param name The name to change; a new one has no id (core::makeName() gives it its
    ///        models and forms)
    /// @param declension The models of the language of the book
    /// @param dictionary The forms the spelling dictionary knows (none: the models alone)
    /// @param parent Parent widget
    NameDialog(const core::DictionaryName& name, core::NameDeclension declension,
               core::NameDeclension::DictionaryForms dictionary, QWidget* parent = nullptr);

    /// @brief The name as the window has it: the texts without spaces around them, without
    ///        the other names left empty
    [[nodiscard]] core::DictionaryName name() const;

    /// @brief The other name chosen in the list, from 0 for the name itself
    [[nodiscard]] int currentVariant() const;

    /// @brief Choose a name in the list: its fields show
    void setCurrentVariant(int index);

    /// @brief The table of the forms of the one chosen
    [[nodiscard]] NameFormsTable* formsTable() const { return m_forms; }

    /// @brief A text is being typed: its models and its forms come at a pause in typing,
    ///        or when something else is done in the window
    [[nodiscard]] bool typing() const;

    /// @brief The models and the forms of the text typed now, without the pause
    void finishTyping();

    /// @brief Add or Save: the forms of the text typed first
    void accept() override;

private:
    /// @brief The fields at the right for the one chosen in the list
    void showVariant();

    /// @brief The rows of the models of the words of the one chosen
    void fillModels();

    /// @brief The list at the left from the name
    void fillList();

    /// @brief The text of the one chosen was changed: in the list at once, its models and
    ///        forms at a pause in typing (formsOfText())
    void onTextEdited(const QString& text);

    /// @brief The words of the text of the one chosen get the models the program picks, and
    ///        its forms those of the models; for a new name, the first name and the surname
    ///        alone follow the name itself
    void formsOfText();

    /// @brief The kind was changed: the words whose models the program picked get those it
    ///        picks for the new kind, and every name the forms of its models
    void onKindChanged();

    /// @brief The model of a word of the one chosen was changed
    void onModelChosen(int word, const QString& model);

    /// @brief "Does not change" was turned on (every word keeps its form) or off (the models
    ///        the program picks)
    void onUnchangedToggled(bool unchanged);

    /// @brief Give the one chosen the forms of its models anew, but in the cells the author
    ///        corrected
    void refreshCurrent();

    /// @brief Add an empty other name and choose it, its text field with the keys
    void addVariant();

    /// @brief Take the other name chosen out
    void removeVariant();

    /// @brief What the hint under the table says of where the forms come from
    void updateHint();

    /// @brief The buttons that can do something now
    void updateButtons();

    /// @brief The models the program picks for a text of the kind of the name
    [[nodiscard]] QStringList pickedModels(const QString& text, core::NameKind kind) const;

    /// @brief What the list says of a name beside its text: "main", "first name alone"
    [[nodiscard]] QString roleOf(int index) const;

    core::DictionaryName m_name;                         ///< The name as the window has it
    core::NameDeclension m_declension;                   ///< The models of the language
    core::NameDeclension::DictionaryForms m_dictionary;  ///< The forms the dictionary knows
    bool m_new = false;                                  ///< A new name (with no id)
    int m_current = 0;                                   ///< The one chosen in the list
    bool m_showing = false;  ///< The fields are being filled, not changed

    QTreeWidget* m_list = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QLineEdit* m_text = nullptr;
    QComboBox* m_kind = nullptr;
    QGroupBox* m_modelsBox = nullptr;
    QFormLayout* m_modelsLayout = nullptr;
    QList<QComboBox*> m_models;  ///< The model of each word of the one chosen
    QStringList m_modelWords;    ///< The words of the rows of m_models
    QCheckBox* m_unchanged = nullptr;
    NameFormsTable* m_forms = nullptr;
    QLabel* m_hint = nullptr;
    QPushButton* m_suggestButton = nullptr;
    QCheckBox* m_plural = nullptr;
    QCheckBox* m_quickInsert = nullptr;
    QTimer* m_formsTimer = nullptr;  ///< The pause in typing a text
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_NAME_DIALOG_H
