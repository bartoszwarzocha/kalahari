/// @file footnote_coordinator.cpp
/// @brief Insert > Footnote and Endnote, the frame a note's text is written in, and the
///        numbers of the notes of every open text in the book around it

#include "kalahari/gui/footnote_coordinator.h"

#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/footnote_settings_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/note_frame.h"
#include "kalahari/gui/read_only_book.h"

#include <QApplication>
#include <QTabWidget>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// How long after a change of notes the other texts get their numbers (typing stays quick)
constexpr int REFRESH_DELAY_MS = 300;

/// The commands this coordinator gives callbacks
constexpr const char* FOOTNOTE_COMMAND = "insert.footnote";
constexpr const char* ENDNOTE_COMMAND = "insert.endnote";
constexpr const char* SETTINGS_COMMAND = "insert.footnoteSettings";

/// The notes of a kind among @p notes
int countOf(const std::vector<editor::Footnote>& notes, editor::NoteKind kind) {
    return static_cast<int>(std::count_if(notes.cbegin(), notes.cend(),
                                          [kind](const editor::Footnote& note) {
                                              return note.kind == kind;
                                          }));
}

}  // namespace

FootnoteCoordinator::FootnoteCoordinator(QTabWidget* centralTabs, TextWithKeys textWithKeys,
                                         ChapterOpener openChapter, QWidget* window,
                                         QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_textWithKeys(std::move(textWithKeys)),
      m_openChapter(std::move(openChapter)),
      m_window(window)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(REFRESH_DELAY_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FootnoteCoordinator::refresh);

    // Another book: nothing kept of the texts of the last one; its settings
    auto& projects = core::ProjectManager::getInstance();
    const auto bookChanged = [this]() {
        m_savedTexts.clear();
        refreshSoon();
        CommandRegistry::getInstance().updateActionState(SETTINGS_COMMAND);
    };
    connect(&projects, &core::ProjectManager::projectOpened, this, bookChanged);
    connect(&projects, &core::ProjectManager::projectClosed, this, bookChanged);
    connect(&projects, &core::ProjectManager::bookFootnotesChanged, this,
            &FootnoteCoordinator::refresh);

    // A text opened, closed or brought to the front: its clicks on the notes open their
    // frames, the numbers follow it
    if (m_centralTabs != nullptr) {
        DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        const auto textsChanged = [this]() {
            attachEditors();
            refreshSoon();
        };
        connect(&windows, &DocumentWindows::windowsChanged, this, textsChanged);
        connect(&windows, &DocumentWindows::frontChanged, this, textsChanged);
    }
}

void FootnoteCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(FOOTNOTE_COMMAND)) {
        command->execute = [this]() { insertNote(editor::NoteKind::Footnote); };
    }
    if (Command* command = registry.getCommand(ENDNOTE_COMMAND)) {
        command->execute = [this]() { insertNote(editor::NoteKind::Endnote); };
    }
    if (Command* command = registry.getCommand(SETTINGS_COMMAND)) {
        command->execute = [this]() { editSettings(); };
        command->isEnabled = [this]() { return whyNotEditSettings().isEmpty(); };
        command->disabledReason = [this]() { return whyNotEditSettings(); };
        registry.updateActionState(SETTINGS_COMMAND);
    }
    attachEditors();
}

// =============================================================================
// The book
// =============================================================================

core::BookFootnotes FootnoteCoordinator::footnotes() const {
    const auto& projects = core::ProjectManager::getInstance();
    const std::optional<core::BookFootnotes> own =
        projects.isProjectOpen() ? projects.bookFootnotes() : std::nullopt;
    return own.value_or(core::BookFootnotes{});
}

bool FootnoteCoordinator::setFootnotes(const core::BookFootnotes& footnotes) {
    auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen() || projects.isReadOnly()) {
        return false;
    }
    return projects.setBookFootnotes(footnotes);
}

QList<std::pair<QString, editor::BookEditor*>> FootnoteCoordinator::openEditors() const {
    QList<std::pair<QString, editor::BookEditor*>> editors;
    if (m_centralTabs == nullptr) {
        return editors;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel != nullptr && panel->getBookEditor() != nullptr) {
            editors.append({panel->property("elementId").toString(), panel->getBookEditor()});
        }
    }
    return editors;
}

const FootnoteCoordinator::CachedText& FootnoteCoordinator::savedText(
    const QString& elementId) const {
    CachedText& cached = m_savedTexts[elementId];
    if (const std::optional<QString> kml = savedTextIfChanged(elementId, cached.signature)) {
        cached.notes = editor::notesOfKml(*kml);
    }
    return cached;
}

std::vector<FootnoteCoordinator::BookText> FootnoteCoordinator::bookTexts() const {
    std::vector<BookText> texts;
    const std::vector<BookTextElement> elements = bookTextElements();
    if (elements.empty()) {
        return texts;
    }
    // The open texts as their editors have them
    QHash<QString, const editor::BookEditor*> open;
    for (const auto& [elementId, editor] : openEditors()) {
        if (!elementId.isEmpty()) {
            open.insert(elementId, editor);
        }
    }
    for (const BookTextElement& element : elements) {
        BookText text;
        text.elementId = element.element->id;
        text.chapter = element.chapter;
        if (const editor::BookEditor* editor = open.value(text.elementId, nullptr)) {
            text.notes = editor->noteList();
        } else {
            text.notes = savedText(text.elementId).notes;
        }
        texts.push_back(std::move(text));
    }
    return texts;
}

editor::FootnoteContext FootnoteCoordinator::contextAt(const std::vector<BookText>& texts,
                                                       int index,
                                                       const core::BookFootnotes& footnotes) {
    editor::FootnoteContext context;
    context.footnoteFormat = editor::noteNumberFormatOf(footnotes.footnotes.format);
    context.endnoteFormat = editor::noteNumberFormatOf(footnotes.endnotes.format);
    if (index < 0 || index >= static_cast<int>(texts.size())) {
        return context;  // numbered alone
    }

    // The numbers go on from the notes of the texts before it, when the book numbers them
    // through; else each text starts from 1
    int footnotesBefore = 0;
    int endnotesBefore = 0;
    for (int i = 0; i < index; ++i) {
        const std::vector<editor::Footnote>& notes = texts[static_cast<size_t>(i)].notes;
        footnotesBefore += countOf(notes, editor::NoteKind::Footnote);
        endnotesBefore += countOf(notes, editor::NoteKind::Endnote);
    }
    context.footnotesBefore = footnotes.footnotes.perChapter ? 0 : footnotesBefore;
    context.endnotesBefore = footnotes.endnotes.perChapter ? 0 : endnotesBefore;
    if (!footnotes.endnotesAtBookEnd) {
        return context;
    }

    // At the end of the book: after the last paragraph of the last chapter (without one, of
    // the last text), every text's endnotes in reading order, numbered as in their texts
    int last = static_cast<int>(texts.size()) - 1;
    for (int i = last; i >= 0; --i) {
        if (texts[static_cast<size_t>(i)].chapter) {
            last = i;
            break;
        }
    }
    context.endnotesHere = index == last;
    if (index != last) {
        return context;
    }
    int inBook = 0;
    for (int i = 0; i < static_cast<int>(texts.size()); ++i) {
        const BookText& text = texts[static_cast<size_t>(i)];
        int inText = 0;
        for (const editor::Footnote& note : text.notes) {
            if (note.kind != editor::NoteKind::Endnote) {
                continue;
            }
            ++inText;
            ++inBook;
            if (i == index) {
                continue;  // the text shows its own
            }
            const int number = footnotes.endnotes.perChapter ? inText : inBook;
            context.otherEndnotes.push_back(
                {text.elementId, note.id, editor::noteNumberText(number, context.endnoteFormat),
                 note.content});
        }
        if (i < index) {
            context.otherEndnotesBefore = context.otherEndnotes.size();
        }
    }
    return context;
}

editor::FootnoteContext FootnoteCoordinator::contextFor(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return {};  // a file outside the book: numbered alone
    }
    const std::vector<BookText> texts = bookTexts();
    const auto found = std::find_if(texts.cbegin(), texts.cend(), [&elementId](const BookText& text) {
        return text.elementId == elementId;
    });
    return contextAt(texts, found != texts.cend() ? static_cast<int>(found - texts.cbegin()) : -1,
                     footnotes());
}

void FootnoteCoordinator::refreshSoon() {
    m_refreshTimer.start();
}

void FootnoteCoordinator::refresh() {
    m_refreshTimer.stop();
    attachEditors();
    const std::vector<BookText> texts = bookTexts();
    const core::BookFootnotes book = footnotes();
    for (const auto& [elementId, editor] : openEditors()) {
        editor::FootnoteContext context;  // a file outside the book: numbered alone
        if (!elementId.isEmpty()) {
            int index = -1;
            for (int i = 0; i < static_cast<int>(texts.size()); ++i) {
                if (texts[static_cast<size_t>(i)].elementId == elementId) {
                    index = i;
                    break;
                }
            }
            context = contextAt(texts, index, book);
        }
        editor->setFootnoteContext(context);  // nothing when it is the same
    }
}

bool FootnoteCoordinator::editSettings() {
    if (!whyNotEditSettings().isEmpty()) {
        return false;
    }
    dialogs::FootnoteSettingsDialog dialog(footnotes(), m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const core::BookFootnotes chosen = dialog.footnotes();
    if (chosen == footnotes()) {
        return false;
    }
    return setFootnotes(chosen);
}

QString FootnoteCoordinator::whyNotEditSettings() const {
    const auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen()) {
        //: Why Insert > Footnote Layout... is greyed out.
        //: In Polish: Żadna książka nie jest otwarta
        return tr("No book is open");
    }
    return projects.isReadOnly() ? readOnlyReason() : QString();
}

void FootnoteCoordinator::openBookEndnote(const QString& elementId, const QString& id) {
    if (!m_openChapter || elementId.isEmpty()) {
        return;
    }
    m_openChapter(elementId);
    refresh();
    for (const auto& [openId, editor] : openEditors()) {
        if (openId == elementId) {
            // Its frame after its number; in a read-only text the cursor there
            if (!editNote(*editor, id) && editor->goToNote(id)) {
                editor->setFocus();
            }
            return;
        }
    }
}

// =============================================================================
// The commands
// =============================================================================

bool FootnoteCoordinator::insertNote(editor::NoteKind kind) {
    editor::BookEditor* editor = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (editor == nullptr || editor->isReadOnly() || !editor->annotationsAllowed()) {
        return false;
    }
    finishWriting();  // the text of the open frame is kept
    attachEditors();
    const QString id = editor->insertNote(kind);
    if (id.isEmpty()) {
        return false;
    }
    openFrame(Writing{editor, id, true});
    return true;
}

bool FootnoteCoordinator::editNote(editor::BookEditor& editor, const QString& id) {
    if (editor.isReadOnly() || !editor.noteOf(id)) {
        return false;
    }
    if (m_writing && m_writing->editor == &editor && m_writing->id == id && m_frame != nullptr) {
        m_frame->startTyping();  // already written in its frame
        return true;
    }
    finishWriting();  // the text of the open frame is kept
    openFrame(Writing{&editor, id, false});
    return true;
}

void FootnoteCoordinator::finishWriting(const editor::BookEditor* editor) {
    if (!m_writing || (editor != nullptr && m_writing->editor != editor)) {
        return;
    }
    saveWriting();
}

// =============================================================================
// The editors
// =============================================================================

void FootnoteCoordinator::attachEditors() {
    if (m_centralTabs == nullptr) {
        return;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        editor::BookEditor* const text = panel != nullptr ? panel->getBookEditor() : nullptr;
        if (text == nullptr || m_attached.contains(text)) {
            continue;
        }
        m_attached.append(text);
        connect(text, &editor::BookEditor::noteEditRequested, this,
                [this, text](const QString& id) { editNote(*text, id); });
        connect(text, &editor::BookEditor::notesChanged, this,
                &FootnoteCoordinator::refreshSoon);
        connect(text, &editor::BookEditor::bookEndnoteOpenAsked, this,
                &FootnoteCoordinator::openBookEndnote);
        connect(text, &QObject::destroyed, this, [this, text]() {
            m_attached.removeAll(text);
            refreshSoon();
        });
    }
}

// =============================================================================
// The frame
// =============================================================================

void FootnoteCoordinator::openFrame(const Writing& writing) {
    editor::BookEditor* editor = writing.editor;
    const std::optional<editor::NotePlace> place = editor->noteOf(writing.id);
    if (!place) {
        return;
    }
    m_writing = writing;
    auto* frame = new NoteFrame(editor);
    m_frame = frame;
    frame->setNote(place->note.kind, editor->noteNumber(writing.id));
    frame->setContent(place->note.content);
    frame->setTextFont(editor->noteFrameFont());
    connect(frame, &NoteFrame::saveRequested, this, &FootnoteCoordinator::saveWriting);
    connect(frame, &NoteFrame::cancelRequested, this, &FootnoteCoordinator::cancelWriting);
    connect(frame, &NoteFrame::closeRequested, this, &FootnoteCoordinator::saveWriting);

    // A closed tab takes the frame with its editor: nothing is left to write
    connect(frame, &QObject::destroyed, this, [this, frame]() {
        if (m_frame == frame) {
            m_frame = nullptr;
            m_writing.reset();
        }
    });

    // In the colors of the paper, also when the paper or the theme changes; the font follows
    // the zoom
    connect(editor, &editor::BookEditor::appearanceChanged, frame, [this]() { colorFrame(); });
    connect(editor, &editor::BookEditor::zoomChanged, frame,
            [frame, editor]() { frame->setTextFont(editor->noteFrameFont()); });
    // Another view puts the note elsewhere: its text is kept
    connect(editor, &editor::BookEditor::viewModeChanged, frame, [this]() { saveWriting(); });
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, frame, [this]() { colorFrame(); });
    connect(&themes, &core::ThemeManager::editorColorsChanged, frame, [this]() { colorFrame(); });
    colorFrame();

    // In the continuous views the note makes room for the frame, at the place of its text
    const QString id = writing.id;
    if (editor->viewMode() != editor::ViewMode::Page) {
        editor->setNoteFrameRoom(id, frame->sizeHint().height());
    }
    connect(frame, &NoteFrame::cardHeightNeeded, editor,
            [editor, id](int height) { editor->setNoteFrameRoom(id, height); });

    // Over the note's text, as the text moves; after its number when the note is not shown
    frame->setPlacement([this]() {
        NoteFrame::Placement placement;
        if (!m_writing || m_writing->editor == nullptr) {
            return placement;
        }
        editor::BookEditor* text = m_writing->editor;
        if (const std::optional<editor::NotePlace> note = text->noteOf(m_writing->id)) {
            placement.place = text->placeRect(note->position + 1, true);
            placement.column = text->textColumnRect(placement.place.center().y());
            placement.card = text->noteRect(m_writing->id);
            placement.cardMakesRoom = text->viewMode() != editor::ViewMode::Page;
        }
        return placement;
    });
    editor->scrollToNote(id);
    frame->place();
    frame->show();
    frame->raise();
    frame->startTyping();
}

void FootnoteCoordinator::saveWriting() {
    if (!m_writing) {
        return;
    }
    const Writing writing = *m_writing;
    if (writing.editor != nullptr && m_frame != nullptr) {
        writing.editor->setNoteContent(writing.id, m_frame->content(), writing.isNew);
    }
    closeFrame();
}

void FootnoteCoordinator::cancelWriting() {
    if (!m_writing) {
        return;
    }
    const Writing writing = *m_writing;
    closeFrame();
    if (writing.isNew && writing.editor != nullptr) {
        writing.editor->cancelNewNote(writing.id);
    }
}

void FootnoteCoordinator::closeFrame() {
    NoteFrame* frame = m_frame;
    const std::optional<Writing> writing = m_writing;
    m_frame = nullptr;
    m_writing.reset();
    if (frame == nullptr) {
        return;
    }
    frame->disconnect(this);  // its going tells nothing any more
    if (writing && writing->editor != nullptr) {
        frame->disconnect(writing->editor);
        writing->editor->setNoteFrameRoom(writing->id, 0.0);  // its text shows again

        // The keys go back to the text, before the frame goes
        const QWidget* focus = QApplication::focusWidget();
        if (focus != nullptr && (focus == frame || frame->isAncestorOf(focus))) {
            writing->editor->setFocus(Qt::OtherFocusReason);
        }
    }
    frame->hide();
    frame->deleteLater();
}

void FootnoteCoordinator::colorFrame() {
    if (m_frame == nullptr || !m_writing || m_writing->editor == nullptr) {
        return;
    }
    // The frame's color on this paper, the text and the links as on it
    const editor::EditorAppearance& appearance = m_writing->editor->appearance();
    const editor::EditorColorMode mode = appearance.colorMode;
    const QColor paper = appearance.colors.background(mode);
    const QColor text = appearance.colors.textColor(mode);
    const auto& themes = core::ThemeManager::getInstance();
    const QColor frameColor =
        shownColor(appearance.colors.noteFrameColor(mode), paper, themes.optimizesEditorColors());
    m_frame->setColors(annotationCardColors(frameColor, paper, text));
    m_frame->setLinkColor(appearance.colors.linkColor(mode));
}

}  // namespace kalahari::gui
