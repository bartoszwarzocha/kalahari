// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_forms_table.h
/// @brief The table of the forms of a name of the book's dictionary, by case, which the author
///        corrects

#ifndef KALAHARI_GUI_WIDGETS_NAME_FORMS_TABLE_H
#define KALAHARI_GUI_WIDGETS_NAME_FORMS_TABLE_H

#include <kalahari/core/book_dictionary.h>

#include <QList>
#include <QTableWidget>

namespace kalahari::gui {

/// @brief The forms of one name (a core::NameVariant) by case: a row for each case of the
///        language of the book with its questions, a column for the singular and, when the
///        plural of the name is used, one for the plural
///
/// A cell can have several forms, apart by commas, when each is right ("Zakopanem,
/// Zakopanym"). A cell the author changes is one of the variant's edited cells, which keep
/// their forms when the model changes (core::refreshForms()). ↑ and ↓ choose a case, F2 or
/// typing changes its form, Enter ends the change; Enter outside a change goes to the window
/// (its default button), and Tab leaves the table. The window of a name has it, and so can a
/// card of the Workshop.
class NameFormsTable : public QTableWidget {
    Q_OBJECT

public:
    /// @brief An empty table for the cases of a language
    /// @param declension The models of the language of the book: its cases are the rows
    /// @param parent Parent widget
    explicit NameFormsTable(const core::NameDeclension& declension, QWidget* parent = nullptr);

    /// @brief Show the forms of a variant
    void setVariant(const core::NameVariant& variant);

    /// @brief The variant with the forms of the table, and the cells the author changed among
    ///        its edited cells
    [[nodiscard]] const core::NameVariant& variant() const { return m_variant; }

    /// @brief Show the column of the plural, or not (it is not shown at first)
    void setPluralShown(bool shown);

    /// @brief Whether the column of the plural is shown
    [[nodiscard]] bool isPluralShown() const { return m_pluralShown; }

    /// @brief Let the author change the forms, or not (a book open read-only)
    void setEditable(bool editable);

    /// @brief The forms of a cell as they are written in it: "Zakopanem, Zakopanym"
    [[nodiscard]] static QString cellText(const QStringList& forms);

    /// @brief The forms written in a cell, without spaces around them and empty ones
    [[nodiscard]] static QStringList formsOfText(const QString& text);

signals:
    /// @brief The author changed the forms of a cell: variant() has them
    void formsEdited();

private:
    /// @brief The rows and the columns, and the forms of the variant in the cells
    void fill();

    /// @brief A cell changed: its forms go to the variant, as written by the author
    void onItemChanged(const QTableWidgetItem* item);

    QList<core::GrammaticalCase> m_cases;  ///< The rows: the cases of the language
    core::NameVariant m_variant;           ///< What the table shows
    bool m_pluralShown = false;            ///< The column of the plural is shown
    bool m_editable = true;                ///< The forms can be changed
    bool m_filling = false;                ///< The cells are being filled, not changed
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_NAME_FORMS_TABLE_H
