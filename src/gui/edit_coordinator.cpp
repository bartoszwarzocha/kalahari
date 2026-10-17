/// @file edit_coordinator.cpp
/// @brief Implementation of EditCoordinator

#include "kalahari/gui/edit_coordinator.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotations_panel.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>

#include <array>
#include <utility>

namespace kalahari::gui {

namespace {

using Edit = EditCoordinator::Edit;

/// The commands and their ids
constexpr std::array<std::pair<Edit, const char*>, 6> COMMANDS = {{
    {Edit::Undo, "edit.undo"},
    {Edit::Redo, "edit.redo"},
    {Edit::Cut, "edit.cut"},
    {Edit::Copy, "edit.copy"},
    {Edit::Paste, "edit.paste"},
    {Edit::SelectAll, "edit.selectAll"},
}};

/// Whether a widget is a toolbar of the window, on its edge or floating (a panel's own
/// toolbar is not)
bool isWindowToolbar(const QWidget* widget) {
    return widget != nullptr && qobject_cast<const QToolBar*>(widget) != nullptr &&
           qobject_cast<const QMainWindow*>(widget->parentWidget()) != nullptr;
}

/// Whether a widget takes the keys only to give the window's commands: the menu bar in its
/// keyboard mode, a menu, a toolbar of the window and its buttons (a floating toolbar's
/// window may give them the keys)
bool givesCommands(const QWidget* widget) {
    if (qobject_cast<const QMenuBar*>(widget) != nullptr ||
        qobject_cast<const QMenu*>(widget) != nullptr || isWindowToolbar(widget)) {
        return true;
    }
    return qobject_cast<const QToolButton*>(widget) != nullptr &&
           isWindowToolbar(widget->parentWidget());
}

/// Whether the clipboard holds text (its formats only: reading the text may ask another
/// program for it)
bool clipboardHasText() {
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    return data != nullptr && data->hasText();
}

/// A field of one line, also the one in a spin box or in a combo box one types into
QLineEdit* lineOf(QWidget* element) {
    if (auto* line = qobject_cast<QLineEdit*>(element)) {
        return line;
    }
    if (const auto* spinBox = qobject_cast<QAbstractSpinBox*>(element)) {
        return spinBox->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
    }
    if (const auto* comboBox = qobject_cast<QComboBox*>(element)) {
        return comboBox->lineEdit();
    }
    return nullptr;
}

bool lineCan(const QLineEdit& line, Edit edit) {
    const bool editable = !line.isReadOnly();
    // A field of a password shows no text, and copies none
    const bool copies = line.hasSelectedText() && line.echoMode() == QLineEdit::Normal;
    switch (edit) {
    case Edit::Undo:
        return editable && line.isUndoAvailable();
    case Edit::Redo:
        return editable && line.isRedoAvailable();
    case Edit::Cut:
        return editable && copies;
    case Edit::Copy:
        return copies;
    case Edit::Paste:
        return editable && clipboardHasText();
    case Edit::SelectAll:
        return !line.text().isEmpty();
    }
    return false;
}

/// QTextEdit and QPlainTextEdit
template <typename Text>
bool textCan(const Text& text, Edit edit) {
    const bool editable = !text.isReadOnly();
    const bool selected = text.textCursor().hasSelection();
    const QTextDocument* document = text.document();
    switch (edit) {
    case Edit::Undo:
        return editable && text.isUndoRedoEnabled() && document->isUndoAvailable();
    case Edit::Redo:
        return editable && text.isUndoRedoEnabled() && document->isRedoAvailable();
    case Edit::Cut:
        return editable && selected;
    case Edit::Copy:
        return selected;
    case Edit::Paste:
        return editable && text.canPaste();
    case Edit::SelectAll:
        return !document->isEmpty();
    }
    return false;
}

/// QLineEdit, QTextEdit and QPlainTextEdit
template <typename Field>
void fieldDo(Field& field, Edit edit) {
    switch (edit) {
    case Edit::Undo:
        field.undo();
        break;
    case Edit::Redo:
        field.redo();
        break;
    case Edit::Cut:
        field.cut();
        break;
    case Edit::Copy:
        field.copy();
        break;
    case Edit::Paste:
        field.paste();
        break;
    case Edit::SelectAll:
        field.selectAll();
        break;
    }
}

bool bookCan(const editor::BookEditor& book, Edit edit) {
    switch (edit) {
    case Edit::Undo:
        return book.canUndo();
    case Edit::Redo:
        return book.canRedo();
    case Edit::Cut:
        // A read-only text (a chapter of a book open read-only) is copied, not cut; Undo,
        // Redo and Paste ask it themselves
        return book.hasSelection() && !book.isReadOnly();
    case Edit::Copy:
        return book.hasSelection();
    case Edit::Paste:
        return book.canPaste();
    case Edit::SelectAll:
        return true;
    }
    return false;
}

void bookDo(editor::BookEditor& book, Edit edit) {
    switch (edit) {
    case Edit::Undo:
        book.undo();
        break;
    case Edit::Redo:
        book.redo();
        break;
    case Edit::Cut:
        book.cut();
        break;
    case Edit::Copy:
        book.copy();
        break;
    case Edit::Paste:
        book.paste();
        break;
    case Edit::SelectAll:
        book.selectAll();
        break;
    }
}

/// The text of a list's current item, as the list shows it
QString currentItemText(const QAbstractItemView& list) {
    const QModelIndex current = list.currentIndex();
    return current.isValid() ? current.data(Qt::DisplayRole).toString() : QString();
}

/// A list copies the text of its current item, as Qt's lists do with Ctrl+C, and selects all
/// its items where it can select more than one
bool listCan(const QAbstractItemView& list, Edit edit) {
    switch (edit) {
    case Edit::Copy:
        return !currentItemText(list).isEmpty();
    case Edit::SelectAll: {
        const QAbstractItemView::SelectionMode mode = list.selectionMode();
        const bool many = mode == QAbstractItemView::MultiSelection ||
                          mode == QAbstractItemView::ExtendedSelection ||
                          mode == QAbstractItemView::ContiguousSelection;
        return many && list.model() != nullptr && list.model()->rowCount(list.rootIndex()) > 0;
    }
    default:
        return false;
    }
}

}  // namespace

/// What the commands work on for an element: one of these, or nothing
struct EditCoordinator::Target {
    QLineEdit* line = nullptr;
    QTextEdit* text = nullptr;
    QPlainTextEdit* plainText = nullptr;
    QAbstractItemView* list = nullptr;
    editor::BookEditor* book = nullptr;     ///< The book's text: every command
    editor::BookEditor* history = nullptr;  ///< The text's undo history only: Undo and Redo
};

EditCoordinator::EditCoordinator(EditorInFront editorInFront, QObject* parent)
    : QObject(parent), m_editorInFront(std::move(editorInFront)) {}

EditCoordinator::~EditCoordinator() {
    // Their states asked this coordinator
    auto& registry = CommandRegistry::getInstance();
    for (const auto& command : COMMANDS) {
        if (Command* found = registry.getCommand(command.second)) {
            found->isEnabled = nullptr;
        }
    }
}

void EditCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    for (const auto& [edit, id] : COMMANDS) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [this, edit = edit]() { return canDo(edit, focusedElement()); };
        }
    }

    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        connect(app, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            if (now != nullptr && !givesCommands(now)) {
                setLastFocus(now);
            }
            updateStates();
        });
    }
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this,
            &EditCoordinator::updateStates);
    QWidget* focus = QApplication::focusWidget();
    setLastFocus(focus != nullptr && !givesCommands(focus) ? focus : nullptr);
    updateStates();
}

void EditCoordinator::setLastFocus(QWidget* element) {
    if (element == m_lastFocus) {
        return;
    }
    disconnect(m_lastFocusGone);
    m_lastFocus = element;
    if (element != nullptr) {
        m_lastFocusGone =
            connect(element, &QObject::destroyed, this, [this]() { m_lastFocus = nullptr; });
    }
}

QWidget* EditCoordinator::focusedElement() const {
    QWidget* focus = QApplication::focusWidget();
    if (focus == nullptr) {
        // A floating toolbar's window may be active with no widget that has the keys
        return qobject_cast<const QToolBar*>(QApplication::activeWindow()) != nullptr
                   ? m_lastFocus
                   : nullptr;
    }
    return givesCommands(focus) ? m_lastFocus : focus;
}

EditCoordinator::Target EditCoordinator::targetOf(QWidget* element) const {
    Target target;
    if (element == nullptr) {
        return target;
    }
    if (QLineEdit* line = lineOf(element)) {
        target.line = line;
        return target;
    }
    if (auto* text = qobject_cast<QTextEdit*>(element)) {
        target.text = text;
        return target;
    }
    if (auto* plainText = qobject_cast<QPlainTextEdit*>(element)) {
        target.plainText = plainText;
        return target;
    }
    if (auto* book = qobject_cast<editor::BookEditor*>(element)) {
        target.book = book;
        return target;
    }
    if (auto* list = qobject_cast<QAbstractItemView*>(element)) {
        target.list = list;
        return target;
    }

    // What is done in the find bar and in the Annotations panel are steps of the text's undo
    // history: the text in the find bar's book, the one in front for the panel
    for (QWidget* part = element; part != nullptr; part = part->parentWidget()) {
        if (qobject_cast<editor::FindReplaceBar*>(part) != nullptr) {
            target.history = qobject_cast<editor::BookEditor*>(part->parentWidget());
            break;
        }
        if (qobject_cast<AnnotationsPanel*>(part) != nullptr) {
            target.history = m_editorInFront ? m_editorInFront() : nullptr;
            break;
        }
        if (part->isWindow()) {
            break;
        }
    }
    return target;
}

bool EditCoordinator::canDo(Edit edit, QWidget* element) const {
    const Target target = targetOf(element);
    if (target.line != nullptr) {
        return lineCan(*target.line, edit);
    }
    if (target.text != nullptr) {
        return textCan(*target.text, edit);
    }
    if (target.plainText != nullptr) {
        return textCan(*target.plainText, edit);
    }
    if (target.book != nullptr) {
        return bookCan(*target.book, edit);
    }
    if (target.history != nullptr) {
        return (edit == Edit::Undo && target.history->canUndo()) ||
               (edit == Edit::Redo && target.history->canRedo());
    }
    if (target.list != nullptr) {
        return listCan(*target.list, edit);
    }
    return false;
}

bool EditCoordinator::execute(Edit edit, QWidget* element) {
    if (!canDo(edit, element)) {
        return false;
    }
    const Target target = targetOf(element);
    if (target.line != nullptr) {
        fieldDo(*target.line, edit);
    } else if (target.text != nullptr) {
        fieldDo(*target.text, edit);
    } else if (target.plainText != nullptr) {
        fieldDo(*target.plainText, edit);
    } else if (target.book != nullptr) {
        bookDo(*target.book, edit);
    } else if (target.history != nullptr) {
        bookDo(*target.history, edit);  // Undo or Redo: canDo() allows no other
    } else if (target.list != nullptr) {
        if (edit == Edit::Copy) {
            QGuiApplication::clipboard()->setText(currentItemText(*target.list));
        } else {
            target.list->selectAll();
        }
    }
    updateStates();
    return true;
}

void EditCoordinator::updateStates() {
    follow(focusedElement());
    auto& registry = CommandRegistry::getInstance();
    for (const auto& command : COMMANDS) {
        registry.updateActionState(command.second);
    }
}

void EditCoordinator::follow(QWidget* element) {
    const Target target = targetOf(element);
    QObject* text = target.book != nullptr ? target.book : target.history;
    if (element == m_followed && text == m_followedText) {
        return;
    }
    stopFollowing();
    m_followed = element;
    m_followedText = text;

    const auto update = [this]() { updateStates(); };
    const auto add = [this](const QMetaObject::Connection& connection) {
        m_connections.push_back(connection);
    };
    if (target.line != nullptr) {
        add(connect(target.line, &QLineEdit::selectionChanged, this, update));
        add(connect(target.line, &QLineEdit::textChanged, this, update));
    } else if (target.text != nullptr) {
        add(connect(target.text, &QTextEdit::selectionChanged, this, update));
        add(connect(target.text, &QTextEdit::textChanged, this, update));
        add(connect(target.text, &QTextEdit::undoAvailable, this, update));
        add(connect(target.text, &QTextEdit::redoAvailable, this, update));
    } else if (target.plainText != nullptr) {
        add(connect(target.plainText, &QPlainTextEdit::selectionChanged, this, update));
        add(connect(target.plainText, &QPlainTextEdit::textChanged, this, update));
        add(connect(target.plainText, &QPlainTextEdit::undoAvailable, this, update));
        add(connect(target.plainText, &QPlainTextEdit::redoAvailable, this, update));
    } else if (target.list != nullptr) {
        if (QItemSelectionModel* selection = target.list->selectionModel()) {
            add(connect(selection, &QItemSelectionModel::currentChanged, this, update));
        }
        if (QAbstractItemModel* model = target.list->model()) {
            add(connect(model, &QAbstractItemModel::rowsInserted, this, update));
            add(connect(model, &QAbstractItemModel::rowsRemoved, this, update));
            add(connect(model, &QAbstractItemModel::modelReset, this, update));
            add(connect(model, &QAbstractItemModel::dataChanged, this, update));
        }
    }
    if (auto* book = qobject_cast<editor::BookEditor*>(text)) {
        add(connect(book, &editor::BookEditor::selectionChanged, this, update));
        add(connect(book, &editor::BookEditor::contentChanged, this, update));
        add(connect(book, &editor::BookEditor::documentChanged, this, update));
    }

    const auto forget = [this]() { stopFollowing(); };
    if (element != nullptr) {
        add(connect(element, &QObject::destroyed, this, forget));
    }
    if (text != nullptr && text != element) {
        add(connect(text, &QObject::destroyed, this, forget));
    }
}

void EditCoordinator::stopFollowing() {
    for (const QMetaObject::Connection& connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
    m_followed = nullptr;
    m_followedText = nullptr;
}

}  // namespace kalahari::gui
