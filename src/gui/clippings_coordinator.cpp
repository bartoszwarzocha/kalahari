// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clippings_coordinator.cpp
/// @brief Clippings: Edit > Cut to Clippings, Insert > Clipping..., and the way back to where
///        a clipping was cut from

#include "kalahari/gui/clippings_coordinator.h"

#include "kalahari/core/chapter_document.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/quick_insert.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/clipping_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/version_texts.h"

#include <QMimeData>
#include <QRect>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The choice of setting @p key (core::Clippings)
QString settingOf(const char* key) {
    return QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(key));
}

/// @brief Whether setting @p key has the choice @p value
bool settingIs(const char* key, const char* value) {
    return settingOf(key) == QLatin1String(value);
}

/// @brief The texts of the paragraphs of @p document (its blocks), for core::Clippings
QStringList paragraphsOf(const QTextDocument& document) {
    QStringList paragraphs;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        paragraphs << block.text();
    }
    return paragraphs;
}

/// @brief Whether @p block is in a table
bool inTable(const QTextBlock& block) {
    return QTextCursor(block).currentTable() != nullptr;
}

/// @brief Why the paragraph with the cursor is not cut: it is empty
QString nothingToCut() {
    //: Edit > Cut to Clippings without a selection, in an empty paragraph. In Polish: Akapit z
    //: kursorem jest pusty, a nic nie jest zaznaczone
    return ClippingsCoordinator::tr(
        "The paragraph with the cursor is empty, and nothing is selected");
}

/// @brief What Cut puts on the clipboard from the selection of @p text (KML), with the
///        formats, the notes and the annotations; empty when nothing can be taken
QString selectionKml(const editor::BookEditor& text) {
    const std::unique_ptr<QMimeData> data = text.createMimeDataFromSelection();
    return data != nullptr ? QString::fromUtf8(data->data(QString::fromLatin1(editor::MIME_KML)))
                           : QString();
}

/// @brief The paragraph of @p text with its cursor
QTextBlock cursorBlockOf(const editor::BookEditor& text) {
    const QTextDocument* document = text.textDocument();
    return document != nullptr ? document->findBlockByNumber(text.cursorPosition().paragraph)
                               : QTextBlock();
}

}  // namespace

ClippingsCoordinator::ClippingsCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                           QTabWidget* centralTabs,
                                           NavigatorCoordinator* navigator,
                                           TextWithKeys textWithKeys, Feedback feedback,
                                           QObject* parent)
    : QObject(parent),
      m_book(book),
      m_commands(commands),
      m_centralTabs(centralTabs),
      m_navigator(navigator),
      m_textWithKeys(std::move(textWithKeys)),
      m_showNotice(std::move(feedback.showNotice)),
      m_statusBar(feedback.statusBar),
      m_window(feedback.window) {
    // Another book, or none: the steps were in the old one
    connect(&m_book, &core::ProjectManager::projectOpened, this,
            &ClippingsCoordinator::forgetSteps);
    connect(&m_book, &core::ProjectManager::projectClosed, this,
            &ClippingsCoordinator::forgetSteps);
}

ClippingsCoordinator::~ClippingsCoordinator() {
    forgetSteps();
}

void ClippingsCoordinator::connectCommands() {
    if (Command* command = m_commands.getCommand(CUT_COMMAND)) {
        command->execute = [this]() { cut(m_textWithKeys ? m_textWithKeys() : nullptr); };
        command->disabledReason = [this]() {
            return whyNotCut(m_textWithKeys ? m_textWithKeys() : nullptr);
        };
    }
    if (Command* command = m_commands.getCommand(INSERT_COMMAND)) {
        command->execute = [this]() { insert(m_textWithKeys ? m_textWithKeys() : nullptr); };
        command->disabledReason = [this]() {
            return whyNotInsert(m_textWithKeys ? m_textWithKeys() : nullptr);
        };
    }
}

void ClippingsCoordinator::connectPanels(PropertiesPanel* properties,
                                         MaterialListCoordinator* lists) const {
    // What they did, or why not, the notices under the texts and the status bar say
    if (properties != nullptr) {
        connect(properties, &PropertiesPanel::requestClippingRestore, this,
                &ClippingsCoordinator::restore);
        connect(properties, &PropertiesPanel::requestClippingOrigin, this,
                &ClippingsCoordinator::showOrigin);
    }
    if (lists != nullptr) {
        connect(lists, &MaterialListCoordinator::clippingRestoreRequested, this,
                &ClippingsCoordinator::restore);
        connect(lists, &MaterialListCoordinator::clippingOriginRequested, this,
                &ClippingsCoordinator::showOrigin);
    }
}

void ClippingsCoordinator::connectMarks(ParagraphMarksCoordinator& marks) {
    connect(&marks, &ParagraphMarksCoordinator::clippingRestoreRequested, this,
            &ClippingsCoordinator::restore);
    if (m_navigator != nullptr) {
        connect(m_navigator, &NavigatorCoordinator::refreshNavigatorRequested, &marks,
                &ParagraphMarksCoordinator::refreshSoon);
    }
}

// =============================================================================
// Cut to Clippings
// =============================================================================

QString ClippingsCoordinator::whyNotText(const editor::BookEditor* text) const {
    if (!m_book.isProjectOpen()) {
        //: Why the clippings' commands are greyed out. In Polish: Żadna książka nie jest
        //: otwarta
        return tr("No book is open");
    }
    if (m_book.isReadOnly() || (text != nullptr && text->isReadOnly())) {
        return readOnlyReason();
    }
    if (text == nullptr || text->textDocument() == nullptr || elementOf(text) == nullptr) {
        //: Why the clippings' commands are greyed out: the keys are in a file outside the
        //: book, or not in a text. In Polish: Kursor nie jest w tekście książki ani w notatce
        //: roboczej
        return tr("The cursor is not in a text of the book or in a working note");
    }
    return {};
}

QString ClippingsCoordinator::whyNotCut(const editor::BookEditor* text) const {
    // Without a text there is always a reason
    if (QString why = whyNotText(text); !why.isEmpty() || text == nullptr) {
        return why;
    }
    if (const core::ProjectElement* element = elementOf(text);
        element != nullptr && core::ProjectManager::isClipping(*element)) {
        //: Why Edit > Cut to Clippings is greyed out. In Polish: Ten tekst jest już wycinkiem
        return tr("This text is a clipping already");
    }
    if (text->hasSelection()) {
        return {};
    }
    if (settingIs(core::Clippings::NO_SELECTION_SETTING, core::Clippings::NO_SELECTION_NOTHING)) {
        //: Why Edit > Cut to Clippings is greyed out. In Polish: Najpierw zaznacz fragment
        //: tekstu. Bez zaznaczenia polecenie wycina akapit z kursorem, gdy tak mówią
        //: Ustawienia.
        return tr("Select a fragment of the text first. Without a selection the command cuts "
                  "the paragraph with the cursor when the settings say so.");
    }
    return {};
}

bool ClippingsCoordinator::selectsWhatToCut(editor::BookEditor& text) {
    // Without a selection whyNotCut() lets it through only when the paragraph goes
    if (text.hasSelection()) {
        return true;
    }
    const QTextBlock block = cursorBlockOf(text);
    if (!block.isValid() || block.text().trimmed().isEmpty()) {
        return false;
    }
    // The paragraph with its end, as selecting it whole does: no empty paragraph stays. The
    // last paragraph, and one before a table, goes with the end of the paragraph before it; in
    // a table, and alone between tables, only its text goes.
    const int paragraph = block.blockNumber();
    const QTextBlock next = block.next();
    const QTextBlock previous = block.previous();
    editor::CursorPosition start{.paragraph = paragraph, .offset = 0};
    editor::CursorPosition finish{.paragraph = paragraph, .offset = block.length() - 1};
    if (!inTable(block)) {
        if (next.isValid() && !inTable(next)) {
            finish = {.paragraph = paragraph + 1, .offset = 0};
        } else if (previous.isValid() && !inTable(previous)) {
            start = {.paragraph = paragraph - 1, .offset = previous.length() - 1};
        }
    }
    text.setSelection({.start = start, .end = finish});
    return text.hasSelection();
}

QString ClippingsCoordinator::cut(editor::BookEditor* text) {
    if (const QString why = whyNotCut(text); !why.isEmpty() || text == nullptr) {
        say(why);
        return {};
    }
    if (!settingIs(core::Clippings::CUT_SETTING, core::Clippings::CUT_WITH_WINDOW)) {
        return cutAs(text, QString(), QString());
    }

    // The small window first, with the fragment that goes selected
    const bool hadSelection = text->hasSelection();
    const editor::CursorPosition cursor = text->cursorPosition();
    if (!selectsWhatToCut(*text)) {
        say(nothingToCut());
        return {};
    }
    const QString title =
        core::Clippings::titleOf(core::ChapterDocument::kmlToPlainText(selectionKml(*text)));
    dialogs::CutClippingDialog dialog(title, text->selectionCounts().words,
                                      elementOf(text)->title, m_window);
    if (dialog.exec() != QDialog::Accepted) {
        if (!hadSelection) {
            text->clearSelection();
            text->setCursorPosition(cursor);
        }
        return {};
    }
    if (dialog.cutAtOnceNextTime()) {
        core::SettingsManager::getInstance().set<std::string>(core::Clippings::CUT_SETTING,
                                                              core::Clippings::CUT_AT_ONCE);
    }
    return cutAs(text, dialog.title(), dialog.note());
}

QString ClippingsCoordinator::cutAs(editor::BookEditor* text, const QString& title,
                                    const QString& note) {
    if (const QString why = whyNotCut(text); !why.isEmpty() || text == nullptr) {
        say(why);
        return {};
    }
    if (!selectsWhatToCut(*text)) {
        say(nothingToCut());
        return {};
    }
    // A copy: the book changes
    const core::ProjectElement element = *elementOf(text);
    const QString kml = selectionKml(*text);
    if (kml.isEmpty()) {
        //: In Polish: Zaznaczenia nie da się wyciąć do wycinków
        say(tr("The selection cannot be cut to the clippings"));
        return {};
    }
    const int words = text->selectionCounts().words;
    const QString name = clippingName(title, kml);
    const QString cleanNote = note.trimmed();

    // The clipping first: when the book cannot take it, the text stays as it is
    QString clippingId = addCutClipping(name, kml, cleanNote, element.title);
    if (clippingId.isEmpty()) {
        return {};
    }
    const int levelBefore = text->undoLevel();
    if (!takeSelectionOf(*text)) {
        removeClipping(clippingId);
        say(tr("The selection cannot be cut to the clippings"));
        return {};
    }

    // Where it comes from: the place the cut left
    const editor::CursorPosition place = text->cursorPosition();
    core::ClippingData clipping;
    clipping.origin = core::Clippings::originAt(element.id, paragraphsOf(*text->textDocument()),
                                                place.paragraph, place.offset,
                                                QDateTime::currentDateTimeUtc());
    clipping.origin.title = element.title;
    if (!m_book.setClipping(clippingId, clipping)) {
        core::Logger::getInstance().warn(
            "ClippingsCoordinator: '{}' does not keep where it comes from", name.toStdString());
    }
    refreshAfter(text);
    core::Logger::getInstance().info(
        "ClippingsCoordinator: {} words of '{}' cut to the clipping '{}' ({})", words,
        element.title.toStdString(), name.toStdString(), clippingId.toStdString());

    // Ctrl+Z in the text takes it back, and Ctrl+Y cuts it again
    if (text->undoLevel() > levelBefore) {
        Step step;
        step.text = text;
        step.document = text->textDocument();
        step.level = text->undoLevel();
        step.isCut = true;
        step.clippingId = clippingId;
        step.existsDone = true;
        step.existsUndone = false;
        step.title = name;
        step.kml = kml;
        step.fileKml = m_book.chapterText(clippingId);
        step.note = cleanNote;
        step.dataDone = clipping;
        step.dataUndone = clipping;
        addStep(std::move(step));
    }

    //: The notice under the text after Cut to Clippings. %1: its words ("12 words"), %2: the
    //: title of the clipping, %3: the keys of Undo. In Polish: Wycięty fragment (%1) jest w
    //: wycinkach jako „%2”. %3 cofa wycięcie.
    notice(text,
           tr("The fragment cut (%1) is in the clippings as \"%2\". %3 takes the cut back.")
               .arg(wordCountText(words), name, undoKeys()),
           //: In Polish: Cofnij wycięcie: fragment wraca do tekstu
           tr("Take the cut back: the fragment comes back to the text"));
    return clippingId;
}

QString ClippingsCoordinator::clippingName(const QString& title, const QString& kml) const {
    if (QString name = title.trimmed(); !name.isEmpty()) {
        return name;
    }
    if (QString name = core::Clippings::titleOf(core::ChapterDocument::kmlToPlainText(kml));
        !name.isEmpty()) {
        return name;
    }
    return m_book.defaultTitle(m_book.clippingKind());  // an image alone
}

QString ClippingsCoordinator::addCutClipping(const QString& name, const QString& kml,
                                             const QString& note, const QString& from) {
    auto& logger = core::Logger::getInstance();
    QString clippingId = m_book.addClipping(name, kml, core::ClippingData());
    if (clippingId.isEmpty()) {
        logger.error("ClippingsCoordinator: the clipping of '{}' could not be added",
                     from.toStdString());
        dialogs::MessageDialog::warning(
            m_window,
            //: The title of the warnings of Cut to Clippings. In Polish: Nie wycięto do wycinków
            tr("Not Cut to the Clippings"),
            //: In Polish: Nie udało się dodać wycinka do książki, dlatego nic nie wycięto.
            //: Szczegóły są w dzienniku programu.
            tr("The clipping could not be added to the book, so nothing was cut. The "
               "program's log has the details."));
        return {};
    }
    if (!note.isEmpty() && !m_book.setNotes(clippingId, note)) {
        logger.warn("ClippingsCoordinator: the note of '{}' was not saved", name.toStdString());
    }
    return clippingId;
}

bool ClippingsCoordinator::takeSelectionOf(editor::BookEditor& text) {
    // The words deleted, or moved (the setting)
    const bool keepWords =
        core::SettingsManager::getInstance().get<bool>(core::Clippings::KEEP_WORDS_SETTING);
    m_changing = true;
    const bool taken = text.takeSelection(keepWords);
    m_changing = false;
    return taken;
}

void ClippingsCoordinator::notice(editor::BookEditor* text, const QString& message,
                                  const QString& undoToolTip) {
    if (EditorPanel* panel = panelOf(text); m_showNotice && panel != nullptr) {
        //: In Polish: Cofn&ij
        m_showNotice(panel, message, tr("&Undo"), undoToolTip, [this]() { undoLast(); });
    } else {
        say(message);
    }
}

void ClippingsCoordinator::refreshAfter(const editor::BookEditor* text) {
    saveText(text);
    if (m_navigator != nullptr) {
        m_navigator->refreshNavigator();
    }
}

bool ClippingsCoordinator::canUndoLast() const {
    const auto step = std::ranges::find(m_steps, m_lastStepId, &Step::id);
    return step != m_steps.cend() && step->done && step->text != nullptr &&
           step->text->textDocument() == step->document &&
           step->text->undoLevel() == step->level && !step->text->isReadOnly();
}

bool ClippingsCoordinator::undoLast() {
    if (!canUndoLast()) {
        //: In Polish: Tego nie da się już cofnąć z paska: tekst zmienił się od tamtej pory.
        say(tr("This can no longer be undone from the notice: the text changed since."));
        return false;
    }
    editor::BookEditor* text = stepWithId(m_lastStepId)->text;
    text->undo();
    text->setFocus(Qt::OtherFocusReason);
    return true;
}

// =============================================================================
// Insert > Clipping
// =============================================================================

QString ClippingsCoordinator::whyNotInsert(const editor::BookEditor* text) const {
    if (QString why = whyNotText(text); !why.isEmpty()) {
        return why;
    }
    // The keys follow it often: the clippings are not read, only found
    const QString textId = elementOf(text)->id;
    const QStringList ids = m_book.textElementIds();
    if (std::ranges::none_of(ids, [this, &textId](const QString& elementId) {
            const core::ProjectElement* element = m_book.findElement(elementId);
            return elementId != textId && element != nullptr &&
                   core::ProjectManager::isClipping(*element);
        })) {
        //: Why Insert > Clipping is greyed out. In Polish: Książka nie ma jeszcze wycinków
        //: (Edycja > Wytnij do wycinków)
        return tr("The book has no clippings yet (Edit > Cut to Clippings)");
    }
    return {};
}

void ClippingsCoordinator::insert(editor::BookEditor* text) {
    if (const QString why = whyNotInsert(text); !why.isEmpty() || text == nullptr) {
        say(why);
        return;
    }
    const core::ProjectElement element = *elementOf(text);
    const bool keepsInserted =
        settingIs(core::Clippings::AFTER_INSERT_SETTING, core::Clippings::AFTER_INSERT_KEEP);
    QList<ClippingEntry> all = clippings(true);
    all.removeIf([&element](const ClippingEntry& each) { return each.id == element.id; });

    if (settingIs(core::Clippings::INSERT_SETTING, core::Clippings::INSERT_LIST)) {
        // The list at the cursor, which closes with the choice
        // Into the text open then: by its id, so that nothing goes into a text closed since
        auto* popup = new dialogs::ClippingListPopup(all, m_window);
        connect(popup, &dialogs::ClippingListPopup::chosen, this,
                [this, textId = element.id](ClippingChoice choice, const QString& clippingId) {
                    if (editor::BookEditor* into = editorOf(textId)) {
                        act(into, choice, clippingId);
                    }
                });
        const QRect cursor =
            text->inputMethodQuery(Qt::ImCursorRectangle).toRectF().toAlignedRect();
        popup->showAt(QRect(text->mapToGlobal(cursor.topLeft()), cursor.size()));
        return;
    }
    dialogs::InsertClippingDialog dialog(
        all, [this](const QString& clippingId) { return kmlOf(clippingId); }, text,
        element.title, element.id, keepsInserted, m_window);
    dialog.exec();
    act(text, dialog.choice(), dialog.chosenId());
}

void ClippingsCoordinator::act(editor::BookEditor* text,
                               ClippingChoice choice,
                               const QString& clippingId) {
    switch (choice) {
    case ClippingChoice::Insert:
        insertAt(text, clippingId);
        break;
    case ClippingChoice::Restore:
        restore(clippingId);
        break;
    case ClippingChoice::Open:
        if (editor::BookEditor* opened = open(clippingId)) {
            opened->setFocus(Qt::OtherFocusReason);
        }
        break;
    case ClippingChoice::None:
        break;
    }
}

bool ClippingsCoordinator::insertAt(editor::BookEditor* text, const QString& clippingId) {
    const core::ProjectElement* found = clippingToInsert(text, clippingId);
    if (found == nullptr) {
        return false;
    }
    // Copies: the book changes
    const core::ProjectElement into = *elementOf(text);
    const core::ProjectElement clipping = *found;
    const bool keeps =
        settingIs(core::Clippings::AFTER_INSERT_SETTING, core::Clippings::AFTER_INSERT_KEEP);
    if (!keeps && !savedBeforeGoing(clipping)) {
        return false;
    }
    const QString kml = kmlOf(clippingId);
    const QString note = m_book.notes(clippingId);
    const int words = m_book.wordCount(clippingId);
    if (kml.isEmpty()) {
        //: In Polish: Ten wycinek jest pusty
        say(tr("This clipping is empty"));
        return false;
    }
    if (!pasteInto(*text, kml)) {
        //: In Polish: Tego wycinka nie da się tu wstawić
        say(tr("This clipping cannot be inserted here"));
        return false;
    }

    // It stays marked as inserted, or goes (the setting)
    core::ClippingData inserted = clipping.clipping;
    inserted.inserted.append(core::ClippingInsertion{
        .element = into.id, .at = QDateTime::currentDateTimeUtc(), .extra = QJsonObject()});
    const QString fileKml = m_book.chapterText(clippingId);
    keepOrRemove(clipping, inserted, keeps);
    refreshAfter(text);
    core::Logger::getInstance().info("ClippingsCoordinator: the clipping '{}' inserted into '{}'",
                                     clipping.title.toStdString(), into.title.toStdString());

    Step step;
    step.text = text;
    step.document = text->textDocument();
    step.level = text->undoLevel();
    step.isCut = false;
    step.clippingId = clippingId;
    step.existsDone = keeps;
    step.existsUndone = true;
    step.title = clipping.title;
    step.kml = kml;
    step.fileKml = fileKml;
    step.note = note;
    step.dataDone = keeps ? inserted : clipping.clipping;
    step.dataUndone = clipping.clipping;
    addStep(std::move(step));

    //: In Polish: Cofnij wstawienie wycinka
    notice(text, insertedMessage(clipping.title, words, keeps, undoKeys()),
           tr("Take the insertion of the clipping back"));
    text->setFocus(Qt::OtherFocusReason);
    return true;
}

const core::ProjectElement* ClippingsCoordinator::clippingToInsert(
    const editor::BookEditor* text, const QString& clippingId) const {
    if (const QString why = whyNotText(text); !why.isEmpty()) {
        say(why);
        return nullptr;
    }
    const core::ProjectElement* found = m_book.findElement(clippingId);
    if (found == nullptr || !core::ProjectManager::isClipping(*found)) {
        //: In Polish: Tego wycinka nie ma już w Warsztacie
        say(tr("The clipping is no longer in the Workshop"));
        return nullptr;
    }
    if (const core::ProjectElement* into = elementOf(text);
        into == nullptr || into->id == clippingId) {
        //: In Polish: Wycinka nie da się wstawić do niego samego
        say(tr("A clipping cannot be inserted into itself"));
        return nullptr;
    }
    return found;
}

bool ClippingsCoordinator::savedBeforeGoing(const core::ProjectElement& clipping) {
    // The clipping that goes from the Workshop has the changes of its tab saved first
    if (m_navigator == nullptr || m_navigator->saveTextsOf(clipping).isEmpty()) {
        return true;
    }
    dialogs::MessageDialog::warning(
        m_window,
        //: The title of the warning. In Polish: Nie wstawiono wycinka
        tr("Clipping Not Inserted"),
        //: In Polish: Nie udało się zapisać w pliku zmian w tym wycinku, dlatego go nie
        //: wstawiono. Szczegóły są w dzienniku programu.
        tr("The changes of this clipping could not be saved to its file, so it was not "
           "inserted. The program's log has the details."));
    return false;
}

bool ClippingsCoordinator::pasteInto(editor::BookEditor& text, const QString& kml) {
    // In at the cursor, as a paste
    const int levelBefore = text.undoLevel();
    const std::unique_ptr<QMimeData> data =
        editor::ClipboardHandler::mimeDataOf(kml, nullptr, m_book.imagesBaseFolder());
    m_changing = true;
    text.insertFromMimeData(data.get());
    m_changing = false;
    return text.undoLevel() != levelBefore;
}

void ClippingsCoordinator::keepOrRemove(const core::ProjectElement& clipping,
                                        const core::ClippingData& inserted, bool keeps) {
    auto& logger = core::Logger::getInstance();
    if (keeps) {
        if (!m_book.setClipping(clipping.id, inserted)) {
            logger.warn("ClippingsCoordinator: '{}' is not marked as inserted",
                        clipping.title.toStdString());
        }
    } else if (!removeClipping(clipping.id)) {
        logger.warn("ClippingsCoordinator: '{}' stays in the Workshop",
                    clipping.title.toStdString());
    }
}

QString ClippingsCoordinator::insertedMessage(const QString& title, int words, bool keeps,
                                              const QString& undoKeys) {
    if (keeps) {
        //: The notice under the text after Insert > Clipping. %1: the title of the
        //: clipping, %2: its words ("12 words"), %3: the keys of Undo. In Polish:
        //: Wstawiono wycinek „%1” (%2). Zostaje w Warsztacie, oznaczony jako wstawiony.
        //: %3 cofa wstawienie.
        return tr("Clipping \"%1\" inserted (%2). It stays in the Workshop, marked as inserted. "
                  "%3 takes the insertion back.")
            .arg(title, wordCountText(words), undoKeys);
    }
    //: The notice under the text after Insert > Clipping. In Polish: Wstawiono
    //: wycinek „%1” (%2) i usunięto go z Warsztatu. %3 cofa jedno i drugie.
    return tr("Clipping \"%1\" inserted (%2) and taken from the Workshop. %3 takes both "
              "back.")
        .arg(title, wordCountText(words), undoKeys);
}

bool ClippingsCoordinator::restore(const QString& clippingId) {
    if (!showOrigin(clippingId)) {
        return false;
    }
    const core::ProjectElement* clipping = m_book.findElement(clippingId);
    editor::BookEditor* text = editorOf(clipping->clipping.origin.element);
    const std::optional<core::ClippingPlace> place =
        core::Clippings::placeIn(paragraphsOf(*text->textDocument()), clipping->clipping.origin);
    if (place && place->exact) {
        return insertAt(text, clippingId);
    }

    // The words around the place changed: the writer finds it
    //: The notice under the text of Restore to the Place It Came From. In Polish: Dokładnego
    //: miejsca nie ma już w tekście. Kursor jest w akapicie, z którego pochodzi fragment.
    const QString message = tr("The exact place is no longer in the text. The cursor is in the "
                               "paragraph the fragment came from.");
    if (EditorPanel* panel = panelOf(text); m_showNotice && panel != nullptr) {
        // Into the text while it is open (by its id, as the notice may outlive it)
        //: In Polish: Wstaw t&utaj / Wstaw wycinek w miejscu kursora
        m_showNotice(panel, message, tr("I&nsert Here"), tr("Insert the clipping at the cursor"),
                     [this, textId = clipping->clipping.origin.element, clippingId]() {
                         if (editor::BookEditor* into = editorOf(textId)) {
                             insertAt(into, clippingId);
                         }
                     });
    } else {
        say(message);
    }
    return false;
}

bool ClippingsCoordinator::showOrigin(const QString& clippingId) {
    const core::ProjectElement* clipping = m_book.findElement(clippingId);
    if (clipping == nullptr || !core::ProjectManager::isClipping(*clipping)) {
        say(tr("The clipping is no longer in the Workshop"));
        return false;
    }
    const core::ClippingOrigin origin = clipping->clipping.origin;
    if (core::isEmpty(origin)) {
        //: In Polish: Ten wycinek dodano ręcznie: nie ma pierwotnego miejsca
        say(tr("This clipping was added by hand: it has no place it came from"));
        return false;
    }
    if (m_book.findElement(origin.element) == nullptr) {
        //: In Polish: Tekstu, z którego wycięto ten wycinek, nie ma już w książce
        say(tr("The text this clipping was cut from is no longer in the book"));
        return false;
    }
    editor::BookEditor* text = open(origin.element);
    if (text == nullptr || text->textDocument() == nullptr) {
        return false;
    }
    const std::optional<core::ClippingPlace> place =
        core::Clippings::placeIn(paragraphsOf(*text->textDocument()), origin);
    if (place) {
        text->clearSelection();
        text->setCursorPosition({.paragraph = place->paragraph, .offset = place->offset});
        text->ensureCursorVisible();
    }
    text->setFocus(Qt::OtherFocusReason);
    return true;
}

QList<ClippingEntry> ClippingsCoordinator::clippings(bool withInserted) const {
    QList<ClippingEntry> entries;
    if (!m_book.isProjectOpen()) {
        return entries;
    }
    const QStringList ids = m_book.textElementIds();
    // The last added first: those added by hand, without the time of a cut, keep that order
    for (const QString& elementId : std::views::reverse(ids)) {
        const core::ProjectElement* element = m_book.findElement(elementId);
        if (element != nullptr && core::ProjectManager::isClipping(*element) &&
            (withInserted || element->clipping.inserted.isEmpty())) {
            entries.append(entryOf(*element));
        }
    }
    std::ranges::stable_sort(entries, [](const ClippingEntry& first, const ClippingEntry& second) {
        return first.cut.isValid() && (!second.cut.isValid() || first.cut > second.cut);
    });
    return entries;
}

ClippingEntry ClippingsCoordinator::entryOf(const core::ProjectElement& clipping) const {
    const core::ClippingData& data = clipping.clipping;
    ClippingEntry entry;
    entry.id = clipping.id;
    entry.title = clipping.title;
    entry.inserted = !data.inserted.isEmpty();
    entry.fromId = data.origin.element;
    const ClippingFrom from = ClippingTexts::fromOf(m_book, data);
    entry.from = from.title;
    entry.originGone = !entry.fromId.isEmpty() && !from.live;
    entry.cut = data.origin.cut;
    entry.words = m_book.wordCount(clipping.id);
    entry.note = m_book.notes(clipping.id).trimmed();
    entry.search = editor::foldForSearch(
        entry.title + QLatin1Char('\n') +
        core::ChapterDocument::kmlToPlainText(kmlOf(clipping.id)) + QLatin1Char('\n') +
        entry.note);
    return entry;
}

QString ClippingsCoordinator::kmlOf(const QString& clippingId) const {
    if (const editor::BookEditor* text = editorOf(clippingId)) {
        return text->toKml();
    }
    return m_book.chapterText(clippingId);
}

}  // namespace kalahari::gui
