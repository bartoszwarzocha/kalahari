/// @file style_coordinator.cpp
/// @brief New, changed and deleted paragraph styles of the book, and the undo of the last change

#include "kalahari/gui/style_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/manage_styles_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/style_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/style_draft.h"

#include <QStatusBar>
#include <QTabWidget>
#include <QTextDocument>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace kalahari::gui {

namespace {

/// How long the status bar says what was done
constexpr int MESSAGE_MS = 8000;

/// The commands of the styles of the book, in Format > Text Style
constexpr const char* NEW_COMMAND = "format.style.new";
constexpr const char* MODIFY_COMMAND = "format.style.modify";
constexpr const char* DELETE_COMMAND = "format.style.delete";
constexpr const char* MANAGE_COMMAND = "format.style.manage";

QString normalId() {
    return QString::fromLatin1(editor::NORMAL_STYLE_ID);
}

}  // namespace

StyleCoordinator::StyleCoordinator(QTabWidget* centralTabs, NoticeShower showNotice,
                                   QStatusBar* statusBar, QWidget* window, QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_showNotice(std::move(showNotice))
    , m_statusBar(statusBar)
    , m_window(window) {
    // The commands follow the text in front, the style at its cursor and the open book
    connect(m_centralTabs, &QTabWidget::currentChanged, this, [this]() {
        followEditor();
        updateCommandStates();
    });
    auto& projects = core::ProjectManager::getInstance();
    const auto bookChanged = [this]() {
        forgetUndo();
        followEditor();
        updateCommandStates();
    };
    connect(&projects, &core::ProjectManager::projectOpened, this, bookChanged);
    connect(&projects, &core::ProjectManager::projectClosed, this, bookChanged);
    // A change of the styles made elsewhere is not undone here
    connect(&projects, &core::ProjectManager::paragraphStylesChanged, this, [this]() {
        if (!m_changing) {
            forgetUndo();
        }
        updateCommandStates();
    });
    followEditor();
}

void StyleCoordinator::connectCommands() {
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
    // Modify and delete the style at the cursor: they need a text in front
    const auto whyNotAtCursor = [this]() {
        if (const QString reason = whyNotChange(); !reason.isEmpty()) {
            return reason;
        }
        //: Why Modify Style and Delete Style are greyed out. In Polish: Na wierzchu nie ma
        //: tekstu książki
        return editorInFront() == nullptr ? tr("No text of the book is in front") : QString();
    };
    connectCommand(
        NEW_COMMAND, [this]() { newStyle(); }, [this]() { return whyNotChange(); });
    connectCommand(
        MODIFY_COMMAND, [this]() { modifyStyle(); }, whyNotAtCursor);
    connectCommand(
        DELETE_COMMAND, [this]() { deleteStyle(); },
        [this, whyNotAtCursor]() {
            const QString reason = whyNotAtCursor();
            return reason.isEmpty() ? whyNotDelete(styleAtCursor()) : reason;
        });
    connectCommand(
        MANAGE_COMMAND, [this]() { manageStyles(); }, [this]() { return whyNotChange(); });
}

// =============================================================================
// The styles of the book
// =============================================================================

editor::ParagraphStyleSet StyleCoordinator::styles() const {
    return bookParagraphStyles();
}

QList<core::BookParagraphStyle> StyleCoordinator::entries() const {
    return core::ProjectManager::getInstance().paragraphStyles();
}

QString StyleCoordinator::typeName() const {
    const auto& projects = core::ProjectManager::getInstance();
    const core::BookProject* book = projects.project();
    return book != nullptr ? typeNameOf(projects.bookTypes(), *book) : QString();
}

QHash<QString, StyleCoordinator::Usage> StyleCoordinator::usage() const {
    QHash<QString, Usage> result;
    const auto& projects = core::ProjectManager::getInstance();
    const QHash<QString, editor::BookEditor*> open = openEditors();
    for (const QString& id : projects.textElementIds()) {
        const editor::BookEditor* text = open.value(id, nullptr);
        const QHash<QString, int> counts =
            text != nullptr ? text->paragraphStyleCounts()
                            : editor::paragraphStyleCounts(projects.chapterText(id));
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (it.value() > 0) {
                Usage& used = result[it.key()];
                used.paragraphs += it.value();
                ++used.texts;
            }
        }
    }
    return result;
}

EditorPanel* StyleCoordinator::panelInFront() const {
    if (!core::ProjectManager::getInstance().isProjectOpen()) {
        return nullptr;
    }
    auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->currentWidget());
    return panel != nullptr && panel->getBookEditor() != nullptr &&
                   !panel->property("elementId").toString().isEmpty()
               ? panel
               : nullptr;
}

editor::BookEditor* StyleCoordinator::editorInFront() const {
    EditorPanel* panel = panelInFront();
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

QString StyleCoordinator::styleAtCursor() const {
    const editor::BookEditor* text = editorInFront();
    if (text == nullptr) {
        return normalId();
    }
    // Paragraphs of different styles selected: the style of the one with the cursor
    const QString selected = text->selectedParagraphStyle();
    return selected.isEmpty() ? text->currentParagraphStyle() : selected;
}

QHash<QString, editor::BookEditor*> StyleCoordinator::openEditors() const {
    QHash<QString, editor::BookEditor*> editors;
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        const QString id = panel != nullptr ? panel->property("elementId").toString() : QString();
        if (!id.isEmpty() && panel->getBookEditor() != nullptr) {
            editors.insert(id, panel->getBookEditor());
        }
    }
    return editors;
}

QString StyleCoordinator::nameOf(const QString& id) const {
    const editor::ParagraphStyleSet set = styles();
    const editor::ParagraphStyle* style = set.find(id);
    return style != nullptr && !style->name.isEmpty() ? style->name : id;
}

QString StyleCoordinator::styleFor(const QString& id) const {
    return id.isEmpty() ? styleAtCursor() : id;
}

// =============================================================================
// Why a command cannot be used
// =============================================================================

QString StyleCoordinator::whyNotChange() const {
    const auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen()) {
        //: Why the commands of the styles of the book are greyed out. In Polish: Nie jest
        //: otwarta żadna książka
        return tr("No book is open");
    }
    return projects.isReadOnly() ? readOnlyReason() : QString();
}

QString StyleCoordinator::whyNotDelete(const QString& id) const {
    const editor::ParagraphStyleSet set = styles();
    const editor::ParagraphStyle* style = set.find(id);
    if (style == nullptr) {
        //: In Polish: Książka nie ma stylu „%1”
        return tr("The book has no style \"%1\"").arg(id);
    }
    if (style->origin != editor::StyleOrigin::Own) {
        //: In Polish: „%1” to styl typu „%2”: można go zmienić albo przywrócić, ale nie usunąć
        return tr("\"%1\" is a style of the type \"%2\": it can be changed or reset, but not "
                  "deleted")
            .arg(style->name, typeName());
    }
    return QString();
}

QString StyleCoordinator::whyNotReset(const QString& id) const {
    const editor::ParagraphStyleSet set = styles();
    const editor::ParagraphStyle* style = set.find(id);
    if (style == nullptr) {
        return tr("The book has no style \"%1\"").arg(id);
    }
    if (style->origin == editor::StyleOrigin::Own) {
        //: In Polish: „%1” to własny styl tej książki: typ „%2” nie ma takiego stylu
        return tr("\"%1\" is a style of this book's own: the type \"%2\" has no such style")
            .arg(style->name, typeName());
    }
    if (style->origin == editor::StyleOrigin::Type) {
        //: In Polish: „%1” jest taki jak w typie „%2”
        return tr("\"%1\" is as the type \"%2\" has it").arg(style->name, typeName());
    }
    return QString();
}

// =============================================================================
// The commands
// =============================================================================

QString StyleCoordinator::newStyle(const QString& baseId, bool applyToText, QWidget* parent) {
    if (const QString reason = whyNotChange(); !reason.isEmpty()) {
        say(reason);
        return QString();
    }
    editor::BookEditor* text = editorInFront();
    const QString atCursor = styleAtCursor();
    const QString base = baseId.isEmpty() ? atCursor : baseId;
    // The look of the paragraph at the cursor, when the new style is based on its style
    const QVariantMap own =
        text != nullptr && base == atCursor ? text->ownStyleProperties() : QVariantMap();
    const bool offer = applyToText && text != nullptr && !text->isReadOnly() &&
                       (text->hasSelection() || text->currentObject() == editor::TextObject::None);

    dialogs::StyleDialog dialog(StyleDraft::newStyle(styles(), base, own), typeName(), offer,
                                parent != nullptr ? parent : m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }
    const QString id = saveDraft(dialog.draft());
    if (id.isEmpty()) {
        return QString();
    }
    // The formatting the style took from the paragraph is the style's now
    if (offer && dialog.applyToText() && text == editorInFront()) {
        text->applyNewParagraphStyle(id, own.keys());
    }
    return id;
}

bool StyleCoordinator::createMissingStyle(const QString& id, QWidget* parent) {
    if (const QString reason = whyNotChange(); !reason.isEmpty()) {
        say(reason);
        return false;
    }
    const editor::ParagraphStyleSet set = styles();
    if (id.trimmed().isEmpty() || set.contains(id)) {
        return false;
    }
    dialogs::StyleDialog dialog(StyleDraft::forMissing(set, id), typeName(), false,
                                parent != nullptr ? parent : m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    return saveDraft(dialog.draft()) == id;
}

bool StyleCoordinator::modifyStyle(const QString& requested, QWidget* parent) {
    if (const QString reason = whyNotChange(); !reason.isEmpty()) {
        say(reason);
        return false;
    }
    const QString id = styleFor(requested);
    const editor::ParagraphStyleSet set = styles();
    if (!set.contains(id)) {
        say(tr("The book has no style \"%1\"").arg(id));
        return false;
    }
    dialogs::StyleDialog dialog(StyleDraft::existing(set, bookTypeStyles(), id), typeName(),
                                false, parent != nullptr ? parent : m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    return !saveDraft(dialog.draft()).isEmpty();
}

bool StyleCoordinator::deleteStyle(const QString& requested, QWidget* parent, bool notice) {
    const QString id = styleFor(requested);
    QString reason = whyNotChange();
    if (reason.isEmpty()) {
        reason = whyNotDelete(id);
    }
    if (!reason.isEmpty()) {
        say(reason);
        return false;
    }
    const QString name = nameOf(id);
    const QString successor = nameOf(editor::successorOf(styles(), id));

    // A style no paragraph uses goes at once; else the window says what happens to them
    const Usage used = usage().value(id);
    if (used.paragraphs > 0) {
        //: Part of the question before a style is deleted: "in 3 texts". In Polish: w %n
        //: tekście / w %n tekstach / w %n tekstach
        const QString inTexts = tr("in %n text(s)", nullptr, used.texts);
        //: In Polish: Używa go %n akapit %1. / Używają go %n akapity %1. / Używa go %n
        //: akapitów %1.
        QString text = tr("%n paragraph(s) %1 use it.", nullptr, used.paragraphs).arg(inTexts);
        text += QLatin1Char(' ');
        //: In Polish: Przejdzie do stylu „%1”, a style oparte na nim zachowają wygląd. /
        //: Przejdą do stylu „%1”, a style oparte na nim zachowają wygląd.
        text += tr("It goes to the style \"%1\", and the styles based on it keep their look.",
                   nullptr, used.paragraphs)
                    .arg(successor);
        text += QLatin1Char(' ');
        text += notice
                    //: In Polish: Usunięcie cofa przycisk „Cofnij” pod tekstem.
                    ? tr("Undo under the text undoes it.")
                    //: In Polish: Usunięcie cofa Ctrl+Z w oknie „Zarządzaj stylami”.
                    : tr("Ctrl+Z in Manage Styles undoes it.");
        //: In Polish: Usunąć styl „%1”?
        if (!dialogs::MessageDialog::confirm(parent != nullptr ? parent : m_window,
                                             tr("Delete the Style \"%1\"?").arg(name), text,
                                             tr("&Delete Style"))) {
            return false;
        }
    }

    const int moved = removeStyle(id);
    if (moved < 0) {
        //: In Polish: Nie udało się usunąć stylu: nie udało się zapisać pliku książki.
        say(tr("The style could not be deleted: the file of the book could not be saved."));
        return false;
    }
    if (notice) {
        notify(moved > 0
                   //: In Polish: Styl „%1” usunięty: %n akapit ma teraz styl „%2”. / %n akapity
                   //: mają teraz styl „%2”. / %n akapitów ma teraz styl „%2”.
                   ? tr("The style \"%1\" is deleted: %n paragraph(s) now have the style \"%2\".",
                        nullptr, moved)
                         .arg(name, successor)
                   //: In Polish: Styl „%1” usunięty.
                   : tr("The style \"%1\" is deleted.").arg(name));
    }
    return true;
}

bool StyleCoordinator::resetStyle(const QString& id, bool notice) {
    QString reason = whyNotChange();
    if (reason.isEmpty()) {
        reason = whyNotReset(id);
    }
    if (!reason.isEmpty()) {
        say(reason);
        return false;
    }
    const QString name = nameOf(id);
    //: What Undo undoes. In Polish: przywrócenie stylu „%1”
    if (change(tr("resetting the style \"%1\"").arg(name),
               editor::withoutStyleEntry(entries(), id), std::nullopt) < 0) {
        //: In Polish: Nie udało się zapisać pliku książki.
        say(tr("The file of the book could not be saved."));
        return false;
    }
    if (notice) {
        //: In Polish: Styl „%1” znów jest taki jak w typie „%2”.
        notify(tr("The style \"%1\" is as the type \"%2\" has it again.").arg(name, typeName()));
    }
    return true;
}

void StyleCoordinator::manageStyles(const QString& selectedId) {
    if (const QString reason = whyNotChange(); !reason.isEmpty()) {
        say(reason);
        return;
    }
    dialogs::ManageStylesDialog dialog(*this, styleFor(selectedId), m_window);
    dialog.exec();
}

// =============================================================================
// The changes
// =============================================================================

QString StyleCoordinator::saveDraft(const StyleDraft& draft) {
    if (!whyNotChange().isEmpty() || !draft.problem().isEmpty()) {
        return QString();
    }
    const QList<core::BookParagraphStyle> now = entries();
    QString id = draft.id();
    if (draft.kind() == StyleDraft::Kind::New) {
        QStringList taken;
        for (const editor::ParagraphStyle& style : draft.styles().styles()) {
            taken.append(style.id);
        }
        for (const core::BookParagraphStyle& entry : now) {
            taken.append(entry.id);
        }
        // A style for paragraphs from another book keeps their id
        id = !draft.id().isEmpty() && !taken.contains(draft.id())
                 ? draft.id()
                 : editor::paragraphStyleIdFor(draft.name(), taken);
    }

    QList<core::BookParagraphStyle> next;
    if (std::optional<core::BookParagraphStyle> entry = draft.entry(id)) {
        // What the program does not know of the entry stays in it
        const auto old = std::find_if(now.cbegin(), now.cend(),
                                      [&id](const core::BookParagraphStyle& each) {
                                          return each.id == id;
                                      });
        if (old != now.cend()) {
            entry->extra = old->extra;
        }
        next = editor::withStyleEntry(now, *entry);
    } else {
        next = editor::withoutStyleEntry(now, id);
    }
    if (next == now) {
        return id;
    }
    const QString text = draft.kind() == StyleDraft::Kind::New
                             //: What Undo undoes. In Polish: dodanie stylu „%1”
                             ? tr("adding the style \"%1\"").arg(draft.name().trimmed())
                             //: What Undo undoes. In Polish: zmianę stylu „%1”
                             : tr("changing the style \"%1\"").arg(draft.name());
    if (change(text, next, std::nullopt) < 0) {
        say(tr("The file of the book could not be saved."));
        return QString();
    }
    return id;
}

int StyleCoordinator::removeStyle(const QString& id) {
    if (!whyNotChange().isEmpty() || !whyNotDelete(id).isEmpty()) {
        return -1;
    }
    const editor::ParagraphStyleSet set = styles();
    //: What Undo undoes. In Polish: usunięcie stylu „%1”
    return change(tr("deleting the style \"%1\"").arg(nameOf(id)),
                  editor::withoutOwnStyle(entries(), set, id),
                  std::pair<QString, QString>{id, editor::successorOf(set, id)});
}

int StyleCoordinator::change(const QString& text, const QList<core::BookParagraphStyle>& entries,
                             const std::optional<std::pair<QString, QString>>& replace) {
    auto& projects = core::ProjectManager::getInstance();
    Step step;
    step.text = text;
    step.entries = projects.paragraphStyles();

    // The styles first: the texts show their paragraphs in the new ones at once
    forgetUndo();
    m_changing = true;
    const bool kept = projects.setParagraphStyles(entries);
    m_changing = false;
    if (!kept) {
        return -1;
    }

    int moved = 0;
    if (replace) {
        const QHash<QString, editor::BookEditor*> open = openEditors();
        const std::vector<QString> dirty = projects.getDirtyElements();
        for (const QString& id : projects.textElementIds()) {
            if (editor::BookEditor* editor = open.value(id, nullptr)) {
                // An open text: one step of its undo history, saved with the rest of it
                const int count = editor->replaceParagraphStyle(replace->first, replace->second);
                if (count > 0) {
                    step.editors.append(editor);
                    moved += count;
                }
                continue;
            }
            // A text not open: its file, unless it has other changes not saved yet
            const QString kml = projects.chapterText(id);
            int count = 0;
            const QString changed =
                editor::replaceParagraphStyleInKml(kml, replace->first, replace->second, &count);
            if (count == 0) {
                continue;
            }
            const bool clean = std::find(dirty.cbegin(), dirty.cend(), id) == dirty.cend();
            projects.setChapterContent(id, changed);
            const bool saved = clean && projects.saveChapterContent(id);
            if (clean && !saved) {
                core::Logger::getInstance().warn(
                    "StyleCoordinator: the text of {} with the paragraphs of style {} moved "
                    "could not be saved",
                    id.toStdString(), replace->first.toStdString());
            }
            step.savedTexts.append({id, kml, saved});
            moved += count;
        }
    }

    // A later change of a text it changed makes it final
    m_undo = step;
    for (const QPointer<editor::BookEditor>& editor : std::as_const(m_undo->editors)) {
        if (editor) {
            m_undoWatch.append(connect(editor.data(), &editor::BookEditor::contentChanged, this,
                                       &StyleCoordinator::forgetUndo));
        }
    }
    emit stylesChanged();
    emit undoChanged();
    updateCommandStates();
    return moved;
}

// =============================================================================
// Undo
// =============================================================================

bool StyleCoordinator::canUndo() const {
    if (!m_undo || !whyNotChange().isEmpty()) {
        return false;
    }
    // Every open text it changed is still open, and no text it changed in its file is open now
    const QHash<QString, editor::BookEditor*> open = openEditors();
    const auto& editors = m_undo->editors;
    const QList<editor::BookEditor*> openNow = open.values();
    if (std::any_of(editors.cbegin(), editors.cend(),
                    [&openNow](const QPointer<editor::BookEditor>& editor) {
                        return editor.isNull() || !openNow.contains(editor.data());
                    })) {
        return false;
    }
    const auto& saved = m_undo->savedTexts;
    return std::none_of(saved.cbegin(), saved.cend(),
                        [&open](const SavedText& text) { return open.contains(text.elementId); });
}

QString StyleCoordinator::undoText() const {
    return m_undo ? m_undo->text : QString();
}

bool StyleCoordinator::undo() {
    if (!canUndo()) {
        forgetUndo();
        //: In Polish: Tego nie da się już cofnąć: teksty zmieniły się od tamtej pory.
        say(tr("This can no longer be undone: the texts changed since."));
        return false;
    }
    const Step step = *m_undo;
    forgetUndo();
    auto& projects = core::ProjectManager::getInstance();
    m_changing = true;
    const bool kept = projects.setParagraphStyles(step.entries);
    m_changing = false;
    if (!kept) {
        say(tr("The file of the book could not be saved."));
        return false;
    }
    for (const QPointer<editor::BookEditor>& editor : step.editors) {
        if (editor) {
            editor->undo();
        }
    }
    for (const SavedText& text : step.savedTexts) {
        projects.setChapterContent(text.elementId, text.kml);
        if (text.saved) {
            projects.saveChapterContent(text.elementId);
        }
    }
    //: In Polish: Cofnięto %1.
    say(tr("Undone %1.").arg(step.text));
    emit stylesChanged();
    emit undoChanged();
    updateCommandStates();
    return true;
}

void StyleCoordinator::forgetUndo() {
    for (const QMetaObject::Connection& connection : std::as_const(m_undoWatch)) {
        disconnect(connection);
    }
    m_undoWatch.clear();
    if (m_undo) {
        m_undo.reset();
        emit undoChanged();
    }
}

// =============================================================================
// Messages and the commands' states
// =============================================================================

void StyleCoordinator::notify(const QString& message) {
    EditorPanel* panel = panelInFront();
    if (!m_showNotice || panel == nullptr) {
        say(message);
        return;
    }
    m_showNotice(panel, message, tr("&Undo"),
                 //: The tooltip of Undo in the notice. In Polish: Cofnij %1
                 tr("Undo %1").arg(undoText()), [this]() { undo(); });
}

void StyleCoordinator::say(const QString& message) const {
    if (m_statusBar != nullptr && !message.isEmpty()) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

void StyleCoordinator::followEditor() {
    editor::BookEditor* text = editorInFront();
    if (text == m_followed) {
        return;
    }
    if (m_followed) {
        disconnect(m_followed.data(), nullptr, this, nullptr);
        // The watch of the undo stays
        if (m_undo) {
            for (const QPointer<editor::BookEditor>& editor : std::as_const(m_undo->editors)) {
                if (editor == m_followed) {
                    m_undoWatch.append(connect(editor.data(), &editor::BookEditor::contentChanged,
                                               this, &StyleCoordinator::forgetUndo));
                }
            }
        }
    }
    m_followed = text;
    if (text != nullptr) {
        connect(text, &editor::BookEditor::cursorPositionChanged, this,
                &StyleCoordinator::updateCommandStates);
        connect(text, &editor::BookEditor::selectionChanged, this,
                &StyleCoordinator::updateCommandStates);
    }
}

void StyleCoordinator::updateCommandStates() {
    auto& registry = CommandRegistry::getInstance();
    for (const char* id : {NEW_COMMAND, MODIFY_COMMAND, DELETE_COMMAND, MANAGE_COMMAND}) {
        registry.updateActionState(id);
    }
}

}  // namespace kalahari::gui
