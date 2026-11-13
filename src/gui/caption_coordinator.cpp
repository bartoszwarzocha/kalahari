/// @file caption_coordinator.cpp
/// @brief The numbered captions of the book: the numbers of every open text in the book around
///        it, Insert Caption, Insert Cross-Reference and Insert List of Figures

#include "kalahari/gui/caption_coordinator.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/caption_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QPointer>
#include <QTabWidget>
#include <QTextDocument>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// How long after a change of captions the other texts get their numbers (typing in a caption
/// stays quick)
constexpr int REFRESH_DELAY_MS = 300;

/// The commands this coordinator gives callbacks
constexpr const char* CAPTION_COMMAND = "insert.caption";
constexpr const char* REFERENCE_COMMAND = "insert.crossReference";
constexpr const char* FIGURE_LIST_COMMAND = "insert.figureList";

}  // namespace

CaptionCoordinator::CaptionCoordinator(QTabWidget* centralTabs, ChapterOpener openChapter,
                                       QWidget* window, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_openChapter(std::move(openChapter)),
      m_window(window)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(REFRESH_DELAY_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &CaptionCoordinator::refresh);

    // Another book: nothing kept of the texts of the last one
    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::projectOpened, this, [this]() {
        m_savedTexts.clear();
        refreshSoon();
    });
    connect(&projects, &core::ProjectManager::projectClosed, this, [this]() {
        m_savedTexts.clear();
        refreshSoon();
    });
    connect(&projects, &core::ProjectManager::bookCaptionsChanged, this, &CaptionCoordinator::refresh);

    // A text opened, closed or brought to the front: the numbers, and the commands follow it
    if (m_centralTabs != nullptr) {
        const auto textsChanged = [this]() {
            refreshSoon();
            auto& registry = CommandRegistry::getInstance();
            for (const char* id : {CAPTION_COMMAND, REFERENCE_COMMAND, FIGURE_LIST_COMMAND}) {
                registry.updateActionState(id);
            }
        };
        DocumentWindows& windows = DocumentWindows::of(*m_centralTabs);
        connect(&windows, &DocumentWindows::frontChanged, this, textsChanged);
        connect(&windows, &DocumentWindows::windowsChanged, this, textsChanged);
    }

    // The numbering of new books numbers a book without its own, and the files outside books
    const int listener = core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
        if (key.rfind("editor.captions.", 0) == 0) {
            refreshSoon();
        }
    });
    connect(this, &QObject::destroyed, [listener]() {
        core::SettingsManager::getInstance().unsubscribe(listener);
    });
}

void CaptionCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    const auto connectCommand = [this, &registry](const char* id, std::function<void()> execute) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = std::move(execute);
            command->isEnabled = [this]() { return whyNotInsert().isEmpty(); };
            command->disabledReason = [this]() { return whyNotInsert(); };
            registry.updateActionState(id);
        }
    };
    connectCommand(CAPTION_COMMAND, [this]() { insertCaption(); });
    connectCommand(REFERENCE_COMMAND, [this]() { insertCrossReference(); });
    connectCommand(FIGURE_LIST_COMMAND, [this]() { insertFigureList(); });
}

// =============================================================================
// The book
// =============================================================================

core::BookCaptions CaptionCoordinator::captions() const {
    return currentBookCaptions();
}

bool CaptionCoordinator::setCaptions(const core::BookCaptions& captions) {
    auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen()) {
        return false;
    }
    return projects.setBookCaptions(captions);
}

QList<std::pair<QString, editor::BookEditor*>> CaptionCoordinator::openEditors() const {
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

const CaptionCoordinator::CachedText& CaptionCoordinator::savedText(const QString& elementId) const {
    CachedText& cached = m_savedTexts[elementId];
    if (const std::optional<QString> kml = savedTextIfChanged(elementId, cached.signature)) {
        cached.entries = editor::captionEntriesOfKml(*kml);
        cached.references = editor::crossReferenceCountsOfKml(*kml);
    }
    return cached;
}

std::vector<CaptionCoordinator::BookText> CaptionCoordinator::bookTexts() const {
    std::vector<BookText> texts;
    const std::vector<BookTextElement> elements = bookTextElements();
    if (elements.empty()) {
        return texts;
    }

    // The open texts as their editors have them
    QHash<QString, editor::BookEditor*> open;
    for (const auto& [elementId, editor] : openEditors()) {
        if (!elementId.isEmpty()) {
            open.insert(elementId, editor);
        }
    }
    int chapter = 0;
    bool inBack = false;
    for (const auto& [element, place, isChapter] : elements) {
        BookText text;
        text.elementId = element->id;
        text.title = element->title;
        if (place == core::BookPlace::Main) {
            if (isChapter) {
                ++chapter;
                text.chapterStart = true;
            }
            text.chapter = chapter;
        } else if (place == core::BookPlace::Back && !inBack) {
            inBack = true;
            text.backStart = true;
        }
        if (const editor::BookEditor* editor = open.value(element->id, nullptr)) {
            text.entries = editor->captionEntries();
            if (editor->textDocument() != nullptr) {
                text.references = editor::crossReferenceCounts(*editor->textDocument());
            }
        } else {
            const CachedText& saved = savedText(element->id);
            text.entries = saved.entries;
            text.references = saved.references;
        }
        texts.push_back(std::move(text));
    }
    return texts;
}

editor::CaptionContext CaptionCoordinator::contextAt(const std::vector<BookText>& texts,
                                                     int index) const {
    editor::CaptionContext context;
    context.numbering = numberingsOf(captions());
    if (index >= 0 && index < static_cast<int>(texts.size())) {
        context.chapter = texts[static_cast<size_t>(index)].chapter;
    }

    // The numbers of the book in its order: through the book, or anew in each chapter and in
    // the back part
    QHash<QString, int> inBook;
    QHash<QString, int> inScope;
    for (int i = 0; i < static_cast<int>(texts.size()); ++i) {
        const BookText& text = texts[static_cast<size_t>(i)];
        if (text.chapterStart || text.backStart) {
            inScope.clear();
        }
        if (i == index) {
            // Where the text goes on from, for every label the book counts
            QSet<QString> labels(context.numbering.keyBegin(), context.numbering.keyEnd());
            for (auto it = inBook.cbegin(); it != inBook.cend(); ++it) {
                labels.insert(it.key());
            }
            for (const QString& label : labels) {
                context.counted.insert(label, context.numberingOf(label).perChapter
                                                  ? inScope.value(label)
                                                  : inBook.value(label));
            }
        }
        for (const editor::CaptionEntry& entry : text.entries) {
            const int ownInBook = ++inBook[entry.label];
            const int ownInScope = ++inScope[entry.label];
            if (i == index) {
                continue;  // the text numbers its own captions
            }
            const editor::CaptionNumbering numbering = context.numberingOf(entry.label);
            editor::BookCaption caption;
            caption.id = entry.id;
            caption.label = entry.label;
            caption.number = editor::captionNumberText(numbering.perChapter ? ownInScope : ownInBook,
                                                       numbering, text.chapter);
            caption.pieces = entry.pieces;
            caption.elementId = text.elementId;
            (i < index ? context.before : context.after).push_back(std::move(caption));
        }
    }
    return context;
}

editor::CaptionContext CaptionCoordinator::contextFor(const QString& elementId) const {
    const std::vector<BookText> texts = bookTexts();
    const auto found = std::find_if(texts.cbegin(), texts.cend(), [&elementId](const BookText& text) {
        return !elementId.isEmpty() && text.elementId == elementId;
    });
    return contextAt(texts, found != texts.cend() ? static_cast<int>(found - texts.cbegin()) : -1);
}

int CaptionCoordinator::referenceCount(const QStringList& ids) const {
    int count = 0;
    QSet<QString> counted;
    for (const BookText& text : bookTexts()) {
        counted.insert(text.elementId);
        for (const QString& id : ids) {
            count += text.references.value(id);
        }
    }
    // An open text outside the book (a file of the book, a standalone file)
    for (const auto& [elementId, editor] : openEditors()) {
        if ((elementId.isEmpty() || !counted.contains(elementId)) && editor->textDocument() != nullptr) {
            const QHash<QString, int> references = editor::crossReferenceCounts(*editor->textDocument());
            for (const QString& id : ids) {
                count += references.value(id);
            }
        }
    }
    return count;
}

void CaptionCoordinator::refreshSoon() {
    m_refreshTimer.start();
}

void CaptionCoordinator::refresh() {
    m_refreshTimer.stop();
    attachEditors();
    const std::vector<BookText> texts = bookTexts();
    for (const auto& [elementId, editor] : openEditors()) {
        int index = -1;
        for (int i = 0; i < static_cast<int>(texts.size()); ++i) {
            if (!elementId.isEmpty() && texts[static_cast<size_t>(i)].elementId == elementId) {
                index = i;
                break;
            }
        }
        const editor::CaptionContext context = contextAt(texts, index);
        if (editor->captionContext() != context) {
            editor->setCaptionContext(context);
        }
    }
}

void CaptionCoordinator::attachEditors() {
    for (const auto& open : openEditors()) {
        editor::BookEditor* const text = open.second;  // a variable of its own, for the lambdas
        if (m_attached.contains(text)) {
            continue;
        }
        m_attached.append(text);
        connect(text, &editor::BookEditor::captionsChanged, this, [this]() {
            keepCaptionsOnce();
            refreshSoon();
        });
        connect(text, &editor::BookEditor::captionOpenAsked, this, &CaptionCoordinator::openCaption);
        connect(text, &editor::BookEditor::imageCaptionAsked, this, [this, text](int position) {
            editCaption(*text, position);
        });
        connect(text, &QObject::destroyed, this, [this, text]() {
            m_attached.removeAll(text);
            refreshSoon();
        });
        text->setCaptionRemovalConfirmation(
            [this](const QStringList& ids) { return confirmRemoval(ids); });
    }
}

void CaptionCoordinator::keepCaptionsOnce() {
    auto& projects = core::ProjectManager::getInstance();
    if (!projects.isProjectOpen() || projects.isReadOnly() || projects.bookCaptions()) {
        return;
    }
    const bool numbered = std::any_of(m_attached.cbegin(), m_attached.cend(),
                                      [](const editor::BookEditor* editor) {
                                          return !editor->captionEntries().empty();
                                      });
    if (numbered) {
        projects.setBookCaptions(settingsBookCaptions());
    }
}

bool CaptionCoordinator::confirmRemoval(const QStringList& ids) {
    const int references = referenceCount(ids);
    if (references == 0) {
        return true;
    }
    //: The question before an image with its numbered caption goes. In Polish: Usuwanie
    //: podpisu / %n odsyłacz do tego podpisu pokaże / %n odsyłacze… pokażą / %n odsyłaczy…
    //: pokaże „?” zamiast numeru. Usunąć podpis?
    return dialogs::MessageDialog::confirm(
        m_window, tr("Delete Caption"),
        tr("The cross-references to this caption (%n) will show \"?\" in place of its number. "
           "Delete the caption?",
           nullptr, references),
        //: In Polish: &Usuń
        tr("&Delete"), dialogs::MessageDialog::Kind::Warning,
        dialogs::MessageDialog::DefaultButton::Cancel);
}

void CaptionCoordinator::openCaption(const QString& elementId, const QString& captionId) {
    if (!m_openChapter || elementId.isEmpty()) {
        return;
    }
    m_openChapter(elementId);
    refresh();
    for (const auto& [openId, editor] : openEditors()) {
        if (openId == elementId) {
            editor->goToCaption(captionId);
            editor->setFocus();
            return;
        }
    }
}

// =============================================================================
// The commands
// =============================================================================

editor::BookEditor* CaptionCoordinator::editorInFront() const {
    if (m_centralTabs == nullptr) {
        return nullptr;
    }
    auto* panel = qobject_cast<EditorPanel*>(DocumentWindows::of(*m_centralTabs).frontPage());
    return panel != nullptr ? panel->getBookEditor() : nullptr;
}

QString CaptionCoordinator::whyNotInsert() const {
    const editor::BookEditor* editor = editorInFront();
    if (editor == nullptr) {
        //: Why Insert > Caption..., Cross-Reference... and List of Figures are greyed out. In
        //: Polish: Na wierzchu nie ma tekstu
        return tr("No text is in front");
    }
    return editor->isReadOnly() ? readOnlyReason() : QString();
}

void CaptionCoordinator::insertCaption() {
    editor::BookEditor* editor = editorInFront();
    if (editor == nullptr || !whyNotInsert().isEmpty()) {
        return;
    }
    const std::optional<int> image = editor->captionTargetImage();
    if (!image) {
        //: Insert > Caption... without an image at the cursor. In Polish: Wstaw podpis / Zaznacz
        //: obraz albo ustaw kursor tuż za nim – podpis dodaje się do obrazu.
        dialogs::MessageDialog::information(
            m_window, tr("Insert Caption"),
            tr("Select an image, or put the cursor just after it: a caption goes with an image."));
        return;
    }
    editCaption(*editor, *image);
}

bool CaptionCoordinator::editCaption(editor::BookEditor& editor, int position) {
    const std::optional<editor::ImageCaption> current = editor.imageCaptionOf(position);
    if (!current || editor.isReadOnly()) {
        return false;
    }
    refresh();  // the numbers of the book as they are now

    core::BookCaptions bookCaptions = captions();
    dialogs::InsertCaptionDialog::Data data;
    data.labels = bookCaptions.labels;
    data.caption = *current;
    if (data.caption.label.isEmpty()) {
        const QString newImageLabel = settingsNewImageOptions().captionLabel;
        data.caption.label = data.labels.contains(newImageLabel) ? newImageLabel : data.labels.value(0);
    }
    for (const BookText& text : bookTexts()) {
        for (const editor::CaptionEntry& entry : text.entries) {
            data.usedLabels.insert(entry.label);
        }
    }
    for (const editor::CaptionEntry& entry : editor.captionEntries()) {
        data.usedLabels.insert(entry.label);
    }
    QPointer<editor::BookEditor> text(&editor);
    data.numberOf = [text, position](const QString& label) {
        return text ? text->captionNumberAt(position, label) : QString();
    };
    data.numberingOf = [this](const QString& label) {
        return numberingOf(captions().numberingOf(label));
    };
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        data.editNumbering = [this](const QString& label, QWidget* parent) {
            return editNumbering(label, parent);
        };
    }

    dialogs::InsertCaptionDialog dialog(std::move(data), m_window);
    if (dialog.exec() != QDialog::Accepted || !text) {
        return false;
    }

    // The labels the window added or took away, and the book keeps its own from now on
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        core::BookCaptions changed = captions();
        changed.labels = dialog.labels();
        if (changed != bookCaptions || !core::ProjectManager::getInstance().bookCaptions()) {
            setCaptions(changed);
        }
    }
    const bool done = text->setImageCaption(position, dialog.caption());
    refresh();
    return done;
}

bool CaptionCoordinator::editNumbering(const QString& label, QWidget* parent) {
    core::BookCaptions bookCaptions = captions();
    dialogs::CaptionNumberingDialog dialog(label, numberingOf(bookCaptions.numberingOf(label)),
                                           parent != nullptr ? parent : m_window);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const core::CaptionLabelNumbering numbering = bookNumberingOf(label, dialog.numbering());
    bool found = false;
    for (core::CaptionLabelNumbering& own : bookCaptions.numbering) {
        if (own.label == label) {
            own = numbering;
            found = true;
        }
    }
    if (!found) {
        bookCaptions.numbering << numbering;
    }
    if (!core::ProjectManager::getInstance().isProjectOpen()) {
        return false;  // a file outside a book: the Settings number it
    }
    const bool kept = setCaptions(bookCaptions);
    refresh();
    return kept;
}

void CaptionCoordinator::insertCrossReference() {
    editor::BookEditor* editor = editorInFront();
    if (editor == nullptr || !whyNotInsert().isEmpty()) {
        return;
    }
    refresh();
    const core::BookCaptions bookCaptions = captions();
    const editor::CaptionNumbers& numbers = editor->captionNumbers();
    const editor::CursorPosition place = editor->cursorPosition();
    const int cursor = editor->textDocument() != nullptr
                           ? editor->textDocument()->findBlockByNumber(place.paragraph).position() +
                                 place.offset
                           : 0;

    dialogs::InsertCrossReferenceDialog::Data data;
    QHash<QString, QString> titles;
    for (const BookText& text : bookTexts()) {
        titles.insert(text.elementId, text.title);
    }
    for (const editor::BookCaption& caption : numbers.book()) {
        dialogs::InsertCrossReferenceDialog::Target target;
        target.id = caption.id;
        target.label = caption.label;
        target.number = caption.number;
        target.text = numbers.captionText(caption);
        target.where = caption.elementId.isEmpty() ? QString() : titles.value(caption.elementId);
        const std::vector<editor::BookCaption>& before = editor->captionContext().before;
        target.before = caption.elementId.isEmpty()
                            ? caption.position < cursor
                            : std::any_of(before.cbegin(), before.cend(),
                                          [&caption](const editor::BookCaption& earlier) {
                                              return earlier.id == caption.id;
                                          });
        data.targets.push_back(std::move(target));
        if (!data.labels.contains(caption.label)) {
            data.labels << caption.label;
        }
    }
    if (data.targets.empty()) {
        //: Insert > Cross-Reference... in a book without numbered captions. In Polish: Wstaw
        //: odsyłacz / Książka nie ma jeszcze podpisów z numerami. Dodaj podpis do obrazu (Wstaw >
        //: Podpis...), a potem odsyłacz do niego.
        dialogs::MessageDialog::information(
            m_window, tr("Insert Cross-Reference"),
            tr("The book has no numbered captions yet. Add a caption to an image (Insert > "
               "Caption...), then a cross-reference to it."));
        return;
    }
    data.shows = referenceShowsOf(bookCaptions);

    dialogs::InsertCrossReferenceDialog dialog(std::move(data), m_window);
    if (dialog.exec() == QDialog::Accepted) {
        editor->insertCrossReference(dialog.reference());
    }
}

void CaptionCoordinator::insertFigureList() {
    editor::BookEditor* editor = editorInFront();
    if (editor == nullptr || !whyNotInsert().isEmpty()) {
        return;
    }
    refresh();
    //: The heading of a new list of figures in the text. In Polish: Spis ilustracji
    editor->insertFigureList(tr("List of Figures"));
}

}  // namespace kalahari::gui
