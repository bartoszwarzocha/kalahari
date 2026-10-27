/// @file quick_insert_controller.h
/// @brief Quick insert in a BookEditor: the list at the cursor, opened by @ or # typed at the
///        start of a word or by Insert > Quick Insert...
///
/// The list does not take the keyboard: the writer goes on typing in the text, and the letters
/// typed after the character narrow the list. Up, Down, Page Up and Page Down choose an item,
/// Enter or Tab inserts it in place of the character and the letters (one undo step), Esc
/// closes the list and leaves the text as typed; a space or a punctuation mark closes it too.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/quick_insert.h>

#include <QFrame>
#include <QList>
#include <QPointer>
#include <QString>

#include <functional>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari::editor {

class BookEditor;

/// @brief The list of quick insert: the items in their groups, what the chosen one inserts
///        and the keys
///
/// A child of the editor that never takes the focus: QuickInsertController moves its choice
/// with the keys of the text.
class QuickInsertList : public QFrame {
    Q_OBJECT

public:
    explicit QuickInsertList(QWidget* parent = nullptr);

    /// @brief Show @p items, the first one chosen; @p typed is what the text has for them
    ///        ("@an"), for the line of the keys and when nothing matches
    void setItems(const QList<QuickInsertItem>& items, const QString& typed);

    /// @brief The items shown
    const QList<QuickInsertItem>& items() const { return m_items; }

    /// @brief The index of the item chosen in items(), or -1 without items
    int current() const { return m_current; }

    /// @brief Choose the item @p index of items() (wrapping around at the ends with move())
    void setCurrent(int index);

    /// @brief Choose the item @p steps after the one chosen (before it when negative); at an
    ///        end the choice wraps around to the other end, for single steps
    void move(int steps);

    /// @brief How many items a page of the list shows (Page Up, Page Down)
    int pageSize() const;

signals:
    /// @brief An item was clicked: it is to be inserted
    void activated(int index);

private:
    void showChoice();

    QTreeWidget* m_tree = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_keys = nullptr;
    QList<QuickInsertItem> m_items;
    QList<QTreeWidgetItem*> m_rows;  ///< The row of each item
    int m_current = -1;
};

/// @brief Quick insert in one editor
class QuickInsertController : public QObject {
    Q_OBJECT

public:
    /// @brief The items for a character that opens the list; QChar() for all of them
    ///        (Insert > Quick Insert...)
    using ItemsFor = std::function<QList<QuickInsertItem>(QChar trigger)>;

    /// @param editor The editor whose text it follows (it is the parent of the list)
    /// @param items The items for each character
    QuickInsertController(BookEditor* editor, ItemsFor items, QObject* parent = nullptr);

    /// @brief The characters that open the list when typed at the start of a word ("@#");
    ///        empty: none (the command still opens it)
    void setTriggers(const QString& triggers);
    QString triggers() const { return m_triggers; }

    /// @brief Open the list at the cursor with all the items (Insert > Quick Insert...)
    void openAtCursor();

    /// @brief Whether the list is open
    bool isOpen() const { return m_open; }

    /// @brief The list (hidden while it is closed)
    QuickInsertList* list() const { return m_list; }

    /// @brief Insert the item chosen in place of the character and the letters typed after it
    /// @return Whether an item was inserted
    bool insertCurrent();

    /// @brief Close the list, leaving the text as it is
    void close();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void open(QChar trigger, const CursorPosition& start);
    void onTextChanged();
    void follow();
    void place();
    bool handleKey(int key);

    QPointer<BookEditor> m_editor;
    ItemsFor m_itemsFor;
    QString m_triggers;
    QuickInsertList* m_list = nullptr;
    QList<QuickInsertItem> m_all;   ///< The items of the character that opened the list
    bool m_open = false;
    QChar m_trigger;                ///< The character that opened the list; QChar(): the command
    CursorPosition m_start;         ///< Where the letters typed after it start
    QChar m_typedTrigger;           ///< A character of m_triggers being typed
    int m_lastFound = 0;            ///< How many letters found items the last time
    bool m_inserting = false;
};

}  // namespace kalahari::editor
