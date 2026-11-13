/// @file field_coordinator.h
/// @brief The fields of the open texts: what they show of the book, Insert > Date && Time...,
///        Insert > Field..., the date and the time at once (Alt+Shift+D, Alt+Shift+T) and
///        Edit Field...

#pragma once

#include "kalahari/editor/text_fields.h"

#include <QList>
#include <QObject>
#include <QString>

#include <array>
#include <functional>
#include <optional>

class QTabWidget;
class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

/// @brief Gives every open text what its fields show and inserts the fields
///
/// A field shows the book's title, its author, the title of its text, the date (the book's
/// language gives their look) and, from the text itself, the words and the pages
/// (editor::FieldContext). This coordinator gives every open text its context when a text
/// opens, closes or comes to the front, when another book opens and when the book's title,
/// author or language or a text's title changes: the dates are those of then.
///
/// Insert > Date && Time... inserts today's date or time as text, or as a field that follows
/// the day; Alt+Shift+D and Alt+Shift+T insert the date (in the format chosen last) and the
/// time at once, as text. Insert > Field... inserts any field, and its window changes one
/// (Edit Field... of the field's context menu, a double click on it). A plain text file keeps
/// no fields, only the date as text.
class FieldCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The book's text the commands act on (the text with the keys); null: none
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief The commands this coordinator gives callbacks
    static constexpr const char* DATE_TIME_COMMAND = "insert.dateTime";
    static constexpr const char* FIELD_COMMAND = "insert.field";
    static constexpr const char* DATE_COMMAND = "insert.date";
    static constexpr const char* TIME_COMMAND = "insert.time";

    /// @brief The setting of the format of the date chosen last (Insert > Date && Time...)
    static constexpr const char* DATE_FORMAT_KEY = "editor.fields.dateFormat";
    /// @brief The setting of Update Automatically, as it was last
    static constexpr const char* DATE_AS_FIELD_KEY = "editor.fields.dateAsField";

    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param textWithKeys The text the commands act on
    /// @param window Over which the windows open, which has the keys of the date and the time
    /// @param parent Parent object
    FieldCoordinator(QTabWidget* centralTabs, TextWithKeys textWithKeys, QWidget* window,
                     QObject* parent = nullptr);

    FieldCoordinator(const FieldCoordinator&) = delete;
    FieldCoordinator& operator=(const FieldCoordinator&) = delete;

    /// @brief Give the commands their callbacks; the window gets the keys of the date and the
    ///        time, which no menu has
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief What the fields of an open text show: of its element in the book; of a text
    ///        outside it (a file of the book, a standalone file) its title @p fileTitle
    [[nodiscard]] editor::FieldContext contextFor(const QString& elementId,
                                                  const QString& fileTitle = QString()) const;

    /// @brief Give every open text its context now, with the time of now
    void refresh();

    /// @brief Insert > Date && Time...: the window of today's date or time at the cursor
    void insertDateTime();

    /// @brief Insert > Field...: the window of a field at the cursor
    void insertField();

    /// @brief Alt+Shift+D: today's date as text, in the format chosen last
    void insertDate();

    /// @brief Alt+Shift+T: the time as text
    void insertTime();

    /// @brief Edit Field...: the window of the field at @p position of @p editor
    /// @return Whether the field changed
    bool editField(editor::BookEditor& editor, int position);

    /// @brief The format Alt+Shift+D gives the date: the one chosen last when it shows the date
    ///        alone, else the long one ("11 October 2026")
    [[nodiscard]] static QString dateFormatOfKeys();

    /// @brief The names of the fields in the window, for a text of the book: its title and its
    ///        words named after its kind ("Chapter Title"), in the order of editor::FIELD_TYPES
    [[nodiscard]] static std::array<QString, editor::FIELD_TYPES.size()> fieldNames(
        const QString& elementId);

    /// @brief Why the field window cannot insert in @p editor (read-only, a plain text file);
    ///        empty: it can
    [[nodiscard]] static QString whyNoField(const editor::BookEditor& editor);

private:
    /// @brief The editors of the open texts, with their elements and the titles of the others
    struct OpenText {
        editor::BookEditor* editor = nullptr;
        QString elementId;
        QString fileTitle;
    };
    [[nodiscard]] QList<OpenText> openTexts() const;

    /// @brief Connect the editors not connected yet (Edit Field...)
    void attachEditors();

    /// @brief The context of an editor of the open texts
    [[nodiscard]] editor::FieldContext contextOf(const editor::BookEditor* editor) const;

    /// @brief The window of a field for @p editor: chosen first @p field
    /// @return The field chosen; std::nullopt: cancelled
    std::optional<editor::TextField> askField(editor::BookEditor& editor,
                                              const editor::TextField& field, bool editing);

    QTabWidget* m_centralTabs = nullptr;
    TextWithKeys m_textWithKeys;
    QWidget* m_window = nullptr;
    QList<editor::BookEditor*> m_attached;
};

}  // namespace kalahari::gui
