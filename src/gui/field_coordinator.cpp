// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file field_coordinator.cpp
/// @brief The fields of the open texts: what they show of the book, Insert > Date && Time...,
///        Insert > Field..., the date and the time at once and Edit Field...

#include "kalahari/gui/field_coordinator.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/field_dialogs.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/own_fields.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QAction>
#include <QFileInfo>
#include <QSet>
#include <QTabWidget>

#include <algorithm>

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// The formats of the date that show the date alone (Alt+Shift+D gives the date in one)
bool isDateOnly(const QString& format)
{
    return format == QLatin1String("long") || format == QLatin1String("short") ||
           format == QLatin1String("full") || format == QLatin1String("iso");
}

/// The keys of a command as the writer has them ("Alt+Shift+D"); empty: none
QString keysOf(const CommandRegistry& commands, const char* id)
{
    const Command* command = commands.getCommand(id);
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QString();
}

}  // namespace

FieldCoordinator::FieldCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                   QTabWidget* centralTabs, TextWithKeys textWithKeys,
                                   QWidget* window, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_textWithKeys(std::move(textWithKeys)),
      m_window(window),
      m_book(book),
      m_commands(commands)
{
    // Another book: other values
    connect(&m_book, &core::ProjectManager::projectOpened, this, &FieldCoordinator::refresh);
    connect(&m_book, &core::ProjectManager::projectClosed, this, &FieldCoordinator::refresh);
    connect(&m_book, &core::ProjectManager::elementRenamed, this, &FieldCoordinator::refresh);
    // A field of the book made, changed or deleted: its fields in the open texts show it
    connect(&m_book, &core::ProjectManager::customFieldsChanged, this, &FieldCoordinator::refresh);
    connect(&m_book, &core::ProjectManager::projectClosed, this, &FieldCoordinator::forgetDelete);

    // A text opened, closed or brought to the front: its fields show what is now
    if (m_centralTabs != nullptr) {
        const DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        connect(&windows, &DocumentWindows::frontChanged, this, &FieldCoordinator::refresh);
        connect(&windows, &DocumentWindows::windowsChanged, this, &FieldCoordinator::refresh);
        connect(m_centralTabs, &QTabWidget::currentChanged, this, &FieldCoordinator::refresh);
    }
}

void FieldCoordinator::connectCommands()
{
    CommandRegistry& registry = m_commands;
    const auto connectCommand = [&registry](const char* id, std::function<void()> execute) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = std::move(execute);
            registry.updateActionState(id);
        }
    };
    connectCommand(DATE_TIME_COMMAND, [this]() { insertDateTime(); });
    connectCommand(FIELD_COMMAND, [this]() { insertField(); });
    connectCommand(DATE_COMMAND, [this]() { insertDate(); });
    connectCommand(TIME_COMMAND, [this]() { insertTime(); });

    // No menu has the date and the time at once: their keys work in the window that has
    // their actions
    for (const char* id : {DATE_COMMAND, TIME_COMMAND}) {
        if (QAction* action = registry.getAction(QString::fromLatin1(id));
            action != nullptr && m_window != nullptr) {
            m_window->addAction(action);
        }
    }
}

// =============================================================================
// The contexts
// =============================================================================

editor::FieldContext FieldCoordinator::contextFor(const QString& elementId,
                                                  const QString& fileTitle) const
{
    editor::FieldContext context;
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : m_book.findElement(elementId);
    // A text of the book: the values of its volume of a series (of the current one for a
    // material of the Workshop); a file outside it has none, and the program's language
    if (const core::ProjectBook* book = volumeOfText(m_book, elementId);
        book != nullptr && element != nullptr) {
        context.bookTitle = book->title;
        context.author = book->author;
        context.language = book->language;
        context.customFields = own_fields::values(m_book, *book);
    }
    context.textTitle = element != nullptr ? element->title : fileTitle;
    if (context.language.isEmpty()) {
        context.language = programLanguage();
    }
    context.now = QDateTime::currentDateTime();
    return context;
}

QList<FieldCoordinator::OpenText> FieldCoordinator::openTexts() const
{
    QList<OpenText> texts;
    if (m_centralTabs == nullptr) {
        return texts;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel == nullptr || panel->getBookEditor() == nullptr) {
            continue;
        }
        OpenText text;
        text.editor = panel->getBookEditor();
        text.elementId = panel->property("elementId").toString();
        const QString path = panel->property("standaloneFilePath").toString();
        text.fileTitle = !path.isEmpty() ? QFileInfo(path).completeBaseName()
                                         : panel->property("newFileName").toString();
        if (text.fileTitle.isEmpty() && text.elementId.isEmpty()) {
            text.fileTitle = DocumentWindows::tabText(page);
        }
        texts.append(text);
    }
    return texts;
}

editor::FieldContext FieldCoordinator::contextOf(const editor::BookEditor* editor) const
{
    for (const OpenText& text : openTexts()) {
        if (text.editor == editor) {
            return contextFor(text.elementId, text.fileTitle);
        }
    }
    return contextFor(QString());
}

void FieldCoordinator::refresh()
{
    attachEditors();
    for (const OpenText& text : openTexts()) {
        text.editor->setFieldContext(contextFor(text.elementId, text.fileTitle));
    }
}

void FieldCoordinator::attachEditors()
{
    for (const OpenText& text : openTexts()) {
        editor::BookEditor* const editor = text.editor;
        if (m_attached.contains(editor)) {
            continue;
        }
        m_attached.append(editor);
        connect(editor, &editor::BookEditor::fieldEditRequested, this,
                [this, editor](int position) { editField(*editor, position); });
        connect(editor, &QObject::destroyed, this,
                [this, editor]() { m_attached.removeAll(editor); });
    }
}

// =============================================================================
// The commands
// =============================================================================

QString FieldCoordinator::dateFormatOfKeys()
{
    const auto saved =
        core::SettingsManager::getInstance().get<std::string>(DATE_FORMAT_KEY, "long");
    const QString chosen =
        editor::normalizedFieldFormat(editor::FieldType::Date, QString::fromStdString(saved));
    return isDateOnly(chosen) ? chosen : QStringLiteral("long");
}

std::array<QString, editor::FIELD_TYPES.size()> FieldCoordinator::fieldNames(
    const QString& elementId) const
{
    std::array<QString, editor::FIELD_TYPES.size()> names;
    for (size_t i = 0; i < editor::FIELD_TYPES.size(); ++i) {
        names.at(i) = editor::fieldTypeTitle(editor::FIELD_TYPES.at(i));
    }
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : m_book.findElement(elementId);
    if (element == nullptr) {
        return names;
    }
    const core::KindWords words = wordsOf(m_book, *element);
    const QHash<QString, core::KindWords> kind{{QStringLiteral("kind"), words}};
    //: A field: the title of the text it is in, named after its kind ("Chapter Title"). In
    //: Polish: Tytuł {kind:genitive}
    names.at(static_cast<size_t>(editor::FieldType::TextTitle)) =
        core::fillWords(tr("{Kind} Title"), kind);
    //: A field: the words of the text it is in ("Chapter Word Count"). In Polish: Liczba słów
    //: {kind:genitive}
    names.at(static_cast<size_t>(editor::FieldType::Words)) =
        core::fillWords(tr("{Kind} Word Count"), kind);
    return names;
}

QString FieldCoordinator::whyNoField(const editor::BookEditor& editor)
{
    if (editor.isReadOnly()) {
        return readOnlyReason();
    }
    //: Why Insert > Field... is greyed out in a .txt file. In Polish: Plik zwykłego tekstu nie
    //: przechowuje pól
    return editor.annotationsAllowed() ? QString() : tr("A plain text file keeps no fields");
}

void FieldCoordinator::insertDateTime()
{
    editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (editor == nullptr || editor->isReadOnly()) {
        return;
    }
    auto& settings = core::SettingsManager::getInstance();
    const editor::FieldContext context = contextOf(editor);
    dialogs::DateTimeDialog::Data data;
    data.now = QDateTime::currentDateTime();
    data.language = context.language;
    data.format = QString::fromStdString(settings.get<std::string>(DATE_FORMAT_KEY, "long"));
    data.asField = settings.get<bool>(DATE_AS_FIELD_KEY, false);
    data.whyNoField = editor->annotationsAllowed() ? QString() : whyNoField(*editor);
    data.dateKeys = keysOf(m_commands, DATE_COMMAND);
    data.timeKeys = keysOf(m_commands, TIME_COMMAND);
    dialogs::DateTimeDialog dialog(data, m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString format = dialog.format();
    settings.set<std::string>(DATE_FORMAT_KEY, format.toStdString());
    if (data.whyNoField.isEmpty()) {
        settings.set<bool>(DATE_AS_FIELD_KEY, dialog.asField());
    }
    if (dialog.asField()) {
        editor->setFieldContext(context);  // the time of now
        editor->insertField({.type = editor::FieldType::Date, .format = format});
    } else {
        editor->insertText(
            editor::dateText(QDateTime::currentDateTime(), format, context.language));
    }
    editor->setFocus();
}

void FieldCoordinator::insertDate()
{
    editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (editor == nullptr || editor->isReadOnly()) {
        return;
    }
    editor->insertText(editor::dateText(QDateTime::currentDateTime(), dateFormatOfKeys(),
                                        contextOf(editor).language));
}

void FieldCoordinator::insertTime()
{
    editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (editor == nullptr || editor->isReadOnly()) {
        return;
    }
    editor->insertText(editor::dateText(QDateTime::currentDateTime(), QStringLiteral("time"),
                                        contextOf(editor).language));
}

void FieldCoordinator::insertField()
{
    if (editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr) {
        insertFieldIn(*editor, editor::TextField{});
    }
}

bool FieldCoordinator::insertOwnField(editor::BookEditor* editor, const QString& id)
{
    if (editor == nullptr) {
        return false;
    }
    editor::TextField start;
    start.type = editor::FieldType::Custom;
    start.key = id;
    return insertFieldIn(*editor, start);
}

bool FieldCoordinator::insertFieldIn(editor::BookEditor& editor, const editor::TextField& start)
{
    if (!whyNoField(editor).isEmpty()) {
        return false;
    }
    editor.setFieldContext(contextOf(&editor));  // what the window shows is what is now
    const std::optional<editor::TextField> field = askField(editor, start, false);
    if (field) {
        editor.insertField(*field);
    }
    editor.setFocus();
    return field.has_value();
}

bool FieldCoordinator::editField(editor::BookEditor& editor, int position)
{
    const std::optional<editor::TextField> current = editor.textFieldAt(position);
    if (!current || editor.isReadOnly()) {
        return false;
    }
    editor.setFieldContext(contextOf(&editor));
    const std::optional<editor::TextField> field = askField(editor, *current, true);
    editor.setFocus();
    return field && editor.setTextFieldAt(position, *field);
}

std::optional<editor::TextField> FieldCoordinator::askField(editor::BookEditor& editor,
                                                            const editor::TextField& field,
                                                            bool editing)
{
    QString elementId;
    for (const OpenText& text : openTexts()) {
        if (text.editor == &editor) {
            elementId = text.elementId;
        }
    }
    dialogs::FieldDialog::Data data;
    data.names = fieldNames(elementId);
    // The own fields of the volume of a text of the book
    if (m_book.isProjectOpen() && !elementId.isEmpty() && m_book.findElement(elementId) != nullptr) {
        const QString bookId = bookIdOf(elementId);
        data.ownFields = [this, bookId]() { return own_fields::of(m_book, bookId); };
        data.newOwnField = [this, bookId]() { return newOwnField(bookId, nullptr); };
        data.changeOwnField = [this, bookId](const QString& id) {
            return changeOwnField(id, bookId, nullptr);
        };
        data.deleteOwnField = [this, bookId](const QString& id) {
            return deleteOwnField(id, bookId, nullptr);
        };
        data.whyOwnFieldsFixed = m_book.isReadOnly() ? readOnlyReason() : QString();
    }
    data.field = field;
    data.editing = editing;
    data.language = editor.fieldContext().language;
    data.now = editor.fieldContext().now;
    // As the field shows where it goes: the book's values, the words, the page of the cursor
    data.shown = [&editor](const editor::TextField& shown) {
        editor::FieldCounts counts;
        counts.words = editor.textCounts().words;
        counts.page = editor.currentPage();
        counts.pages = editor.totalPages();
        return editor::textFieldText(shown, editor.fieldContext(), counts);
    };
    const editor::EditorAppearance& appearance = editor.appearance();
    const bool light = appearance.colorMode == editor::EditorColorMode::Light;
    if (appearance.fieldShading) {
        data.background = light ? appearance.colors.fields.light : appearance.colors.fields.dark;
    }
    data.text = appearance.colors.textColor(appearance.colorMode);
    dialogs::FieldDialog dialog(data, m_window);
    m_fieldDialog = &dialog;
    const int result = dialog.exec();
    m_fieldDialog = nullptr;
    if (result != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.field();
}

// =============================================================================
// The own fields
// =============================================================================

FieldCoordinator::~FieldCoordinator()
{
    forgetDelete();
}

QWidget* FieldCoordinator::dialogParent(QWidget* over) const
{
    if (over != nullptr) {
        return over;
    }
    return m_fieldDialog != nullptr ? m_fieldDialog.data() : m_window;
}

QString FieldCoordinator::bookIdOf(const QString& elementId) const
{
    const core::ProjectBook* volume = volumeOfText(m_book, elementId);
    return volume != nullptr ? volume->id : QString();
}

QString FieldCoordinator::newOwnField(const QString& bookId, QWidget* over)
{
    if (!m_book.isProjectOpen() || m_book.isReadOnly()) {
        return {};
    }
    QWidget* parent = dialogParent(over);
    dialogs::OwnFieldDialog dialog(
        OwnField{}, m_book.isSeries(),
        [this, bookId](const QString& name) {
            //: In Polish: Książka ma już pole o tej nazwie.
            return own_fields::isNameTaken(m_book, name, QString(), bookId)
                       ? tr("The book already has a field of this name.")
                       : QString();
        },
        parent);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    OwnField field = dialog.field();
    field.id = own_fields::newId();
    return own_fields::add(m_book, field, bookId) ? field.id : QString();
}

bool FieldCoordinator::changeOwnField(const QString& id, const QString& bookId, QWidget* over)
{
    const std::optional<OwnField> field = own_fields::find(m_book, id, bookId);
    if (!field || m_book.isReadOnly()) {
        return false;
    }
    QWidget* parent = dialogParent(over);
    dialogs::OwnFieldDialog dialog(
        *field, m_book.isSeries(),
        [this, id, bookId](const QString& name) {
            return own_fields::isNameTaken(m_book, name, id, bookId)
                       ? tr("The book already has a field of this name.")
                       : QString();
        },
        parent);
    if (dialog.exec() != QDialog::Accepted || dialog.field() == *field) {
        return false;
    }
    return own_fields::change(m_book, dialog.field(), bookId);
}

QStringList FieldCoordinator::textsOf(const QString& id) const
{
    QStringList elements;
    const core::BookProject* project = m_book.project();
    if (project == nullptr) {
        return elements;
    }
    // A field of a book is in its texts; one of the series, in the texts of every volume
    bool series = false;
    for (const core::BookCustomField& field : m_book.seriesCustomFields()) {
        series = series || field.id == id;
    }
    for (const core::ProjectBook& volume : project->books) {
        const bool own = std::any_of(volume.customFields.cbegin(), volume.customFields.cend(),
                                     [&id](const core::BookCustomField& field) {
                                         return field.id == id;
                                     });
        if (!series && !own) {
            continue;
        }
        for (const BookTextElement& text : bookTextElements(m_book, &volume)) {
            elements.append(text.element->id);
        }
    }
    return elements;
}

FieldCoordinator::FieldUses FieldCoordinator::usesOf(const QString& id) const
{
    FieldUses uses;
    QHash<QString, editor::BookEditor*> open;
    for (const OpenText& text : openTexts()) {
        if (!text.elementId.isEmpty()) {
            open.insert(text.elementId, text.editor);
        }
    }
    for (const QString& elementId : textsOf(id)) {
        const editor::BookEditor* editor = open.value(elementId, nullptr);
        const int count = editor != nullptr && editor->textDocument() != nullptr
                              ? editor::customFieldCount(*editor->textDocument(), id)
                              : editor::customFieldCountInKml(m_book.chapterText(elementId), id);
        if (count > 0) {
            uses.count += count;
            ++uses.texts;
        }
    }
    return uses;
}

bool FieldCoordinator::deleteOwnField(const QString& id, const QString& bookId, QWidget* over)
{
    const std::optional<OwnField> field = own_fields::find(m_book, id, bookId);
    if (!field || m_book.isReadOnly()) {
        return false;
    }

    // Used in the texts: their places become text or empty fields, as the writer says
    const FieldUses uses = usesOf(id);
    bool asText = false;
    if (uses.count > 0) {
        std::optional<bool> answer;
        if (m_askDelete) {
            answer = m_askDelete(*field, uses.count, uses.texts);
        } else {
            QWidget* parent = dialogParent(over);
            dialogs::DeleteOwnFieldDialog dialog(*field, uses.count, uses.texts,
                                                 keysOf(m_commands, "edit.undo"), parent);
            if (dialog.exec() == QDialog::Accepted) {
                answer = dialog.asText();
            }
        }
        if (!answer) {
            return false;
        }
        asText = *answer;
    }

    forgetDelete();
    DeletedField step;
    step.field = *field;
    step.bookId = bookId;
    const editor::CustomFieldValue value{field->name, field->value, field->description};
    QHash<QString, editor::BookEditor*> open;
    for (const OpenText& text : openTexts()) {
        if (!text.elementId.isEmpty()) {
            open.insert(text.elementId, text.editor);
        }
    }
    const std::vector<QString> dirty = m_book.getDirtyElements();
    for (const QString& elementId : textsOf(id)) {
        if (editor::BookEditor* editor = open.value(elementId, nullptr)) {
            if (editor->changeCustomFields(id, value, asText) > 0) {
                step.editors.append(editor);
            }
            continue;
        }
        const QString kml = m_book.chapterText(elementId);
        int count = 0;
        const QString changed = editor::customFieldsReplacedInKml(
            kml, id, value, contextFor(elementId).language, asText, &count);
        if (count == 0) {
            continue;
        }
        const bool clean = std::find(dirty.cbegin(), dirty.cend(), elementId) == dirty.cend();
        m_book.setChapterContent(elementId, changed);
        const bool saved = clean && m_book.saveChapterContent(elementId);
        step.savedTexts.append({elementId, kml, saved});
    }
    if (!own_fields::remove(m_book, id, bookId)) {
        // The book could not be saved: the texts as they were
        m_deleted = step;
        undoDelete();
        return false;
    }

    // The writer's Undo in an open text brings it back, until a text it changed changes again
    m_deleted = step;
    for (const OpenText& text : openTexts()) {
        editor::BookEditor* editor = text.editor;
        if (editor->isReadOnly()) {
            continue;
        }
        m_sharing.append({editor, editor->sharedHistory()});
        editor->setSharedHistory(this);
    }
    for (const QPointer<editor::BookEditor>& editor : std::as_const(step.editors)) {
        m_deleteWatch.append(connect(editor.data(), &editor::BookEditor::contentChanged, this,
                                     [this]() {
                                         if (!m_undoing) {
                                             forgetDelete();
                                         }
                                     }));
        m_deleteWatch.append(connect(editor.data(), &QObject::destroyed, this,
                                     &FieldCoordinator::forgetDelete));
    }

    //: In Polish: Usunięto pole „%1”.
    const QString message = tr("The field \u201c%1\u201d was deleted.").arg(field->name);
    EditorPanel* front =
        m_centralTabs != nullptr
            ? qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage())
            : nullptr;
    if (m_showNotice && front != nullptr) {
        //: In Polish: &Cofnij / Przywróć pole we wszystkich tekstach
        m_showNotice(front, message, tr("&Undo"), tr("Bring the field back in every text"),
                     [this]() { undoDelete(); });
    }
    return true;
}

bool FieldCoordinator::canUndoDelete() const
{
    if (!m_deleted || m_book.isReadOnly()) {
        return false;
    }
    // Every open text it changed is still open, and no text it changed in its file is open now
    QSet<QString> open;
    QList<const editor::BookEditor*> editors;
    for (const OpenText& text : openTexts()) {
        open.insert(text.elementId);
        editors.append(text.editor);
    }
    const auto& changed = m_deleted->editors;
    const auto& saved = m_deleted->savedTexts;
    return std::all_of(changed.cbegin(), changed.cend(),
                       [&editors](const QPointer<editor::BookEditor>& editor) {
                           return editor != nullptr && editors.contains(editor.data());
                       }) &&
           std::none_of(saved.cbegin(), saved.cend(),
                        [&open](const SavedText& text) { return open.contains(text.elementId); });
}

bool FieldCoordinator::undoDelete()
{
    if (!m_deleted || !canUndoDelete()) {
        forgetDelete();
        return false;
    }
    const DeletedField step = *m_deleted;
    forgetDelete();
    m_undoing = true;
    if (!own_fields::find(m_book, step.field.id, step.bookId)) {
        own_fields::add(m_book, step.field, step.bookId);
    }
    for (const QPointer<editor::BookEditor>& editor : step.editors) {
        editor->undo();
    }
    for (const SavedText& text : step.savedTexts) {
        m_book.setChapterContent(text.elementId, text.kml);
        if (text.saved) {
            m_book.saveChapterContent(text.elementId);
        }
    }
    m_undoing = false;
    refresh();
    return true;
}

bool FieldCoordinator::undoesFor(const editor::BookEditor& text) const
{
    return !m_undoing && m_deleted.has_value() && canUndoDelete() &&
           std::any_of(m_sharing.cbegin(), m_sharing.cend(),
                       [&text](const auto& shared) { return shared.first == &text; });
}

bool FieldCoordinator::redoesFor(const editor::BookEditor& text) const
{
    Q_UNUSED(text)
    return false;
}

void FieldCoordinator::undoFor(editor::BookEditor& text)
{
    Q_UNUSED(text)
    undoDelete();
}

void FieldCoordinator::redoFor(editor::BookEditor& text)
{
    Q_UNUSED(text)
}

void FieldCoordinator::forgetDelete()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_deleteWatch)) {
        disconnect(connection);
    }
    m_deleteWatch.clear();
    // The open texts' Undo is what it was before
    for (const auto& [editor, previous] : std::as_const(m_sharing)) {
        if (editor != nullptr && editor->sharedHistory() == this) {
            editor->setSharedHistory(previous);
        }
    }
    m_sharing.clear();
    m_deleted.reset();
}

}  // namespace kalahari::gui
