/// @file field_dialogs.h
/// @brief The windows of the fields: Insert > Date && Time... and Insert > Field...

#pragma once

#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QColor>
#include <QDateTime>
#include <QString>

#include <array>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QTabWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

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
/// The fields in four tabs (the book: its title and author; the text: its title and words;
/// the pages: the page and the page count; the date), and under the list of the tab shown the
/// formats of the field chosen and how it shows in the text, on the gray background of the
/// fields. The same window changes a field (Edit Field...). The gear opens Settings > Editor >
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
    };

    explicit FieldDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The field chosen
    [[nodiscard]] editor::TextField field() const;

private:
    /// @brief The formats of the field chosen, the one of @p format chosen
    void showFormats(const QString& format);

    /// @brief The format, the preview and the hint in tab @p tab, under its list
    void showTab(int tab);

    /// @brief How the field chosen shows in the text, and what changes it
    void showPreview();

    /// @brief The type of the field chosen
    [[nodiscard]] editor::FieldType chosenType() const;

    Data m_data;
    QTabWidget* m_tabs = nullptr;
    std::vector<QListWidget*> m_lists;  ///< Of the tabs, in their order
    QWidget* m_details = nullptr;       ///< The format, the preview and the hint
    QComboBox* m_formats = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_hint = nullptr;
};

/// @brief The text of a format of a type in the lists of the windows: "11 October 2026",
///        "i, ii, iii", "In Capitals"
/// @param now What the date formats show
/// @param language The book's language; empty: the program's
QString fieldFormatText(editor::FieldType type, const QString& format, const QDateTime& now,
                        const QString& language);

} // namespace dialogs
} // namespace gui
} // namespace kalahari
