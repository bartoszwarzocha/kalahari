/// @file file_preview_dialog.cpp
/// @brief Preview of a chapter file that no element of the book has: its text, read-only,
/// and what can be done with the file

#include "kalahari/gui/dialogs/file_preview_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QStringList>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int PREVIEW_WIDTH = 640;   ///< Width of the window where the screen allows
constexpr int PREVIEW_HEIGHT = 560;  ///< Height of the window where the screen allows
constexpr double SCREEN_PART = 0.9;  ///< The part of the screen the window takes at most

} // anonymous namespace

FilePreviewDialog::FilePreviewDialog(const core::ChapterDocument& chapter, const QString& path,
                                     const QString& projectFolder, QWidget* parent)
    : KalahariDialog(parent)
{
    // The heading: the title of the text, and the file with what the element would take
    // from it
    QStringList facts;
    facts << QDir::toNativeSeparators(QDir(projectFolder).relativeFilePath(path));
    if (core::ProjectManager::isTextStatus(chapter.status())) {
        //: %1: the status of the text in the file, e.g. Draft
        facts << tr("Status: %1").arg(AddToBookDialog::statusName(chapter.status()));
    }
    //: %1: the number of words of the text in the file
    facts << tr("Words: %1").arg(QLocale().toString(chapter.wordCount()));
    const QString title =
        chapter.title().trimmed().isEmpty() ? QFileInfo(path).fileName() : chapter.title();
    setHeading(title, facts.join(QStringLiteral(" · ")));
    setWindowTitle(tr("Preview"));
    setHeadingIcon(QStringLiteral("common.preview"));
    setCompactHeading(true);

    // The text, as the editor would show its paragraphs
    m_text = new QTextBrowser(this);
    m_text->setOpenLinks(false);
    m_text->setHtml(editor::ClipboardHandler::kmlToHtml(chapter.kml()));
    m_text->setPlaceholderText(tr("The file has no text."));
    m_text->setAccessibleName(tr("Text of the file"));
    contentLayout()->addWidget(m_text, 1);

    // Close is the default; the other buttons lead on to the next window
    setAcceptText(tr("&Add to Book..."));
    cancelButton()->setText(tr("Close"));
    m_deleteButton = addSideButton(tr("&Delete File..."));
    connect(acceptButton(), &QPushButton::clicked, this,
            [this]() { m_choice = Choice::AddToBook; });
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::Delete;
        accept();
    });
    acceptButton()->setDefault(false);
    cancelButton()->setDefault(true);
    cancelButton()->setFocus();

    // Large enough to read, within the screen
    QSize size(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    if (const QScreen* screen = this->screen()) {
        size = size.boundedTo(screen->availableGeometry().size() * SCREEN_PART);
    }
    resize(size);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
