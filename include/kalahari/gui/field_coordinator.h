// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file field_coordinator.h
/// @brief The fields of the open texts: what they show of the book, Insert > Date && Time...,
///        Insert > Field..., the date and the time at once (Alt+Shift+D, Alt+Shift+T) and
///        Edit Field...

#ifndef KALAHARI_GUI_FIELD_COORDINATOR_H
#define KALAHARI_GUI_FIELD_COORDINATOR_H

#include "kalahari/editor/shared_history.h"
#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/own_fields.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <functional>
#include <optional>

class QTabWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}  // namespace kalahari::editor

namespace kalahari::gui {

class CommandRegistry;
class EditorPanel;

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
///
/// The own fields of the book (own_fields.h) are made, changed and deleted in the tab Own of
/// Insert > Field... and in the Properties panel. A field deleted while the texts have it:
/// its places become the text of its value, or empty fields, in every text of the book, the
/// open ones and the others; Undo in the bar under the text, or the writer's Undo in an open
/// text, brings it back everywhere.
class FieldCoordinator : public QObject, public editor::SharedHistory {
    Q_OBJECT

public:
    /// @brief Says something under the text of a tab, with a button
    ///        (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief Asks how to delete an own field the texts have: std::nullopt: not at all; true:
    ///        its places become text; false: empty fields (the window asks; the tests answer)
    using DeleteAsker = std::function<std::optional<bool>(const OwnField& field, int uses,
                                                          int texts)>;

    /// @brief The book's text the commands act on (the text with the keys); null: none
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief The commands this coordinator gives callbacks: Insert > Date && Time...
    static constexpr const char* DATE_TIME_COMMAND = "insert.dateTime";
    /// @brief Insert > Field...
    static constexpr const char* FIELD_COMMAND = "insert.field";
    /// @brief The date at once, as text (Alt+Shift+D)
    static constexpr const char* DATE_COMMAND = "insert.date";
    /// @brief The time at once, as text (Alt+Shift+T)
    static constexpr const char* TIME_COMMAND = "insert.time";

    /// @brief The setting of the format of the date chosen last (Insert > Date && Time...)
    static constexpr const char* DATE_FORMAT_KEY = "editor.fields.dateFormat";
    /// @brief The setting of Update Automatically, as it was last
    static constexpr const char* DATE_AS_FIELD_KEY = "editor.fields.dateAsField";

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param textWithKeys The text the commands act on
    /// @param window Over which the windows open, which has the keys of the date and the time
    /// @param parent Parent object
    FieldCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                     QTabWidget* centralTabs, TextWithKeys textWithKeys, QWidget* window,
                     QObject* parent = nullptr);

    ~FieldCoordinator() override;
    FieldCoordinator(const FieldCoordinator&) = delete;
    FieldCoordinator& operator=(const FieldCoordinator&) = delete;
    FieldCoordinator(FieldCoordinator&&) = delete;
    FieldCoordinator& operator=(FieldCoordinator&&) = delete;

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

    /// @brief Insert in Text... of the own fields in the Properties panel: Insert > Field... on
    ///        its tab Own for @p editor, the field @p id chosen (empty: the first)
    /// @return Whether a field went in the text
    bool insertOwnField(editor::BookEditor* editor, const QString& id = QString());

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
    [[nodiscard]] std::array<QString, editor::FIELD_TYPES.size()> fieldNames(
        const QString& elementId) const;

    /// @brief Why the field window cannot insert in @p editor (read-only, a plain text file);
    ///        empty: it can
    [[nodiscard]] static QString whyNoField(const editor::BookEditor& editor);

    /// @brief Where the notice of a field deleted shows, with Undo
    void setNoticeShower(NoticeShower shower) { m_showNotice = std::move(shower); }

    /// @brief Replace the window that asks how to delete a used field (for the tests)
    void setDeleteAsker(DeleteAsker asker) { m_askDelete = std::move(asker); }

    /// @brief New Field...: the window of a new own field of the volume @p bookId (empty: the
    ///        current book) over @p over
    /// @return The id of the field made; empty: none
    QString newOwnField(const QString& bookId, QWidget* over);

    /// @brief Change...: the window of an own field
    /// @return Whether it changed
    bool changeOwnField(const QString& id, const QString& bookId, QWidget* over);

    /// @brief Delete an own field: once its places in the texts became text or empty fields,
    ///        as the window asks (a field the texts do not have goes at once); the bar under
    ///        the text says so, with Undo
    /// @return Whether it is gone
    bool deleteOwnField(const QString& id, const QString& bookId, QWidget* over);

    /// @brief How many times the texts of the book have an own field, and how many texts
    struct FieldUses {
        int count = 0;
        int texts = 0;
    };
    [[nodiscard]] FieldUses usesOf(const QString& id) const;

    /// @brief Whether the last field deleted can still come back
    [[nodiscard]] bool canUndoDelete() const;

    /// @brief Bring the last field deleted back, in every text it was in
    bool undoDelete();

    // The writer's Undo in an open text brings the field deleted last back (editor::SharedHistory)
    [[nodiscard]] bool undoesFor(const editor::BookEditor& text) const override;
    [[nodiscard]] bool redoesFor(const editor::BookEditor& text) const override;
    void undoFor(editor::BookEditor& text) override;
    void redoFor(editor::BookEditor& text) override;

private:
    /// @brief A text not open that the deletion changed: its text before
    struct SavedText {
        QString elementId;
        QString kml;
        bool saved = false;  ///< Its file was saved with the change
    };

    /// @brief The last own field deleted, to bring back
    struct DeletedField {
        OwnField field;
        QString bookId;
        QList<QPointer<editor::BookEditor>> editors;  ///< The open texts it changed
        QList<SavedText> savedTexts;                  ///< The texts not open it changed
    };

    /// @brief The elements of the texts that can have the field: of its volume, or of every
    ///        volume for a field of the series
    [[nodiscard]] QStringList textsOf(const QString& id) const;

    /// @brief Forget the last field deleted: the open texts' Undo is their own again
    void forgetDelete();

    /// @brief The book whose own fields a text of @p elementId has: its volume's id
    [[nodiscard]] QString bookIdOf(const QString& elementId) const;

    /// @brief Over which a window of the own fields opens: @p over, else Insert > Field...
    ///        while it is open, else the book window
    [[nodiscard]] QWidget* dialogParent(QWidget* over) const;
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

    /// @brief The window of a field, opened as @p start says, and the field at the cursor
    bool insertFieldIn(editor::BookEditor& editor, const editor::TextField& start);

    /// @brief The window of a field for @p editor: chosen first @p field
    /// @return The field chosen; std::nullopt: cancelled
    std::optional<editor::TextField> askField(editor::BookEditor& editor,
                                              const editor::TextField& field, bool editing);

    QTabWidget* m_centralTabs = nullptr;
    TextWithKeys m_textWithKeys;
    QWidget* m_window = nullptr;
    QList<editor::BookEditor*> m_attached;
    NoticeShower m_showNotice;
    DeleteAsker m_askDelete;
    std::optional<DeletedField> m_deleted;
    /// The open texts whose Undo brings the field back, and the history they asked before
    QList<std::pair<QPointer<editor::BookEditor>, editor::SharedHistory*>> m_sharing;
    QList<QMetaObject::Connection> m_deleteWatch;
    bool m_undoing = false;
    QPointer<QWidget> m_fieldDialog;  ///< Insert > Field... while it is open
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_FIELD_COORDINATOR_H
