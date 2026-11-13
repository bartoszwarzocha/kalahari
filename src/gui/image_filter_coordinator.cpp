/// @file image_filter_coordinator.cpp
/// @brief Image > Filter > Same Filter for All Images...: the filter of one image for every
///        image of the book, with Undo

#include "kalahari/gui/image_filter_coordinator.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QHash>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextDocument>

#include <algorithm>
#include <utility>
#include <vector>

namespace kalahari::gui {

namespace {

/// How long a short message stays in the status bar, in milliseconds
constexpr int MESSAGE_MS = 8000;

}  // namespace

ImageFilterCoordinator::ImageFilterCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                                               NoticeShower showNotice, QStatusBar* statusBar,
                                               QWidget* window, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_showNotice(std::move(showNotice)),
      m_statusBar(statusBar),
      m_window(window),
      m_book(book) {
    // Another book: the change is final
    auto& projects = m_book;
    connect(&projects, &core::ProjectManager::projectOpened, this, &ImageFilterCoordinator::forgetUndo);
    connect(&projects, &core::ProjectManager::projectClosed, this, &ImageFilterCoordinator::forgetUndo);
}

QString ImageFilterCoordinator::filterName(editor::ImageFilter filter)
{
    switch (filter) {
    case editor::ImageFilter::Grayscale:
        //: The names of the filters in the messages of Same Filter for All Images. In Polish:
        //: Odcienie szarości
        return tr("Grayscale");
    case editor::ImageFilter::Sepia:
        //: In Polish: Sepia
        return tr("Sepia");
    case editor::ImageFilter::BlackWhite:
        //: In Polish: Czarno-biały
        return tr("Black and White");
    case editor::ImageFilter::Wash:
        //: In Polish: Znak wodny
        return tr("Washout");
    case editor::ImageFilter::None:
        break;
    }
    //: The filter of an image as its file has it. In Polish: Oryginał
    return tr("Original");
}

QList<std::pair<QString, editor::BookEditor*>> ImageFilterCoordinator::openEditors() const
{
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

EditorPanel* ImageFilterCoordinator::panelOf(const editor::BookEditor& text) const
{
    if (m_centralTabs == nullptr) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel != nullptr && panel->getBookEditor() == &text) {
            return panel;
        }
    }
    return nullptr;
}

bool ImageFilterCoordinator::confirm(int count, const QString& filterName) const
{
    if (m_confirm) {
        return m_confirm(count, filterName);
    }
    //: In Polish: Ten sam filtr dla wszystkich obrazów / Filtr „%1” dostanie %n obraz /
    //: dostaną %n obrazy / dostanie %n obrazów książki. Jasność, kontrast i przezroczystość
    //: każdego obrazu pozostaną bez zmian. / &Zmień
    return dialogs::MessageDialog::confirm(
        m_window, tr("Same Filter for All Images"),
        tr("The filter \"%1\" goes to %n images of the book. The brightness, the contrast and "
           "the transparency of each image stay its own.",
           nullptr, count)
            .arg(filterName),
        tr("&Change"), dialogs::MessageDialog::Kind::Question);
}

int ImageFilterCoordinator::applyToBook(editor::BookEditor& text, int position)
{
    const std::optional<editor::ImageProperties> image =
        text.textDocument() != nullptr ? editor::imageAt(*text.textDocument(), position)
                                       : std::nullopt;
    if (!image || text.isReadOnly()) {
        return -1;
    }
    const editor::ImageFilter filter = image->filter;
    const int threshold = image->threshold;
    const QString name = filterName(filter);

    // The texts: the open ones as their editors have them, the others as kept; a file outside
    // the book alone
    auto& projects = m_book;
    QHash<QString, editor::BookEditor*> open;
    QString textId;
    for (const auto& [elementId, editor] : openEditors()) {
        if (!elementId.isEmpty()) {
            open.insert(elementId, editor);
        }
        if (editor == &text) {
            textId = elementId;
        }
    }
    const bool inBook = projects.isProjectOpen() && !textId.isEmpty() && !projects.isReadOnly();
    // A text not open that changes: its text before and after
    struct Closed {
        QString id;
        QString kml;
        QString filtered;
        int images = 0;
    };
    QList<editor::BookEditor*> editors;
    QList<Closed> closed;
    int count = 0;
    if (inBook) {
        for (const QString& id : projects.textElementIds()) {
            if (editor::BookEditor* editor = open.value(id, nullptr)) {
                if (editor->textDocument() != nullptr && !editor->isReadOnly()) {
                    const auto images = static_cast<int>(
                        editor::imagesWithOtherFilter(*editor->textDocument(), filter, threshold).size());
                    if (images > 0) {
                        editors.append(editor);
                        count += images;
                    }
                }
                continue;
            }
            Closed chapter{id, projects.chapterText(id), QString(), 0};
            chapter.filtered =
                editor::withImageFilterKml(chapter.kml, filter, threshold, &chapter.images);
            if (chapter.images > 0) {
                count += chapter.images;
                closed.append(std::move(chapter));
            }
        }
    } else if (text.textDocument() != nullptr) {
        count = static_cast<int>(
            editor::imagesWithOtherFilter(*text.textDocument(), filter, threshold).size());
        if (count > 0) {
            editors.append(&text);
        }
    }
    if (count == 0) {
        //: In Polish: Wszystkie obrazy mają już filtr „%1”.
        say(tr("Every image already has the filter \"%1\".").arg(name));
        return 0;
    }
    if (!confirm(count, name)) {
        return 0;
    }

    // The change, kept so that it can be undone
    forgetUndo();
    Step step;
    step.filterName = name;
    int changed = 0;
    for (editor::BookEditor* editor : std::as_const(editors)) {
        const int images = editor->setAllImagesFilter(filter, threshold);
        if (images > 0) {
            step.editors.append(editor);
            changed += images;
        }
    }
    const std::vector<QString> dirty = projects.getDirtyElements();
    for (const Closed& chapter : std::as_const(closed)) {
        // Saved at once, unless the text has other changes not saved yet
        const bool clean = std::find(dirty.cbegin(), dirty.cend(), chapter.id) == dirty.cend();
        projects.setChapterContent(chapter.id, chapter.filtered);
        const bool saved = clean && projects.saveChapterContent(chapter.id);
        if (clean && !saved) {
            core::Logger::getInstance().warn(
                "ImageFilterCoordinator: the text of {} with its images filtered could not be "
                "saved",
                chapter.id.toStdString());
        }
        step.savedTexts.append({chapter.id, chapter.kml, saved});
        changed += chapter.images;
    }
    m_undo = step;
    // Changed again or closed, a text cannot go back to the filters before
    for (editor::BookEditor* editor : std::as_const(step.editors)) {
        m_undoWatch.append(connect(editor, &editor::BookEditor::contentChanged, this,
                                   &ImageFilterCoordinator::forgetUndo));
        m_undoWatch.append(
            connect(editor, &QObject::destroyed, this, &ImageFilterCoordinator::forgetUndo));
    }

    //: In Polish: Filtr „%1” ma teraz %n obraz / mają teraz %n obrazy / ma teraz %n obrazów.
    const QString message = tr("%n images have the filter \"%1\" now.", nullptr, changed).arg(name);
    EditorPanel* panel = panelOf(text);
    if (m_showNotice && panel != nullptr) {
        //: In Polish: &Cofnij / Cofnij filtr obrazów
        m_showNotice(panel, message, tr("&Undo"), tr("Undo the filter of the images"),
                     [this]() { undo(); });
    } else {
        say(message);
    }
    return changed;
}

bool ImageFilterCoordinator::canUndo() const
{
    if (!m_undo) {
        return false;
    }
    // Every open text it changed is still open, and no text it changed in its file is open now
    QList<editor::BookEditor*> openNow;
    QSet<QString> openIds;
    for (const auto& [elementId, editor] : openEditors()) {
        openNow.append(editor);
        openIds.insert(elementId);
    }
    const auto& editors = m_undo->editors;
    if (std::any_of(editors.cbegin(), editors.cend(), [&openNow](editor::BookEditor* editor) {
            return !openNow.contains(editor);
        })) {
        return false;
    }
    const auto& saved = m_undo->savedTexts;
    return std::none_of(saved.cbegin(), saved.cend(), [&openIds](const SavedText& text) {
        return openIds.contains(text.elementId);
    });
}

bool ImageFilterCoordinator::undo()
{
    if (!m_undo || !canUndo()) {
        forgetUndo();
        //: In Polish: Tego nie da się już cofnąć: teksty zmieniły się od tamtej pory.
        say(tr("This can no longer be undone: the texts changed since."));
        return false;
    }
    const Step step = *m_undo;
    forgetUndo();
    // Every one still open (canUndo())
    for (editor::BookEditor* editor : step.editors) {
        editor->undo();
    }
    auto& projects = m_book;
    for (const SavedText& chapter : step.savedTexts) {
        projects.setChapterContent(chapter.elementId, chapter.kml);
        if (chapter.saved) {
            projects.saveChapterContent(chapter.elementId);
        }
    }
    //: In Polish: Przywrócono poprzednie filtry obrazów.
    say(tr("The images have their filters back."));
    return true;
}

void ImageFilterCoordinator::forgetUndo()
{
    for (const QMetaObject::Connection& connection : std::as_const(m_undoWatch)) {
        disconnect(connection);
    }
    m_undoWatch.clear();
    m_undo.reset();
}

void ImageFilterCoordinator::say(const QString& message) const
{
    if (m_statusBar != nullptr && !message.isEmpty()) {
        m_statusBar->showMessage(message, MESSAGE_MS);
    }
}

}  // namespace kalahari::gui
