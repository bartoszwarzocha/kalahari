/// @file versions_coordinator.cpp
/// @brief Save Version and Version History, and what the window of the versions chose

#include "kalahari/gui/versions_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/version_history_dialog.h"
#include "kalahari/gui/dialogs/version_name_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/version_texts.h"

#include <QMimeData>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>

#include <memory>
#include <utility>

namespace kalahari::gui {

namespace {

/// How long the status bar says what was done with a version
constexpr int MESSAGE_MS = 8000;

/// The commands of the versions, in the File menu
constexpr const char* SAVE_COMMAND = "file.saveVersion";
constexpr const char* HISTORY_COMMAND = "file.versionHistory";

/// Place @p position of @p document as a paragraph and a character in it
editor::CursorPosition placeOf(const QTextDocument& document, int position) {
    const QTextBlock block = document.findBlock(position);
    return {block.blockNumber(), position - block.position()};
}

/// The words of the kind of element @p elementId ("chapter")
core::KindWords kindWordsOf(const QString& elementId) {
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    return element != nullptr ? wordsOf(*element) : mainWords();
}

/// The title of element @p elementId
QString titleOf(const QString& elementId) {
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    return element != nullptr ? element->title : QString();
}

}  // namespace

VersionsCoordinator::VersionsCoordinator(QTabWidget* centralTabs, ChapterOpener openChapter,
                                         NoticeShower showNotice, QStatusBar* statusBar,
                                         QWidget* window, QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_openChapter(std::move(openChapter))
    , m_showNotice(std::move(showNotice))
    , m_statusBar(statusBar)
    , m_window(window)
{
    // The commands follow the text in front and the open book
    connect(m_centralTabs, &QTabWidget::currentChanged, this,
            &VersionsCoordinator::updateCommandStates);
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this,
            &VersionsCoordinator::updateCommandStates);
    connect(&projects, &core::ProjectManager::projectClosed, this,
            &VersionsCoordinator::updateCommandStates);
}

void VersionsCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    const auto connectCommand = [&registry](const char* id, std::function<void()> execute,
                                            std::function<QString()> whyNot) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = std::move(execute);
            command->isEnabled = [whyNot]() { return whyNot().isEmpty(); };
            command->disabledReason = std::move(whyNot);
            registry.updateActionState(id);
        }
    };
    connectCommand(
        SAVE_COMMAND, [this]() { saveVersion(); }, [this]() { return whyNotSave(); });
    connectCommand(
        HISTORY_COMMAND, [this]() { showHistory(); }, [this]() { return whyNotShowHistory(); });
}

QString VersionsCoordinator::whyNotSave() const {
    return whyNot(currentPanel(), true);
}

QString VersionsCoordinator::whyNotShowHistory() const {
    return whyNot(currentPanel(), false);
}

// =============================================================================
// Save Version
// =============================================================================

bool VersionsCoordinator::saveVersion(const QString& elementId, QWidget* parent) {
    EditorPanel* panel = panelFor(elementId);
    const QString reason = whyNot(panel, true);
    if (!reason.isEmpty()) {
        say(reason);
        return false;
    }
    return saveVersionOf(panel, parent != nullptr ? parent : m_window);
}

bool VersionsCoordinator::saveVersionOf(EditorPanel* panel, QWidget* parent) {
    const QString elementId = elementIdOf(panel);
    const core::KindWords kind = kindWordsOf(elementId);
    const QString title = titleOf(elementId);

    // The small window of its description, unless the writer wants none
    const bool ask = core::SettingsManager::getInstance().get<bool>(
        core::ChapterVersions::ASK_NAME_SETTING);
    QString name;
    if (ask) {
        dialogs::VersionNameDialog dialog(title, kind, parent);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        name = dialog.name();
    }

    core::ChapterVersion version;
    const core::ProjectManager::VersionKept kept = core::ProjectManager::getInstance().keepVersion(
        elementId, panel->getBookEditor()->toKml(),
        {.origin = core::VersionOrigin::Manual, .name = name}, &version);
    if (kept != core::ProjectManager::VersionKept::Added) {
        //: In Polish: Nie zapisano wersji
        dialogs::MessageDialog::error(parent, tr("Version Not Saved"),
                                      //: In Polish: Nie można zapisać wersji na dysku.
                                      tr("The version cannot be written on the disk."));
        return false;
    }

    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), kind}};
    //: %1: the title of the text, %2: when the version was kept (today, 15:10). In Polish:
    //: Zapisano wersję {kind:genitive} „%1” (%2).
    QString message = core::fillWords(tr("Saved a version of the {kind} \"%1\" (%2)."), nouns)
                          .arg(title, versionTime(version.created));
    if (!ask) {
        //: After "Saved a version...", when no window asked for its description. In Polish:
        //: Opis można dodać w historii wersji (F2).
        message += QLatin1Char(' ') + tr("The description can be added in the version history "
                                         "(F2).");
    }
    say(message);
    return true;
}

// =============================================================================
// Version History
// =============================================================================

bool VersionsCoordinator::showHistory(const QString& elementId) {
    EditorPanel* panel = panelFor(elementId);
    const QString reason = whyNot(panel, false);
    if (!reason.isEmpty()) {
        say(reason);
        return false;
    }

    auto& pm = core::ProjectManager::getInstance();
    const QString id = elementIdOf(panel);
    editor::BookEditor* editor = panel->getBookEditor();
    dialogs::VersionHistoryDialog::Text text;
    text.elementId = id;
    text.title = titleOf(id);
    text.bookTitle = pm.book() != nullptr ? pm.book()->title : QString();
    text.kind = kindWordsOf(id);
    text.kml = editor->toKml();
    text.editor = editor;
    text.readOnly = pm.isReadOnly();

    // Saved from the window, a version is the text in its editor, as Save Version keeps it
    dialogs::VersionHistoryDialog dialog(
        text,
        [this, id](QWidget* parent) {
            if (EditorPanel* open = openPanel(id)) {
                saveVersionOf(open, parent);
            }
        },
        m_window);
    dialog.exec();

    // The text may have been closed meanwhile: what was chosen goes to it only when it is open
    EditorPanel* open = openPanel(id);
    if (open == nullptr) {
        return true;
    }
    switch (dialog.choice()) {
    case dialogs::VersionHistoryDialog::Choice::Restore:
        restoreVersion(open, dialog.chosenVersion(), dialog.chosenKml());
        break;
    case dialogs::VersionHistoryDialog::Choice::Insert:
        insertFragment(open, dialog.chosenVersion(), dialog.chosenKml());
        break;
    case dialogs::VersionHistoryDialog::Choice::None:
        break;
    }
    return true;
}

bool VersionsCoordinator::restoreVersion(EditorPanel* editor, const core::ChapterVersion& version,
                                         const QString& kml) {
    editor::BookEditor* text = editor != nullptr ? editor->getBookEditor() : nullptr;
    auto& pm = core::ProjectManager::getInstance();
    if (text == nullptr || text->isReadOnly() || pm.isReadOnly()) {
        return false;
    }
    const QString elementId = elementIdOf(editor);

    // Nothing is lost: the text as it is now is kept first
    const core::ProjectManager::VersionKept kept =
        pm.keepVersion(elementId, text->toKml(),
                       {.origin = core::VersionOrigin::Restore, .restored = version.created});
    if (kept == core::ProjectManager::VersionKept::Failed) {
        //: In Polish: Nie przywrócono wersji
        dialogs::MessageDialog::error(m_window, tr("Version Not Restored"),
                                      //: In Polish: Nie można zapisać bieżącego tekstu jako
                                      //: wersji, więc wersja nie została przywrócona.
                                      tr("The current text cannot be kept as a version, so "
                                         "the version is not restored."));
        return false;
    }

    text->replaceWithKml(kml);
    text->ensureCursorVisible();
    text->setFocus();
    core::Logger::getInstance().info("VersionsCoordinator: Version {} of {} restored",
                                     version.file.toStdString(), elementId.toStdString());

    // The bar under the text says so, with Undo, until the text changes
    const QString when = versionTime(version.created);
    QString message;
    if (kept == core::ProjectManager::VersionKept::NoText) {
        //: %1: when the version was kept (today, 14:52); the text before it was empty. In
        //: Polish: Przywrócono wersję z %1. Ctrl+Z cofa przywrócenie.
        message = tr("Restored the version of %1. Ctrl+Z undoes the restoring.").arg(when);
    } else {
        //: %1: when the version was kept (today, 14:52). In Polish: Przywrócono wersję z %1.
        //: Tekst sprzed przywrócenia jest zapisany jako wersja, a Ctrl+Z cofa przywrócenie.
        message = tr("Restored the version of %1. The text from before the restoring is kept "
                     "as a version, and Ctrl+Z undoes the restoring.")
                      .arg(when);
    }
    if (m_showNotice) {
        m_showNotice(editor, message,
                     //: The button of the bar after a version was restored. In Polish: &Cofnij
                     tr("&Undo"),
                     //: In Polish: Cofnij przywrócenie, jak Ctrl+Z
                     tr("Undo the restoring, as Ctrl+Z does"), [this, elementId]() {
                         if (EditorPanel* open = openPanel(elementId)) {
                             open->getBookEditor()->undo();
                             open->getBookEditor()->setFocus();
                         }
                     });
    } else {
        say(message);
    }
    return true;
}

bool VersionsCoordinator::insertFragment(EditorPanel* editor, const core::ChapterVersion& version,
                                         const QString& fragment) {
    editor::BookEditor* text = editor != nullptr ? editor->getBookEditor() : nullptr;
    if (text == nullptr || text->isReadOnly() || fragment.isEmpty()) {
        return false;
    }
    const QTextCursor before = text->selectionCursor();
    if (before.isNull()) {
        return false;
    }
    const int start = before.selectionStart();
    const std::unique_ptr<QMimeData> data = editor::ClipboardHandler::mimeDataOf(fragment);
    text->insertFromMimeData(data.get());

    // The fragment inserted, selected: the cursor is at its end
    if (const QTextDocument* document = text->textDocument()) {
        text->setSelection({placeOf(*document, start), text->cursorPosition()});
    }
    text->ensureCursorVisible();
    text->setFocus();
    //: %1: when the version was kept (today, 14:52). In Polish: Wstawiono fragment wersji z %1.
    //: Ctrl+Z cofa wstawienie.
    say(tr("Inserted a fragment of the version of %1. Ctrl+Z undoes the inserting.")
            .arg(versionTime(version.created)));
    return true;
}

// =============================================================================
// The texts
// =============================================================================

EditorPanel* VersionsCoordinator::currentPanel() const {
    auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->currentWidget());
    return panel != nullptr && panel->getBookEditor() != nullptr ? panel : nullptr;
}

EditorPanel* VersionsCoordinator::openPanel(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return currentPanel();
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (panel != nullptr && panel->getBookEditor() != nullptr &&
            elementIdOf(panel) == elementId) {
            return panel;
        }
    }
    return nullptr;
}

EditorPanel* VersionsCoordinator::panelFor(const QString& elementId) {
    if (EditorPanel* panel = openPanel(elementId)) {
        m_centralTabs->setCurrentWidget(panel);
        return panel;
    }
    if (elementId.isEmpty() || !m_openChapter) {
        return nullptr;
    }
    m_openChapter(elementId);
    return openPanel(elementId);
}

QString VersionsCoordinator::elementIdOf(const EditorPanel* panel) {
    return panel != nullptr ? panel->property("elementId").toString() : QString();
}

QString VersionsCoordinator::whyNot(const EditorPanel* panel, bool saving) const {
    auto& pm = core::ProjectManager::getInstance();
    if (panel == nullptr) {
        //: Why Save Version and Version History are greyed out. In Polish: Na wierzchu nie ma
        //: tekstu książki
        return tr("No text of the book is in front");
    }
    const QString elementId = elementIdOf(panel);
    if (elementId.isEmpty() || !pm.isProjectOpen()) {
        if (!panel->property("isStandaloneFile").toBool()) {
            return tr("No text of the book is in front");
        }
        return pm.isProjectOpen()
                   //: In Polish: Wersje mają tylko teksty książki, a ten plik nie należy do
                   //: otwartej książki
                   ? tr("Only the texts of a book have versions, and this file is not part of "
                        "the open book")
                   //: In Polish: Wersje mają tylko teksty książki, a ten plik nie należy do
                   //: żadnej książki
                   : tr("Only the texts of a book have versions, and this file is not part of "
                        "a book");
    }
    if (!pm.versionsOf(elementId)) {
        //: In Polish: Ten tekst nie ma pliku w folderze książki, więc nie ma wersji
        return tr("This text has no file in the folder of the book, so it has no versions");
    }
    if (saving && pm.isReadOnly()) {
        return readOnlyReason();
    }
    return QString();
}

void VersionsCoordinator::say(const QString& message) const {
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

void VersionsCoordinator::updateCommandStates() const {
    auto& registry = CommandRegistry::getInstance();
    registry.updateActionState(SAVE_COMMAND);
    registry.updateActionState(HISTORY_COMMAND);
}

}  // namespace kalahari::gui
