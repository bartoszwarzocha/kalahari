/// @file field_coordinator.cpp
/// @brief The fields of the open texts: what they show of the book, Insert > Date && Time...,
///        Insert > Field..., the date and the time at once and Edit Field...

#include "kalahari/gui/field_coordinator.h"

#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/field_dialogs.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QAction>
#include <QFileInfo>
#include <QTabWidget>

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
QString keysOf(const char* id)
{
    const Command* command = CommandRegistry::getInstance().getCommand(id);
    return command != nullptr && !command->shortcut.isEmpty()
               ? command->shortcut.toQKeySequence().toString(QKeySequence::NativeText)
               : QString();
}

}  // namespace

FieldCoordinator::FieldCoordinator(QTabWidget* centralTabs, TextWithKeys textWithKeys,
                                   QWidget* window, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_textWithKeys(std::move(textWithKeys)),
      m_window(window)
{
    // Another book: other values
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this, &FieldCoordinator::refresh);
    connect(&projects, &core::ProjectManager::projectClosed, this, &FieldCoordinator::refresh);
    connect(&projects, &core::ProjectManager::elementRenamed, this, &FieldCoordinator::refresh);

    // A text opened, closed or brought to the front: its fields show what is now
    if (m_centralTabs != nullptr) {
        DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        connect(&windows, &DocumentWindows::frontChanged, this, &FieldCoordinator::refresh);
        connect(&windows, &DocumentWindows::windowsChanged, this, &FieldCoordinator::refresh);
        connect(m_centralTabs, &QTabWidget::currentChanged, this, &FieldCoordinator::refresh);
    }
}

void FieldCoordinator::connectCommands()
{
    auto& registry = CommandRegistry::getInstance();
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
    const auto& projects = core::ProjectManager::getInstance();
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : projects.findElement(elementId);
    // A text of the book: its values; a file outside it has none, and the program's language
    if (const core::ProjectBook* book = projects.book(); book != nullptr && element != nullptr) {
        context.bookTitle = book->title;
        context.author = book->author;
        context.language = book->language;
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
    const std::string saved =
        core::SettingsManager::getInstance().get<std::string>(DATE_FORMAT_KEY, "long");
    const QString chosen =
        editor::normalizedFieldFormat(editor::FieldType::Date, QString::fromStdString(saved));
    return isDateOnly(chosen) ? chosen : QStringLiteral("long");
}

std::array<QString, editor::FIELD_TYPES.size()> FieldCoordinator::fieldNames(
    const QString& elementId)
{
    std::array<QString, editor::FIELD_TYPES.size()> names;
    for (size_t i = 0; i < editor::FIELD_TYPES.size(); ++i) {
        names.at(i) = editor::fieldTypeTitle(editor::FIELD_TYPES.at(i));
    }
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : core::ProjectManager::getInstance().findElement(elementId);
    if (element == nullptr) {
        return names;
    }
    const core::KindWords words = wordsOf(*element);
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
    data.dateKeys = keysOf(DATE_COMMAND);
    data.timeKeys = keysOf(TIME_COMMAND);
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
        editor->insertField({editor::FieldType::Date, format});
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
    editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (editor == nullptr || !whyNoField(*editor).isEmpty()) {
        return;
    }
    editor->setFieldContext(contextOf(editor));  // what the window shows is what is now
    if (const std::optional<editor::TextField> field =
            askField(*editor, editor::TextField{}, false)) {
        editor->insertField(*field);
    }
    editor->setFocus();
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
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.field();
}

}  // namespace kalahari::gui
