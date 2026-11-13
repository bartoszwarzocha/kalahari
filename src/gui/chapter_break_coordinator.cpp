/// @file chapter_break_coordinator.cpp
/// @brief Book > Chapter Break: the text from the cursor to a new text of the book right after
///        the text it was in, with Undo

#include "kalahari/gui/chapter_break_coordinator.h"

#include "kalahari/core/kind_words.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QFile>
#include <QHash>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

namespace kalahari::gui {

namespace {

constexpr int MESSAGE_MS = 4000;

/// @brief How many empty paragraphs before a heading still make it the start of the text
constexpr int EMPTY_PARAGRAPHS_BEFORE_TITLE = 20;

/// @brief The position of @p at in @p document
int positionOf(const QTextDocument& document, const editor::CursorPosition& at) {
    const QTextBlock block = document.findBlockByNumber(std::max(0, at.paragraph));
    if (!block.isValid()) {
        return document.characterCount() - 1;
    }
    return block.position() + std::clamp(at.offset, 0, block.length() - 1);
}

/// @brief Where @p text is split: at the start of its selection, else at its cursor
editor::CursorPosition breakPlaceOf(const editor::BookEditor& text) {
    return text.hasSelection() ? text.selection().normalized().start : text.cursorPosition();
}

/// @brief Whether the characters of @p document from @p from in @p step (1: on, -1: back) to
///        its start or end hold more than spaces and the ends of paragraphs and lines
bool hasTextFrom(const QTextDocument& document, int from, int step) {
    const int end = document.characterCount() - 1;
    for (int at = step > 0 ? from : from - 1; at >= 0 && at < end; at += step) {
        const QChar character = document.characterAt(at);
        if (!character.isSpace() && character != QChar::ParagraphSeparator &&
            character != QChar::LineSeparator) {
            return true;
        }
    }
    return false;
}

/// @brief The text of a paragraph from @p from on one line: its objects (images, the numbers of
///        notes) left out, its line breaks and tabs spaces
QString lineOf(const QTextBlock& block, int from) {
    QString text = block.text().mid(from);
    text.remove(QChar::ObjectReplacementCharacter);
    text.replace(QChar::LineSeparator, QLatin1Char(' '));
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    return text.simplified();
}

}  // namespace

QString chapterBreakTitle(const QTextDocument& document, int from) {
    QTextBlock block = document.findBlock(from);
    int offset = block.isValid() ? from - block.position() : 0;
    for (int skipped = 0; block.isValid() && skipped <= EMPTY_PARAGRAPHS_BEFORE_TITLE;
         block = block.next(), ++skipped, offset = 0) {
        if (QTextCursor(block).currentTable() != nullptr) {
            return QString();  // a table first
        }
        const QString line = lineOf(block, offset);
        if (line.isEmpty()) {
            if (block.text().mid(offset).contains(QChar::ObjectReplacementCharacter)) {
                return QString();  // an image first
            }
            continue;  // an empty paragraph before the start
        }
        const bool heading =
            editor::headingLevelOf(editor::paragraphStyleOf(block.blockFormat())) > 0;
        return heading ? line : QString();
    }
    return QString();
}

ChapterBreakCoordinator::ChapterBreakCoordinator(QTabWidget* centralTabs,
                                                 NavigatorCoordinator* navigator,
                                                 TextWithKeys textWithKeys,
                                                 NoticeShower showNotice, QStatusBar* statusBar,
                                                 QWidget* window, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_navigator(navigator),
      m_textWithKeys(std::move(textWithKeys)),
      m_showNotice(std::move(showNotice)),
      m_statusBar(statusBar),
      m_window(window) {
    // Another book, or none: the last break was in the old one
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this,
            &ChapterBreakCoordinator::forgetUndo);
    connect(&projects, &core::ProjectManager::projectClosed, this,
            &ChapterBreakCoordinator::forgetUndo);
}

void ChapterBreakCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND)) {
        command->execute = [this]() { breakText(m_textWithKeys ? m_textWithKeys() : nullptr); };
        command->disabledReason = [this]() {
            return whyNotBreak(m_textWithKeys ? m_textWithKeys() : nullptr);
        };
    }
}

// =============================================================================
// The break
// =============================================================================

QString ChapterBreakCoordinator::whyNotBreak(const editor::BookEditor* text) const {
    const auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen()) {
        //: Why Book > Chapter Break is greyed out. In Polish: Żadna książka nie jest otwarta
        return tr("No book is open");
    }
    if (projects.isReadOnly() || (text != nullptr && text->isReadOnly())) {
        return readOnlyReason();
    }
    const EditorPanel* panel = panelOf(text);
    const QString elementId =
        panel != nullptr ? panel->property("elementId").toString() : QString();
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : projects.findElement(elementId);
    const core::BookProject* project = projects.project();
    const std::optional<core::ElementPlace> at =
        element != nullptr && project != nullptr ? project->placeOf(elementId) : std::nullopt;
    if (text == nullptr || text->textDocument() == nullptr || !at ||
        at->place == core::BookPlace::Workshop) {
        //: In Polish: Kursor nie jest w tekście książki
        return tr("The cursor is not in a text of the book");
    }
    const core::KindRef kind = projects.kindOf(*element);
    if (!kind || !projects.newPlaceAfter(elementId, kind)) {
        const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};
        //: Why Book > Chapter Break is greyed out: the book has as many texts of the kind as it
        //: can have (a prologue). In Polish: Za tym tekstem nie można dodać {kind:genitive}
        return core::fillWords(tr("No {kind} can be added after this text"), nouns);
    }
    if (text->isInTable()) {
        //: In Polish: Kursor jest w tabeli, a tabeli nie da się podzielić
        return tr("The cursor is in a table, which cannot be split");
    }
    const QTextDocument& document = *text->textDocument();
    const int from = positionOf(document, breakPlaceOf(*text));
    if (!hasTextFrom(document, from, -1)) {
        //: In Polish: Przed kursorem nie ma tekstu
        return tr("There is no text before the cursor");
    }
    if (!hasTextFrom(document, from, 1)) {
        //: In Polish: Za kursorem nie ma tekstu
        return tr("There is no text after the cursor");
    }
    return QString();
}

QString ChapterBreakCoordinator::breakText(editor::BookEditor* text) {
    auto& logger = core::Logger::getInstance();
    auto& projects = core::ProjectManager::getInstance();
    // whyNotBreak() also says why without a text or its document
    if (const QString why = whyNotBreak(text);
        !why.isEmpty() || text == nullptr || text->textDocument() == nullptr) {
        say(why);
        return QString();
    }
    EditorPanel* panel = panelOf(text);
    const QString elementId = panel->property("elementId").toString();
    // A copy: the project changes
    const core::ProjectElement element = *projects.findElement(elementId);
    const core::KindRef kind = projects.kindOf(element);
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(element)}};
    //: The title of the warnings of Book > Chapter Break. In Polish: Nie udało się podzielić
    //: {kind:genitive}
    const QString notSplit = core::fillWords(tr("{Kind} Not Split"), nouns);

    // The changes of the text saved first
    if (m_navigator != nullptr && !m_navigator->saveTextsOf(element).isEmpty()) {
        dialogs::MessageDialog::warning(
            m_window, notSplit,
            //: In Polish: Nie udało się zapisać w pliku zmian w tym tekście, dlatego tekstu
            //: nie podzielono. Szczegóły są w dzienniku programu.
            tr("The changes of this text could not be saved to its file, so it was not split. "
               "The program's log has the details."));
        return QString();
    }

    // The new text, right after it, with the text from the cursor
    const editor::CursorPosition place = breakPlaceOf(*text);
    const int from = positionOf(*text->textDocument(), place);
    const QString moved = text->kmlFrom(place);
    QString title = chapterBreakTitle(*text->textDocument(), from);
    if (title.isEmpty()) {
        title = projects.defaultTitle(kind);
    }
    const std::optional<core::ElementPlace> at = projects.newPlaceAfter(elementId, kind);
    QString addedId =
        at ? projects.addElement(kind, title, at->place, at->groupId, at->index, {}, moved,
                                 element.status)
           : QString();
    if (addedId.isEmpty()) {
        logger.error("ChapterBreakCoordinator: the new text after '{}' could not be added",
                     element.title.toStdString());
        dialogs::MessageDialog::warning(
            m_window, notSplit,
            //: In Polish: Nie udało się dodać nowego tekstu do książki, dlatego tekstu nie
            //: podzielono. Szczegóły są w dzienniku programu.
            tr("The new text could not be added to the book, so the text was not split. The "
               "program's log has the details."));
        return QString();
    }

    // The text without it, saved at once (as the book is)
    forgetUndo();
    text->removeTextFrom(place);
    if (m_navigator != nullptr && !m_navigator->saveTextsOf(element).isEmpty()) {
        logger.warn("ChapterBreakCoordinator: '{}' keeps its change unsaved",
                    element.title.toStdString());
    }
    logger.info("ChapterBreakCoordinator: '{}' split, its text from the cursor in '{}' ({})",
                element.title.toStdString(), title.toStdString(), addedId.toStdString());

    // The new text opens, with the cursor at its start: where the cursor was
    if (m_navigator != nullptr) {
        m_navigator->showAdded(addedId, title);
    }
    editor::BookEditor* added = editorOf(addedId);

    // It can be undone while neither text changes
    if (added != nullptr) {
        m_undo = Step{elementId, element.title, addedId, text, added};
        for (editor::BookEditor* each : {text, added}) {
            m_undoWatch.append(connect(each, &editor::BookEditor::contentChanged, this,
                                       &ChapterBreakCoordinator::forgetUndo));
            m_undoWatch.append(
                connect(each, &QObject::destroyed, this, &ChapterBreakCoordinator::forgetUndo));
        }
    }
    //: In Polish: Tekst od kursora przeniesiono do „%1”, tuż za „%2”.
    const QString message =
        tr("The text from the cursor went to \"%1\", right after \"%2\".").arg(title,
                                                                              element.title);
    if (EditorPanel* addedPanel = panelOf(added); m_showNotice && addedPanel != nullptr) {
        //: In Polish: &Cofnij / Połącz teksty z powrotem
        m_showNotice(addedPanel, message, tr("&Undo"), tr("Join the texts again"),
                     [this]() { undo(); });
    } else {
        say(message);
    }
    return addedId;
}

// =============================================================================
// Undo
// =============================================================================

bool ChapterBreakCoordinator::canUndo() const {
    const auto& projects = core::ProjectManager::getInstance();
    return m_undo && projects.isProjectOpen() && !projects.isReadOnly() &&
           projects.findElement(m_undo->addedId) != nullptr &&
           projects.findElement(m_undo->elementId) != nullptr &&
           editorOf(m_undo->elementId) == m_undo->text &&
           editorOf(m_undo->addedId) == m_undo->added;
}

bool ChapterBreakCoordinator::undo() {
    auto& projects = core::ProjectManager::getInstance();
    const std::optional<Step> step = canUndo() ? m_undo : std::nullopt;
    forgetUndo();
    if (!step) {
        //: In Polish: Tego nie da się już cofnąć: teksty zmieniły się od tamtej pory.
        say(tr("This can no longer be undone: the texts changed since."));
        return false;
    }

    // The new text goes from the book, with its file: it was made by the break
    const core::ProjectElement* added = projects.findElement(step->addedId);
    const QStringList files = projects.filesOf(*added);
    const std::optional<core::ProjectElement> removed = projects.removeElement(step->addedId);
    if (!removed) {
        //: In Polish: Nie udało się zapisać zmian.
        say(tr("Failed to save changes."));
        return false;
    }
    if (m_navigator != nullptr) {
        m_navigator->closeTabsOf(*removed);
    }
    for (const QString& file : files) {
        if (!QFile::remove(file)) {
            core::Logger::getInstance().warn("ChapterBreakCoordinator: '{}' stays on the disk",
                                             file.toStdString());
        }
    }

    // The text whole again, saved
    step->text->undo();
    if (const core::ProjectElement* element = projects.findElement(step->elementId);
        element != nullptr && m_navigator != nullptr) {
        m_navigator->saveTextsOf(*element);
        m_navigator->refreshNavigator();
    }
    if (EditorPanel* panel = panelOf(step->text); panel != nullptr && m_centralTabs != nullptr) {
        DocumentWindows::of(*m_centralTabs).showPage(panel);
        step->text->setFocus(Qt::OtherFocusReason);
    }
    //: In Polish: „%1” znów jest w całości.
    say(tr("\"%1\" is whole again.").arg(step->title));
    return true;
}

void ChapterBreakCoordinator::forgetUndo() {
    for (const QMetaObject::Connection& connection : std::as_const(m_undoWatch)) {
        disconnect(connection);
    }
    m_undoWatch.clear();
    m_undo.reset();
}

// =============================================================================
// Helpers
// =============================================================================

EditorPanel* ChapterBreakCoordinator::panelOf(const editor::BookEditor* text) const {
    if (m_centralTabs == nullptr || text == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page);
            panel != nullptr && panel->getBookEditor() == text) {
            return panel;
        }
    }
    return nullptr;
}

editor::BookEditor* ChapterBreakCoordinator::editorOf(const QString& elementId) const {
    if (m_centralTabs == nullptr || elementId.isEmpty()) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page);
            panel != nullptr && page->property("elementId").toString() == elementId) {
            return panel->getBookEditor();
        }
    }
    return nullptr;
}

void ChapterBreakCoordinator::say(const QString& message) const {
    if (m_statusBar != nullptr && !message.isEmpty()) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

}  // namespace kalahari::gui
