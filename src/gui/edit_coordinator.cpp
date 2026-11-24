// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file edit_coordinator.cpp
/// @brief Implementation of EditCoordinator

#include "kalahari/gui/edit_coordinator.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/copy_target.h"
#include "kalahari/gui/edit_target.h"
#include "kalahari/gui/paste_target.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/spelling_panel.h"

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
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::gui {

namespace {

using Edit = EditCoordinator::Edit;

/// The commands and their ids
constexpr std::array<std::pair<Edit, const char*>, 7> COMMANDS = {{
    {Edit::Undo, "edit.undo"},
    {Edit::Redo, "edit.redo"},
    {Edit::Cut, "edit.cut"},
    {Edit::Copy, "edit.copy"},
    {Edit::Paste, "edit.paste"},
    {Edit::Delete, "edit.delete"},
    {Edit::SelectAll, "edit.selectAll"},
}};

/// The categories of the commands that work on the text in front (the ones of the Edit,
/// Insert, Format, Table, Image, View and Tools menus and the Book menu's Scene Break)
constexpr std::array<const char*, 8> TEXT_CATEGORIES = {"EDIT",  "INSERT", "FORMAT", "TABLE",
                                                        "IMAGE", "VIEW",   "TOOLS",  "BOOK"};

/// Whether @p edit changes what it works on
bool changes(Edit edit) {
    return edit != Edit::Copy && edit != Edit::SelectAll;
}

/// Why @p edit does nothing in a text or a field that it works in
QString nothingFor(Edit edit) {
    switch (edit) {
    case Edit::Undo:
        //: Why Edit > Undo is greyed out. In Polish: Nie ma czego cofnąć
        return EditCoordinator::tr("There is nothing to undo");
    case Edit::Redo:
        //: Why Edit > Redo is greyed out. In Polish: Nie ma czego ponowić
        return EditCoordinator::tr("There is nothing to redo");
    case Edit::Cut:
    case Edit::Copy:
    case Edit::Delete:
        //: Why Edit > Cut, Copy and Delete are greyed out. In Polish: Nic nie jest zaznaczone
        return EditCoordinator::tr("Nothing is selected");
    case Edit::Paste:
        //: Why Edit > Paste is greyed out. In Polish: W schowku nie ma niczego, co można tu
        //: wkleić
        return EditCoordinator::tr("The clipboard holds nothing to paste here");
    case Edit::SelectAll:
        //: Why Edit > Select All is greyed out in an empty field. In Polish: Nie ma czego
        //: zaznaczyć
        return EditCoordinator::tr("There is nothing to select");
    }
    return {};
}

/// Why a command that changes the text is greyed out in a read-only text or field
QString cannotChange() {
    //: Why Undo, Redo, Cut, Paste, Delete and the commands that change the text are greyed
    //: out in a text or a field that only shows. In Polish: Tego tekstu nie można zmieniać
    return EditCoordinator::tr("This text cannot be changed");
}

/// Why a command of the text is greyed out with no text in front (the Dashboard, no tab)
QString noTextInFront() {
    //: Why the commands of the text are greyed out with no text in front (the Dashboard, no
    //: tab). In Polish: Na wierzchu nie ma tekstu
    return EditCoordinator::tr("No text is in front");
}

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
    case Edit::Delete:
        return editable && line.hasSelectedText();
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
    case Edit::Delete:
        return editable && selected;
    case Edit::SelectAll:
        return !document->isEmpty();
    }
    return false;
}

/// Delete the selection of a field (Delete is enabled only with one)
void deleteSelection(QLineEdit& line) {
    line.del();
}

/// Delete the selection of a field, as one step of its undo history
template <typename Text>
void deleteSelection(Text& text) {
    QTextCursor cursor = text.textCursor();
    cursor.removeSelectedText();
    text.setTextCursor(cursor);
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
    case Edit::Delete:
        deleteSelection(field);
        break;
    case Edit::SelectAll:
        field.selectAll();
        break;
    }
}

bool bookCan(const editor::BookEditor& book, Edit edit) {
    switch (edit) {
    case Edit::Undo:
        return book.canUndoCommand();
    case Edit::Redo:
        return book.canRedoCommand();
    case Edit::Cut:
        // A read-only text (a chapter of a book open read-only) is copied, not cut; Undo,
        // Redo and Paste ask it themselves
        return book.hasSelection() && !book.isReadOnly();
    case Edit::Copy:
        return book.hasSelection();
    case Edit::Paste:
        return book.canPaste();
    case Edit::Delete:
        return book.hasSelection() && !book.isReadOnly();
    case Edit::SelectAll:
        return true;
    }
    return false;
}

void bookDo(editor::BookEditor& book, Edit edit) {
    switch (edit) {
    case Edit::Undo:
        book.undoCommand();
        break;
    case Edit::Redo:
        book.redoCommand();
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
    case Edit::Delete:
        book.deleteSelectedText();
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
/// its items where it can select more than one; one with Paste of its own (the Navigator)
/// pastes
bool listCan(const QAbstractItemView& list, Edit edit) {
    switch (edit) {
    case Edit::Copy:
        return !currentItemText(list).isEmpty();
    case Edit::Paste: {
        const auto* target = dynamic_cast<const PasteTarget*>(&list);
        return target != nullptr && target->canPasteHere();
    }
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

/// Copy, Paste and Select All in a list (listCan() allows no other)
void listDo(QAbstractItemView& list, Edit edit) {
    if (edit == Edit::Copy) {
        QGuiApplication::clipboard()->setText(currentItemText(list));
    } else if (edit == Edit::Paste) {
        dynamic_cast<PasteTarget&>(list).pasteHere();  // listCan() asked it
    } else {
        list.selectAll();
    }
}

/// Copy and Select All in a view with text to copy (the preview of a PDF file)
bool copyCan(const CopyTarget& view, Edit edit) {
    return (edit == Edit::Copy && view.canCopyHere()) ||
           (edit == Edit::SelectAll && view.canSelectAllHere());
}

/// Copy or Select All in a view with text to copy (copyCan() allows no other)
void copyDo(CopyTarget& view, Edit edit) {
    if (edit == Edit::Copy) {
        view.copyHere();
    } else {
        view.selectAllHere();
    }
}

/// The view with commands of its own that @p element is in, up to its window; nullptr: none
EditTarget* ownEditsOf(QWidget* element) {
    for (QWidget* part = element; part != nullptr; part = part->parentWidget()) {
        if (auto* own = dynamic_cast<EditTarget*>(part)) {
            return own;
        }
        if (part->isWindow()) {
            break;
        }
    }
    return nullptr;
}

}  // namespace

/// What the commands work on for an element: one of these, or nothing
struct EditCoordinator::Target {
    QLineEdit* line = nullptr;
    QTextEdit* text = nullptr;
    QPlainTextEdit* plainText = nullptr;
    QAbstractItemView* list = nullptr;
    CopyTarget* copy = nullptr;             ///< A view with text to copy: Copy and Select All
    editor::BookEditor* book = nullptr;     ///< The book's text: every command
    editor::BookEditor* history = nullptr;  ///< The text's undo history only: Undo and Redo
    EditTarget* own = nullptr;  ///< A view that does the commands it takes itself

    /// Whether it is a field or the book's text, where the commands are theirs
    [[nodiscard]] bool isField() const {
        return line != nullptr || text != nullptr || plainText != nullptr || book != nullptr;
    }
    /// Whether the view does @p edit itself
    [[nodiscard]] bool ownTakes(Edit edit) const { return own != nullptr && own->takesEdit(edit); }
};

EditCoordinator::EditCoordinator(CommandRegistry& commands, EditorInFront editorInFront,
                                 QObject* parent)
    : QObject(parent), m_editorInFront(std::move(editorInFront)), m_commands(commands) {}

EditCoordinator::~EditCoordinator() {
    // Their states and the reasons asked this coordinator
    auto& registry = m_commands;
    registry.setFallbackReason(nullptr);
    for (const auto& command : COMMANDS) {
        if (Command* found = registry.getCommand(command.second)) {
            found->isEnabled = nullptr;
            found->disabledReason = nullptr;
        }
    }
    for (const auto& [id, can] : m_textCommands) {
        if (Command* found = registry.getCommand(id)) {
            found->isEnabled = nullptr;
        }
    }
}

void EditCoordinator::connectCommands() {
    auto& registry = m_commands;
    for (const auto& [edit, id] : COMMANDS) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [this, edit = edit]() { return canDo(edit, focusedElement()); };
            command->disabledReason = [this, edit = edit]() {
                return whyNot(edit, focusedElement());
            };
        }
    }
    // The other commands of the text say that no text is in front, or that the keys are not
    // in it (test of 22.11: greyed out with no explanation)
    registry.setFallbackReason(
        [this](const Command& command) { return fallbackReason(command); });
    registry.updateAllActionStates();

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

editor::BookEditor* EditCoordinator::textWithKeys() const {
    return targetOf(focusedElement()).book;
}

void EditCoordinator::addTextCommands(const std::vector<std::string>& ids, const TextCan& can) {
    auto& registry = m_commands;
    for (const std::string& id : ids) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [this, can]() {
                const editor::BookEditor* text = textWithKeys();
                return text != nullptr && can(*text);
            };
            m_textCommands.emplace_back(id, can);
            registry.updateActionState(id);
        }
    }
}

QString EditCoordinator::fallbackReason(const Command& command) const {
    const editor::BookEditor* front = m_editorInFront ? m_editorInFront() : nullptr;
    const bool textCommand =
        std::any_of(m_textCommands.cbegin(), m_textCommands.cend(),
                    [&command](const auto& entry) { return entry.first == command.id; });
    if (textCommand) {
        const editor::BookEditor* text = textWithKeys();
        if (text == nullptr) {
            //: Why the commands of the book's text are greyed out with the keys in a field or
            //: a panel. In Polish: Kursor nie jest w tekście książki
            return front != nullptr ? tr("The cursor is not in a text of the book")
                                    : noTextInFront();
        }
        return text->isReadOnly() ? cannotChange() : QString();
    }
    const bool ofText = std::any_of(TEXT_CATEGORIES.cbegin(), TEXT_CATEGORIES.cend(),
                                    [&command](const char* category) {
                                        return command.category == category;
                                    });
    return ofText && front == nullptr ? noTextInFront() : QString();
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
    if (element == nullptr) {
        return {};
    }
    Target target = elementTargetOf(element);
    if (target.isField()) {
        return target;
    }
    // Outside a field, a view with commands of its own does them (the plot board, also in
    // its outline); what is done in a panel may be a step of the text's undo history
    target.own = ownEditsOf(element);
    if (target.list == nullptr && target.copy == nullptr) {
        target.history = historyOf(element);
    }
    return target;
}

EditCoordinator::Target EditCoordinator::elementTargetOf(QWidget* element) {
    Target target;
    if (QLineEdit* line = lineOf(element)) {
        target.line = line;
    } else if (auto* text = qobject_cast<QTextEdit*>(element)) {
        target.text = text;
    } else if (auto* plainText = qobject_cast<QPlainTextEdit*>(element)) {
        target.plainText = plainText;
    } else if (auto* book = qobject_cast<editor::BookEditor*>(element)) {
        target.book = book;
    } else if (auto* list = qobject_cast<QAbstractItemView*>(element)) {
        target.list = list;
    } else {
        target.copy = dynamic_cast<CopyTarget*>(element);
    }
    return target;
}

editor::BookEditor* EditCoordinator::historyOf(QWidget* element) const {
    // What is done in the find bar and in the Annotations, the Spelling and Grammar and the
    // Search panels (Replace All in Book) are steps of the text's undo history: the text in
    // the find bar's book, the one in front for the panels
    for (QWidget* part = element; part != nullptr; part = part->parentWidget()) {
        if (qobject_cast<editor::FindReplaceBar*>(part) != nullptr) {
            return qobject_cast<editor::BookEditor*>(part->parentWidget());
        }
        if (qobject_cast<AnnotationsPanel*>(part) != nullptr ||
            qobject_cast<SpellingPanel*>(part) != nullptr ||
            qobject_cast<SearchPanel*>(part) != nullptr) {
            return m_editorInFront ? m_editorInFront() : nullptr;
        }
        if (part->isWindow()) {
            break;
        }
    }
    return nullptr;
}

bool EditCoordinator::fieldCan(const Target& target, Edit edit) {
    if (target.line != nullptr) {
        return lineCan(*target.line, edit);
    }
    if (target.text != nullptr) {
        return textCan(*target.text, edit);
    }
    if (target.plainText != nullptr) {
        return textCan(*target.plainText, edit);
    }
    return target.book != nullptr && bookCan(*target.book, edit);
}

void EditCoordinator::fieldDoOn(const Target& target, Edit edit) {
    if (target.line != nullptr) {
        fieldDo(*target.line, edit);
    } else if (target.text != nullptr) {
        fieldDo(*target.text, edit);
    } else if (target.plainText != nullptr) {
        fieldDo(*target.plainText, edit);
    } else if (target.book != nullptr) {
        bookDo(*target.book, edit);
    }
}

bool EditCoordinator::saysItself(Edit edit) const {
    return targetOf(focusedElement()).ownTakes(edit);
}

bool EditCoordinator::canDo(Edit edit, QWidget* element) const {
    const Target target = targetOf(element);
    if (target.isField()) {
        return fieldCan(target, edit);
    }
    if (target.ownTakes(edit)) {
        return target.own->canEditHere(edit);
    }
    if (target.history != nullptr) {
        return (edit == Edit::Undo && target.history->canUndoCommand()) ||
               (edit == Edit::Redo && target.history->canRedoCommand());
    }
    if (target.copy != nullptr) {
        return copyCan(*target.copy, edit);
    }
    return target.list != nullptr && listCan(*target.list, edit);
}

QString EditCoordinator::whyNot(Edit edit, QWidget* element) const {
    if (canDo(edit, element)) {
        return {};
    }
    const Target target = targetOf(element);
    if (target.isField()) {
        if (target.line != nullptr && target.line->echoMode() != QLineEdit::Normal &&
            target.line->hasSelectedText() && (edit == Edit::Cut || edit == Edit::Copy)) {
            //: Why Cut and Copy are greyed out in a field of a password. In Polish: Hasła się
            //: nie kopiuje
            return tr("A password is not copied");
        }
        const bool readOnly = (target.line != nullptr && target.line->isReadOnly()) ||
                              (target.text != nullptr && target.text->isReadOnly()) ||
                              (target.plainText != nullptr && target.plainText->isReadOnly()) ||
                              (target.book != nullptr && target.book->isReadOnly());
        return readOnly && changes(edit) ? cannotChange() : nothingFor(edit);
    }
    const bool undoRedo = edit == Edit::Undo || edit == Edit::Redo;
    const bool copies = edit == Edit::Copy || edit == Edit::SelectAll;
    if (target.ownTakes(edit) || (target.history != nullptr && undoRedo) ||
        (target.copy != nullptr && copies)) {
        return nothingFor(edit);
    }
    if (target.list != nullptr) {
        if (edit == Edit::Copy) {
            //: Why Edit > Copy is greyed out in a list. In Polish: Żaden element listy nie
            //: jest wybrany
            return tr("No item of the list is chosen");
        }
        if (edit == Edit::SelectAll) {
            const QAbstractItemModel* model = target.list->model();
            return model != nullptr && model->rowCount(target.list->rootIndex()) > 0
                       //: Why Edit > Select All is greyed out in a list. In Polish: Na tej
                       //: liście wybiera się jeden element naraz
                       ? tr("This list chooses one item at a time")
                       //: Why Edit > Select All is greyed out in an empty list. In Polish:
                       //: Lista jest pusta
                       : tr("The list is empty");
        }
        if (edit == Edit::Paste && dynamic_cast<const PasteTarget*>(target.list) != nullptr) {
            return nothingFor(edit);
        }
        //: Why Undo, Redo, Cut, Paste and Delete are greyed out in a list. In Polish: Na liście
        //: nie można tego zrobić
        return tr("This is not possible in a list");
    }
    //: Why the Edit commands are greyed out with the keys outside a text and a field (on a
    //: button, in the Dashboard). In Polish: Kursor nie jest w tekście ani w polu
    return tr("The cursor is not in a text or a field");
}

bool EditCoordinator::execute(Edit edit, QWidget* element) {
    if (!canDo(edit, element)) {
        return false;
    }
    const Target target = targetOf(element);
    if (target.isField()) {
        fieldDoOn(target, edit);
    } else if (target.ownTakes(edit)) {
        target.own->editHere(edit);
    } else if (target.history != nullptr) {
        bookDo(*target.history, edit);  // Undo or Redo: canDo() allows no other
    } else if (target.list != nullptr) {
        listDo(*target.list, edit);
    } else if (target.copy != nullptr) {
        copyDo(*target.copy, edit);
    }
    updateStates();
    return true;
}

void EditCoordinator::updateStates() {
    follow(focusedElement());
    auto& registry = m_commands;
    for (const auto& command : COMMANDS) {
        registry.updateActionState(command.second);
    }
    for (const auto& [id, can] : m_textCommands) {
        registry.updateActionState(id);
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
    followElement(target);
    if (auto* book = qobject_cast<editor::BookEditor*>(text)) {
        m_connections.push_back(
            connect(book, &editor::BookEditor::selectionChanged, this, update));
        m_connections.push_back(connect(book, &editor::BookEditor::contentChanged, this, update));
        m_connections.push_back(
            connect(book, &editor::BookEditor::documentChanged, this, update));
    }

    const auto forget = [this]() { stopFollowing(); };
    if (element != nullptr) {
        m_connections.push_back(connect(element, &QObject::destroyed, this, forget));
    }
    if (text != nullptr && text != element) {
        m_connections.push_back(connect(text, &QObject::destroyed, this, forget));
    }
}

void EditCoordinator::followElement(const Target& target) {
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
    if (target.own != nullptr) {
        add(target.own->followEditStates(this, update));
    }
    if (target.copy != nullptr) {
        add(target.copy->whenSelectionChanges(this, update));
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
