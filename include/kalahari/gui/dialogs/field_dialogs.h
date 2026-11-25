// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file field_dialogs.h
/// @brief The windows of the fields: Insert > Date && Time... and Insert > Field...

#ifndef KALAHARI_GUI_DIALOGS_FIELD_DIALOGS_H
#define KALAHARI_GUI_DIALOGS_FIELD_DIALOGS_H

#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/own_fields.h"

#include <QColor>
#include <QDateTime>
#include <QString>

#include <array>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QTabWidget;
class QTreeWidget;

namespace kalahari::gui::dialogs {

/// @brief Insert > Date && Time...: today's date or time in the language of the book
///
/// A list of the formats, each as it shows now: "11 October 2026", "11.10.2026", with the day
/// of the week, "2026-10-11", "14:35" and the date with the time. Update Automatically inserts
/// a field, which shows the date of the day the text is opened; without it the date is text.
class DateTimeDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows
    struct Data {
        QDateTime now;      ///< The date and time the formats show
        QString language;   ///< The book's language; empty: the program's
        QString format;     ///< The format chosen first (editor::fieldFormats() of the date)
        bool asField = false;  ///< Update Automatically, as it was last
        /// Why the text keeps no field (a plain text file): Update Automatically greyed out,
        /// saying so; empty: it keeps them
        QString whyNoField;
        QString dateKeys;   ///< The keys that insert the date at once ("Alt+Shift+D"); empty: none
        QString timeKeys;   ///< And the time
    };

    /// @brief The window showing @p shown
    explicit DateTimeDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The format chosen
    [[nodiscard]] QString format() const;

    /// @brief Whether the date goes in as a field (Update Automatically)
    [[nodiscard]] bool asField() const;

private:
    Data m_data;
    QListWidget* m_formats = nullptr;
    QCheckBox* m_asField = nullptr;
};

/// @brief Insert > Field...: a value that refreshes by itself
///
/// The fields in five tabs (the book: its title and author; the text: its title and words;
/// the pages: the page and the page count; the date; the own fields of the book and of its
/// series, made, changed and deleted there), and under the list of the tab shown the formats
/// of the field chosen and how it shows in the text, on the gray background of the fields.
/// The same window changes a field (Edit Field...). The gear opens Settings > Editor >
/// General, where the gray background can be turned off.
class FieldDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows
    struct Data {
        /// The names of the fields, in the order of editor::FIELD_TYPES ("Chapter Title")
        std::array<QString, editor::FIELD_TYPES.size()> names;
        editor::TextField field;  ///< Chosen first
        bool editing = false;     ///< It changes a field: OK in place of Insert
        QString language;         ///< The book's language; empty: the program's
        QDateTime now;            ///< What the formats of the date show
        /// The text a field shows where it goes (the book's values, the words, the page)
        std::function<QString(const editor::TextField&)> shown;
        QColor background;  ///< Of the fields in the text; invalid: none
        QColor text;        ///< Of the text; invalid: the window's
        /// The own fields of the book as they are now (the tab Own); null: a text outside a
        /// book, which has none
        std::function<QList<OwnField>()> ownFields;
        /// New Field...: the window of a new field; the id of the field made, empty: none
        std::function<QString()> newOwnField;
        /// Change...: the window of the field; whether it changed
        std::function<bool(const QString& id)> changeOwnField;
        /// Delete: asks first; whether the field is gone
        std::function<bool(const QString& id)> deleteOwnField;
        /// Why the own fields cannot change (a book open read-only); empty: they can
        QString whyOwnFieldsFixed;
    };

    /// @brief The window showing @p shown
    explicit FieldDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The field chosen
    [[nodiscard]] editor::TextField field() const;

    /// @brief The tab of the own fields
    [[nodiscard]] int ownTab() const { return m_ownTab; }

    /// @brief The list of the own fields (for the tests)
    [[nodiscard]] QTreeWidget* ownList() const { return m_ownList; }

    /// @brief The id of the own field chosen; empty: none
    [[nodiscard]] QString chosenOwnField() const;

    /// @brief New Field..., Change... and Delete of the tab Own
    void newOwnField();
    void changeOwnField();
    void deleteOwnField();

private:
    /// @brief The tabs of the fields, each with the list of its fields
    /// @return The tab of the field chosen first
    int addTabs();

    /// @brief The list of the fields @p types in a tab, @p lines high, the field chosen first
    ///        chosen in it
    [[nodiscard]] QListWidget* fieldList(const std::vector<editor::FieldType>& types, int lines,
                                         QWidget* page) const;

    /// @brief The format, the preview and the hint, shown under the list of the tab shown
    void addDetails();

    /// @brief The preview in the colors of the fields in the text
    void colorPreview();

    /// @brief The formats of the field chosen, the one of @p format chosen
    void showFormats(const QString& format);

    /// @brief The format, the preview and the hint in tab @p tab, under its list
    void showTab(int tab);

    /// @brief How the field chosen shows in the text, and what changes it
    void showPreview();

    /// @brief The type of the field chosen
    [[nodiscard]] editor::FieldType chosenType() const;

    /// @brief The hint under the preview of @p field: what changes it, and the gray background
    [[nodiscard]] static QString hintText(const editor::TextField& field);

    /// @brief The tab of the own fields: their list and its buttons, @p lines high
    [[nodiscard]] QWidget* ownFieldsTab(int lines);

    /// @brief The own fields in their list, @p chosen chosen (else the one chosen now)
    void fillOwnFields(const QString& chosen);

    /// @brief Insert and the buttons of the tab Own, as the field chosen allows
    void updateButtons();

    Data m_data;
    QTabWidget* m_tabs = nullptr;
    std::vector<QListWidget*> m_lists;  ///< Of the tabs, in their order; not the tab Own
    int m_ownTab = -1;
    QTreeWidget* m_ownList = nullptr;
    QLabel* m_ownNote = nullptr;  ///< Under the list: none yet, a text outside a book
    QPushButton* m_newOwn = nullptr;
    QPushButton* m_changeOwn = nullptr;
    QPushButton* m_deleteOwn = nullptr;
    QWidget* m_details = nullptr;       ///< The format, the preview and the hint
    QComboBox* m_formats = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_hint = nullptr;
};

/// @brief New Field... and Change... of an own field: its name, value and description; in a
///        series, whether it is the book's or of all the volumes
class OwnFieldDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The name is wrong: empty, or that of another field (the window says why)
    using NameCheck = std::function<QString(const QString& name)>;

    /// @param field The field (a new one: no id)
    /// @param inSeries The book is a volume of a series: the field can be of all the volumes
    /// @param checkName Why a name cannot be the field's; empty: it can
    OwnFieldDialog(const OwnField& field, bool inSeries, NameCheck checkName,
                   QWidget* parent = nullptr);

    /// @brief The field as the window has it
    [[nodiscard]] OwnField field() const;

    [[nodiscard]] QLineEdit* nameField() const { return m_name; }
    [[nodiscard]] QLineEdit* valueField() const { return m_value; }
    [[nodiscard]] QLineEdit* volumeValueField() const { return m_volumeValue; }
    [[nodiscard]] QRadioButton* seriesChoice() const { return m_inSeries; }
    [[nodiscard]] QCheckBox* ownValueChoice() const { return m_ownValue; }

    void accept() override;

private:
    /// @brief The fields of the values as the place of the field says
    void updateValues();

    OwnField m_field;
    NameCheck m_checkName;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_value = nullptr;  ///< Of the book; of a field of the series, of all the volumes
    QLineEdit* m_description = nullptr;
    QRadioButton* m_inBook = nullptr;
    QRadioButton* m_inSeries = nullptr;
    QCheckBox* m_ownValue = nullptr;     ///< A field of the series: another value in this book
    QLineEdit* m_volumeValue = nullptr;  ///< That value
    QLabel* m_problem = nullptr;
};

/// @brief Delete of an own field used in the texts: how often and where it is used, and what
///        its places become: the text of its value, or empty fields to fix
class DeleteOwnFieldDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param field The field
    /// @param uses How many times it is in the texts
    /// @param texts In how many texts
    /// @param undoKeys The keys of Undo ("Ctrl+Z"); empty: none
    DeleteOwnFieldDialog(const OwnField& field, int uses, int texts, const QString& undoKeys,
                         QWidget* parent = nullptr);

    /// @brief Its places become the text of its value; false: empty fields
    [[nodiscard]] bool asText() const;

    [[nodiscard]] QRadioButton* textChoice() const { return m_asText; }
    [[nodiscard]] QRadioButton* emptyChoice() const { return m_keep; }

    /// @brief What the window says, to copy (Copy)
    [[nodiscard]] QString message() const;

private:
    QString m_message;
    QRadioButton* m_asText = nullptr;
    QRadioButton* m_keep = nullptr;
};

/// @brief The text of a format of a type in the lists of the windows: "11 October 2026",
///        "i, ii, iii", "In Capitals"
/// @param now What the date formats show
/// @param language The book's language; empty: the program's
QString fieldFormatText(editor::FieldType type, const QString& format, const QDateTime& now,
                        const QString& language);

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_FIELD_DIALOGS_H
