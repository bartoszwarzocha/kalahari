/// @file footnote_coordinator.cpp
/// @brief Insert > Footnote and Endnote, and the frame a note's text is written in

#include "kalahari/gui/footnote_coordinator.h"

#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/note_frame.h"

#include <QApplication>
#include <QTabWidget>

#include <utility>

namespace kalahari::gui {

namespace {

/// The commands this coordinator gives callbacks
constexpr const char* FOOTNOTE_COMMAND = "insert.footnote";
constexpr const char* ENDNOTE_COMMAND = "insert.endnote";

}  // namespace

FootnoteCoordinator::FootnoteCoordinator(QTabWidget* centralTabs, TextWithKeys textWithKeys,
                                         QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_textWithKeys(std::move(textWithKeys))
{
    // A text opened: its clicks on the notes open their frames
    if (m_centralTabs != nullptr) {
        DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        connect(&windows, &DocumentWindows::windowsChanged, this,
                &FootnoteCoordinator::attachEditors);
        connect(&windows, &DocumentWindows::frontChanged, this,
                &FootnoteCoordinator::attachEditors);
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
    attachEditors();
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
        connect(text, &QObject::destroyed, this, [this, text]() { m_attached.removeAll(text); });
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
