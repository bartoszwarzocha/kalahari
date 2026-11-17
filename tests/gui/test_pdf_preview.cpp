// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_pdf_preview.cpp
/// @brief The preview of a PDF file: its pages, its side panel, the search and the selection;
/// a quote, a bookmark and a highlight from it, the PDF file on the card of its material, and
/// how it opens

#include "kalahari_tests/gui/document_window.h"

#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/copy_target.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_quote_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/pdf_pages_view.h"
#include "kalahari/gui/panels/pdf_preview_panel.h"
#include "kalahari/gui/panels/source_boxes.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/gui/widgets/color_menu.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QBuffer>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>

#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "kalahari_tests/test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::PdfFiles;
using gui::PdfPagesView;
using gui::PdfPreviewPanel;

namespace {

/// The words on the second page of pdfFile()
QString proverb() {
    return QStringLiteral("The caravan goes as fast as its slowest camel.");
}

/// A PDF file of @p pages pages of A5 at 72 points to the inch, titled "Atlas of the Deserts":
/// each page says its number, and the second one has proverb() under it
QByteArray pdfFile(int pages = 3) {
    QBuffer buffer;
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QPdfWriter writer(&buffer);
        writer.setTitle(QStringLiteral("Atlas of the Deserts"));
        writer.setPageSize(QPageSize(QPageSize::A5));
        writer.setResolution(72);
        QPainter painter(&writer);
        QFont font(QStringLiteral("DejaVu Sans"));
        font.setPointSize(14);
        painter.setFont(font);
        for (int page = 1; page <= pages; ++page) {
            if (page > 1) {
                writer.newPage();
            }
            painter.drawText(QPointF(40, 80), QStringLiteral("Page %1 of the atlas").arg(page));
            if (page == 2) {
                painter.drawText(QPointF(40, 120), proverb());
            }
        }
    }
    return buffer.data();
}

/// A PDF file of one empty page with a title, two authors and the year 2019 in its data
QByteArray pdfWithData() {
    const QList<QByteArray> objects{
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] >>",
        "<< /Title (Wells on the Salt Roads) /Author (Ewa Zielinska; Jan Nowak) "
        "/CreationDate (D:20190301120000Z) >>"};
    QByteArray pdf = "%PDF-1.4\n";
    QList<qsizetype> offsets;
    for (qsizetype i = 0; i < objects.size(); ++i) {
        offsets << pdf.size();
        pdf += QByteArray::number(i + 1) + " 0 obj\n" + objects.at(i) + "\nendobj\n";
    }
    const qsizetype xref = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const qsizetype offset : offsets) {
        pdf += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) +
           " /Root 1 0 R /Info 4 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    return pdf;
}

/// A publication with file @p data in the Workshop of the open book
QString addPublication(core::ProjectManager& pm, const QString& title, const QByteArray& data,
                       const QString& fileName = QStringLiteral("atlas.pdf")) {
    core::NewMaterial material;
    material.title = title;
    material.fileName = fileName;
    material.data = data;
    const QString id = pm.addMaterial(
        material, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                          QString::fromLatin1(core::MaterialKinds::PUBLICATION)));
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The description of the book of the examples
core::SourceDescription atlas() {
    core::SourceDescription source;
    source.type = QString::fromLatin1(core::SourceDescription::BOOK);
    source.people << core::SourcePerson{
        .name = QStringLiteral("Anna Wiśniewska"), .role = {}, .extra = {}};
    source.setValue(QString::fromLatin1(core::SourceDescription::TITLE),
                    QStringLiteral("Atlas pustyń świata"));
    source.setValue(QString::fromLatin1(core::SourceDescription::YEAR), QStringLiteral("2011"));
    return source;
}

/// Select proverb() on page 2 of pdfFile() as the mouse does from the margin at its left to the
/// margin at its right, in the middle of its line
void selectProverb(PdfPagesView* view) {
    const QRectF text = view->document()->getAllText(1).boundingRectangle();
    REQUIRE_FALSE(text.isEmpty());
    // The proverb is the last line; its letters reach half a line above its bottom
    const double middle = text.bottom() - 6;
    view->select(1, QPointF(text.left() - 30, middle), QPointF(text.right() + 30, middle));
}

/// Events run until @p done, for 5 seconds at most
bool waitFor(const std::function<bool()>& done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

/// The settings of the preview as they are at first, until the test ends
struct UsualPdfSettings {
    std::map<std::string, std::string> saved;
    bool lastPage = true;
    bool highlights = false;

    UsualPdfSettings() {
        auto& settings = core::SettingsManager::getInstance();
        const std::map<std::string, std::string> usual{
            {PdfFiles::OPEN_SETTING, "preview"},     {PdfFiles::LAYOUT_SETTING, "continuous"},
            {PdfFiles::DARK_SETTING, "file"},        {PdfFiles::ZOOM_SETTING, "pageWidth"},
            {PdfFiles::LAST_ZOOM_SETTING, "pageWidth"}, {PdfFiles::SIDE_SETTING, "bookmarks"}};
        for (const auto& [key, value] : usual) {
            saved[key] = settings.get<std::string>(key, value);
            settings.set<std::string>(key, value);
        }
        lastPage = settings.get<bool>(PdfFiles::LAST_PAGE_SETTING, true);
        settings.set<bool>(PdfFiles::LAST_PAGE_SETTING, true);
        highlights = settings.get<bool>(PdfFiles::HIGHLIGHTS_SETTING, false);
        settings.set<bool>(PdfFiles::HIGHLIGHTS_SETTING, false);
    }
    ~UsualPdfSettings() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            for (const auto& [key, value] : saved) {
                settings.set<std::string>(key, value);
            }
            settings.set<bool>(PdfFiles::LAST_PAGE_SETTING, lastPage);
            settings.set<bool>(PdfFiles::HIGHLIGHTS_SETTING, highlights);
        } catch (...) {
            // A destructor may not throw: the later tests then find the settings of this one
            static_cast<void>(
                std::fputs("UsualPdfSettings: the settings of the test stay\n", stderr));
        }
    }
    UsualPdfSettings(const UsualPdfSettings&) = delete;
    UsualPdfSettings& operator=(const UsualPdfSettings&) = delete;
    UsualPdfSettings(UsualPdfSettings&&) = delete;
    UsualPdfSettings& operator=(UsualPdfSettings&&) = delete;
};

/// The programs that open files, in place of the system's until the test ends
struct TestFileOpener {
    QStringList opened;  ///< The files the program asked to open

    TestFileOpener() {
        gui::setFileOpenerForTests([this](const QString& path) {
            opened << path;
            return true;
        });
    }
    ~TestFileOpener() { gui::setFileOpenerForTests({}); }
    TestFileOpener(const TestFileOpener&) = delete;
    TestFileOpener& operator=(const TestFileOpener&) = delete;
    TestFileOpener(TestFileOpener&&) = delete;
    TestFileOpener& operator=(TestFileOpener&&) = delete;
};

/// Once the window of New Quote shows, @p answer answers it
void whenNewQuote(const std::function<void(gui::dialogs::NewQuoteDialog&)>& answer) {
    auto* timer = new QTimer();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, answer]() {
        if (auto* dialog = qobject_cast<gui::dialogs::NewQuoteDialog*>(
                QApplication::activeModalWidget())) {
            timer->stop();
            timer->deleteLater();
            answer(*dialog);
        }
    });
    timer->start();
}

}  // namespace

TEST_CASE("PDF preview: the pages of the file, the page shown and its number",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    const QString id = addPublication(novel.pm, QStringLiteral("Atlas"), pdfFile());

    PdfPreviewPanel preview(novel.pm, id);
    preview.resize(800, 600);
    REQUIRE(preview.hasDocument());
    CHECK(preview.problem().isEmpty());
    CHECK(preview.property("elementId").toString() == id);
    CHECK(preview.canFind());
    PdfPagesView* view = preview.view();
    CHECK(view->pageCount() == 3);
    CHECK(view->currentPage() == 0);
    CHECK(preview.pageEdit()->text() == QStringLiteral("1"));
    CHECK(preview.pageCountLabel()->text().contains(QStringLiteral("3")));
    CHECK(preview.pageStatus() == QStringLiteral("Page 1 of 3"));
    CHECK(preview.pageLabel(1) == QStringLiteral("2"));

    // The file leaves the paper to the program: the pages are white, also the first one alone
    const QImage first =
        PdfFiles::firstPage(novel.pm.filePathOf(*novel.pm.findElement(id)), QSize(100, 100));
    REQUIRE_FALSE(first.isNull());
    CHECK(first.pixelColor(1, 1) == QColor(Qt::white));
    CHECK(view->grab(view->pageRect(0)).toImage().pixelColor(4, 4) == QColor(Qt::white));

    // To a page by its number, also of a range of pages; a page the file has not stays
    int changes = 0;
    QObject::connect(&preview, &gui::PreviewPanel::pageChanged, &preview,
                     [&changes]() { ++changes; });
    CHECK(preview.showPage(QStringLiteral("3")));
    CHECK(view->currentPage() == 2);
    CHECK(preview.pageStatus() == QStringLiteral("Page 3 of 3"));
    CHECK(changes >= 1);
    CHECK(preview.showPage(QStringLiteral("2–3")));
    CHECK(view->currentPage() == 1);
    CHECK_FALSE(preview.showPage(QStringLiteral("12")));
    CHECK(view->currentPage() == 1);

    // The keys of the pages
    view->setFocus();
    QKeyEvent end(QEvent::KeyPress, Qt::Key_End, Qt::NoModifier);
    QCoreApplication::sendEvent(view, &end);
    CHECK(view->currentPage() == 2);
    QKeyEvent home(QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier);
    QCoreApplication::sendEvent(view, &home);
    CHECK(view->currentPage() == 0);

    // The page typed in its field
    preview.pageEdit()->setText(QStringLiteral("2"));
    emit preview.pageEdit()->returnPressed();
    CHECK(view->currentPage() == 1);
}

TEST_CASE("PDF preview: a file it cannot read says why", "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    const QString id =
        addPublication(novel.pm, QStringLiteral("Broken"), QByteArray("%PDF-1.4\n%%EOF\n"));

    PdfPreviewPanel preview(novel.pm, id);
    CHECK_FALSE(preview.hasDocument());
    CHECK_FALSE(preview.problem().isEmpty());
    CHECK_FALSE(preview.canFind());
    CHECK(preview.pageStatus().isEmpty());
    CHECK_FALSE(preview.showPage(QStringLiteral("1")));

    // The card shows no first page of it and says why
    const gui::MaterialFile file = gui::MaterialFile::of(
        novel.pm.filePathOf(*novel.pm.findElement(id)));
    CHECK(file.pdf);
    CHECK_FALSE(file.readable);
    CHECK_FALSE(file.problem().isEmpty());
}

TEST_CASE("PDF preview: the layouts of the pages and the zoom", "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    auto& settings = core::SettingsManager::getInstance();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    const QString id = addPublication(novel.pm, QStringLiteral("Atlas"), pdfFile(4));

    PdfPreviewPanel preview(novel.pm, id);
    preview.resize(800, 600);
    preview.show();
    PdfPagesView* view = preview.view();
    CHECK(view->pageLayout() == PdfPagesView::Layout::Continuous);
    CHECK(view->fit() == PdfPagesView::Fit::PageWidth);
    CHECK(preview.layoutAction(PdfPagesView::Layout::Continuous)->isChecked());

    // Two pages side by side, as in a book: the first alone, then the even ones on the left; the
    // setting follows the buttons
    preview.layoutAction(PdfPagesView::Layout::Two)->trigger();
    CHECK(view->pageLayout() == PdfPagesView::Layout::Two);
    CHECK(settings.get<std::string>(PdfFiles::LAYOUT_SETTING) == "two");
    view->setCurrentPage(1);
    const QRect left = view->pageRect(1);
    const QRect right = view->pageRect(2);
    REQUIRE_FALSE(left.isEmpty());
    REQUIRE_FALSE(right.isEmpty());
    CHECK(left.right() < right.left());
    CHECK(left.top() == right.top());

    // The setting changed elsewhere: the preview follows it
    settings.set<std::string>(PdfFiles::LAYOUT_SETTING, "single");
    CHECK(waitFor([view]() { return view->pageLayout() == PdfPagesView::Layout::Single; }));
    CHECK(preview.layoutAction(PdfPagesView::Layout::Single)->isChecked());

    // The zoom: the steps, 100%, and back to the width of the page
    const double fitted = preview.zoom();
    preview.zoomReset();
    CHECK(preview.zoom() == 1.0);
    CHECK(view->fit() == PdfPagesView::Fit::None);
    preview.zoomIn();
    CHECK(preview.zoom() == 1.25);
    CHECK(preview.zoomBox()->currentText() == QStringLiteral("125%"));
    preview.zoomOut();
    preview.zoomOut();
    CHECK(preview.zoom() == 0.75);
    preview.zoomToPageWidth();
    CHECK(view->fit() == PdfPagesView::Fit::PageWidth);
    CHECK(preview.zoom() == fitted);
    preview.zoomToWholePage();
    CHECK(view->fit() == PdfPagesView::Fit::WholePage);
    CHECK(preview.zoom() < fitted);
}

TEST_CASE("PDF preview: the search finds the hits, goes to them and selects them",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    const QString id = addPublication(novel.pm, QStringLiteral("Atlas"), pdfFile());

    PdfPreviewPanel preview(novel.pm, id);
    preview.resize(800, 600);
    preview.show();
    preview.find();
    CHECK((preview.searchEdit()->hasFocus() || !preview.isActiveWindow()));

    // "Page 1 of the atlas"... on each page, the camel on the second one
    preview.searchEdit()->setText(QStringLiteral("atlas"));
    preview.search(QStringLiteral("atlas"));
    gui::PdfSearchModel* model = preview.searchModel();
    REQUIRE(waitFor([model]() { return model->rowCount(QModelIndex()) == 3; }));
    CHECK(preview.currentHit() == 0);
    CHECK(preview.hitsLabel()->text() == QStringLiteral("1 of 3"));
    CHECK(preview.view()->hasSelection());
    CHECK(preview.view()->selectedText() == QStringLiteral("atlas"));
    CHECK(preview.sidePanel()->currentIndex() == PdfPreviewPanel::ResultsTab);

    preview.nextHit();
    CHECK(preview.currentHit() == 1);
    CHECK(preview.view()->currentPage() == 1);
    CHECK(preview.view()->selectionPage() == 1);
    preview.previousHit();
    preview.previousHit();
    CHECK(preview.currentHit() == 2);
    CHECK(preview.hitsLabel()->text() == QStringLiteral("3 of 3"));

    preview.searchEdit()->setText(QStringLiteral("camel"));
    preview.search(QStringLiteral("camel"));
    REQUIRE(waitFor([model]() { return model->rowCount(QModelIndex()) == 1; }));
    CHECK(preview.view()->currentPage() == 1);
    // The letters of the hit, without the full stop after it
    CHECK(preview.view()->selectedText() == QStringLiteral("camel"));

    preview.searchEdit()->setText(QStringLiteral("oasis"));
    preview.search(QStringLiteral("oasis"));
    waitFor([]() { return false; });
    CHECK(model->rowCount(QModelIndex()) == 0);
    CHECK(preview.currentHit() == -1);
    CHECK(preview.hitsLabel()->text() == QStringLiteral("No hits"));
}

TEST_CASE("PDF preview: the selection is copied, copied with the reference and makes a quote",
          "[gui][materials][pdf][clipboard]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    REQUIRE(pm.setSource(id, atlas()));

    PdfPreviewPanel preview(pm, id);
    preview.resize(800, 600);
    PdfPagesView* view = preview.view();
    CHECK_FALSE(view->canCopyHere());
    for (QAction* action : preview.selectionActions()) {
        INFO(action->text().toStdString());
        CHECK(action->isEnabled() == (action->text().contains(QStringLiteral("Select")) ||
                                      action->text().contains(QStringLiteral("Bookmark"))));
    }

    // The proverb, from the margin at its left to the margin at its right
    selectProverb(view);
    REQUIRE(view->hasSelection());
    CHECK(view->selectionPage() == 1);
    CHECK(view->selectedText().simplified() == proverb());
    CHECK(view->canCopyHere());
    view->copyHere();
    CHECK(QGuiApplication::clipboard()->text().simplified() == proverb());

    // With the reference: the quotation marks of the book's language
    const QString reference = gui::SourceNames::referenceTo(pm, id, QStringLiteral("2"));
    CHECK(reference == QStringLiteral("(Wiśniewska 2011, p. 2)"));
    preview.copyWithReference();
    CHECK(QGuiApplication::clipboard()->text() ==
          QStringLiteral("“") + proverb() + QStringLiteral("” ") + reference);

    // A quote of it, from its page
    QStringList quote;
    QObject::connect(&preview, &gui::PreviewPanel::newQuoteRequested, &preview,
                     [&quote](const QString& source, const QString& text, const QString& page) {
                         quote = {source, text, page};
                     });
    preview.createQuote();
    CHECK(quote == QStringList{id, proverb(), QStringLiteral("2")});

    // Esc takes the selection away
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(view, &escape);
    CHECK_FALSE(view->hasSelection());
    CHECK(view->selectionRect().isEmpty());

    // Select All: the text of the page shown
    view->setCurrentPage(1);
    CHECK(view->canSelectAllHere());
    view->selectAllHere();
    CHECK(view->selectionPage() == 1);
    CHECK(view->selectedText().contains(QStringLiteral("camel")));
}

TEST_CASE("PDF preview: the bookmarks of the material, at the page shown and of the selection",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());

    PdfPreviewPanel preview(pm, id);
    preview.resize(800, 600);
    // No bookmarks yet: the contents of the file, which has none either
    CHECK(preview.bookmarksList()->topLevelItemCount() == 0);
    int changed = 0;
    QObject::connect(&preview, &gui::PreviewPanel::materialChanged, &preview,
                     [&changed, &id](const QString& material) {
                         if (material == id) {
                             ++changed;
                         }
                     });

    // At the page shown, with its note to write
    preview.view()->setCurrentPage(2);
    CHECK(preview.addBookmarkButton()->text() == QStringLiteral("Bookmark at Page 3"));
    preview.addBookmark();
    REQUIRE(pm.findElement(id)->bookmarks.size() == 1);
    CHECK(pm.findElement(id)->bookmarks.first().page == 3);
    CHECK(pm.findElement(id)->bookmarks.first().note.isEmpty());
    CHECK(changed == 1);
    CHECK(preview.sidePanel()->currentIndex() == PdfPreviewPanel::BookmarksTab);
    REQUIRE(preview.bookmarksList()->topLevelItemCount() == 1);
    CHECK(preview.bookmarksList()->topLevelItem(0)->text(0) == QStringLiteral("3"));

    // Of the selection: its words are its note
    selectProverb(preview.view());
    REQUIRE(preview.view()->hasSelection());
    preview.addBookmark();
    REQUIRE(pm.findElement(id)->bookmarks.size() == 2);
    CHECK(pm.findElement(id)->bookmarks.at(1).page == 2);
    CHECK(pm.findElement(id)->bookmarks.at(1).note == proverb());
    CHECK(preview.bookmarksList()->currentItem() == preview.bookmarksList()->topLevelItem(1));

    // The note changed in the list
    preview.bookmarksList()->topLevelItem(0)->setText(1, QStringLiteral("The dunes"));
    CHECK(pm.findElement(id)->bookmarks.first().note == QStringLiteral("The dunes"));

    // Enter on a bookmark goes to its page
    preview.bookmarksList()->setCurrentItem(preview.bookmarksList()->topLevelItem(0));
    emit preview.bookmarksList()->itemActivated(preview.bookmarksList()->topLevelItem(0), 0);
    CHECK(preview.view()->currentPage() == 2);

    // Taken away, after the question for a note
    preview.bookmarksList()->setCurrentItem(preview.bookmarksList()->topLevelItem(1));
    CHECK(test::accepting([&preview]() { preview.removeBookmark(); }) == 1);
    REQUIRE(pm.findElement(id)->bookmarks.size() == 1);
    CHECK(pm.findElement(id)->bookmarks.first().note == QStringLiteral("The dunes"));

    // Changed on the card of the material: the preview shows them anew
    core::PageBookmark other;
    other.page = 1;
    REQUIRE(pm.setBookmarks(id, {pm.findElement(id)->bookmarks.first(), other}));
    preview.refresh();
    CHECK(preview.bookmarksList()->topLevelItemCount() == 2);
}

TEST_CASE("PDF preview: the highlights of the material, in colors and with notes, when the "
          "Settings allow them",
          "[gui][materials][pdf][highlights]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    auto& settings = core::SettingsManager::getInstance();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());

    PdfPreviewPanel preview(pm, id);
    preview.resize(800, 600);
    PdfPagesView* view = preview.view();
    QTreeWidget* list = preview.highlightsList();
    int changed = 0;
    QObject::connect(&preview, &gui::PreviewPanel::materialChanged, &preview,
                     [&changed](const QString&) { ++changed; });

    // Without the option: no tab, no command, nothing highlighted
    CHECK_FALSE(preview.sidePanel()->isTabVisible(PdfPreviewPanel::HighlightsTab));
    CHECK_FALSE(preview.highlightAction()->isVisible());
    selectProverb(view);
    preview.highlight();
    CHECK(pm.findElement(id)->highlights.isEmpty());

    settings.set<bool>(PdfFiles::HIGHLIGHTS_SETTING, true);
    REQUIRE(waitFor([&preview]() {
        return preview.sidePanel()->isTabVisible(PdfPreviewPanel::HighlightsTab);
    }));
    CHECK(preview.highlightAction()->isVisible());
    CHECK(list->topLevelItemCount() == 0);

    // The selection highlighted in the first highlight color: its page, text and places
    selectProverb(view);
    REQUIRE(view->hasSelection());
    CHECK(preview.highlightAction()->isEnabled());
    preview.highlight();
    REQUIRE(pm.findElement(id)->highlights.size() == 1);
    const core::PdfHighlight first = pm.findElement(id)->highlights.first();
    CHECK(first.page == 2);
    CHECK(first.text == proverb());
    CHECK(first.color == preview.newHighlightColors()->lastColor().name().toUpper());
    REQUIRE_FALSE(first.areas.isEmpty());
    CHECK(changed == 1);
    CHECK_FALSE(view->hasSelection());
    REQUIRE(view->highlights().size() == 1);
    CHECK(view->highlights().first().page == 1);
    CHECK(view->highlights().first().color.name().toUpper() == first.color);
    // One band a line
    CHECK(first.areas.size() == 1);
    REQUIRE(list->topLevelItemCount() == 1);
    CHECK(list->topLevelItem(0)->text(0) == QStringLiteral("2"));
    CHECK(list->topLevelItem(0)->text(1) == proverb());
    CHECK(list->currentItem() == list->topLevelItem(0));

    // The same text again in another color: the highlight gets the color
    selectProverb(view);
    preview.highlightIn(QColor(QStringLiteral("#00ff00")));
    REQUIRE(pm.findElement(id)->highlights.size() == 1);
    CHECK(pm.findElement(id)->highlights.first().color == QStringLiteral("#00FF00"));

    // On the page: the highlight under the mouse
    const QRect page = view->pageRect(1);
    REQUIRE_FALSE(page.isEmpty());
    const double scale = page.width() / view->document()->pagePointSize(1).width();
    const QPointF middle = first.areas.first().center() * scale;
    CHECK(view->highlightAt(page.topLeft() + middle.toPoint()) == 0);
    CHECK(view->highlightAt(page.topLeft() + QPoint(2, 2)) == -1);

    // Its note and its color in the side panel; Enter goes to its text
    list->topLevelItem(0)->setText(2, QStringLiteral("Camels"));
    CHECK(pm.findElement(id)->highlights.first().note == QStringLiteral("Camels"));
    list->setCurrentItem(list->topLevelItem(0));
    CHECK(preview.highlightColorButton()->isEnabled());
    preview.setHighlightColor(QColor(QStringLiteral("#ff8800")));
    CHECK(pm.findElement(id)->highlights.first().color == QStringLiteral("#FF8800"));
    view->setCurrentPage(0);
    emit list->itemActivated(list->topLevelItem(0), 0);
    CHECK(view->currentPage() == 1);

    // The option taken back hides the highlights; the book keeps them
    settings.set<bool>(PdfFiles::HIGHLIGHTS_SETTING, false);
    REQUIRE(waitFor([&preview]() {
        return !preview.sidePanel()->isTabVisible(PdfPreviewPanel::HighlightsTab);
    }));
    CHECK(view->highlights().isEmpty());
    CHECK(pm.findElement(id)->highlights.size() == 1);
    settings.set<bool>(PdfFiles::HIGHLIGHTS_SETTING, true);
    REQUIRE(waitFor([view]() { return view->highlights().size() == 1; }));

    // Taken away with Delete, after the question for its note
    list->setCurrentItem(list->topLevelItem(0));
    QStringList said;
    using Answer = gui::dialogs::MessageDialog::Answer;
    QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    CHECK(test::answering([&]() { QCoreApplication::sendEvent(list, &remove); }, Answer::Cancel,
                          said) == 1);
    CHECK(pm.findElement(id)->highlights.size() == 1);
    CHECK(test::accepting([&preview]() { preview.removeHighlight(); }) == 1);
    CHECK(pm.findElement(id)->highlights.isEmpty());
    CHECK(view->highlights().isEmpty());
    CHECK(list->topLevelItemCount() == 0);
}

TEST_CASE("PDF preview: it opens at the page shown last, which it keeps",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    auto& settings = core::SettingsManager::getInstance();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    REQUIRE(pm.setLastPage(id, 3));
    settings.set<std::string>(PdfFiles::ZOOM_SETTING, PdfFiles::LAST);
    {
        PdfPreviewPanel preview(pm, id);
        preview.resize(800, 600);
        CHECK(preview.view()->currentPage() == 2);
        preview.view()->setCurrentPage(1);
        preview.zoomReset();
    }
    // Closed, it kept the page and the zoom
    CHECK(pm.findElement(id)->lastPage == 2);
    CHECK(settings.get<std::string>(PdfFiles::LAST_ZOOM_SETTING) == "1");

    // As the settings say: from the first page, at the zoom shown last
    settings.set<bool>(PdfFiles::LAST_PAGE_SETTING, false);
    settings.set<std::string>(PdfFiles::SIDE_SETTING, PdfFiles::HIDDEN);
    PdfPreviewPanel again(pm, id);
    again.resize(800, 600);
    CHECK(again.view()->currentPage() == 0);
    CHECK(again.zoom() == 1.0);
    CHECK(again.sidePanel()->isHidden());
    CHECK_FALSE(again.sideAction()->isChecked());
}

TEST_CASE("PDF preview: Edit > Copy and Select All work on its pages",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    const QString id = addPublication(novel.pm, QStringLiteral("Atlas"), pdfFile());

    PdfPreviewPanel preview(novel.pm, id);
    preview.resize(800, 600);
    auto* copy = dynamic_cast<gui::CopyTarget*>(preview.view());
    REQUIRE(copy != nullptr);
    int changes = 0;
    const QMetaObject::Connection watch =
        copy->whenSelectionChanges(&preview, [&changes]() { ++changes; });
    preview.view()->selectPage(1);
    CHECK(changes == 1);
    CHECK(copy->canCopyHere());
    preview.view()->clearSelection();
    CHECK(changes == 2);
    CHECK_FALSE(copy->canCopyHere());
    QObject::disconnect(watch);
}

TEST_CASE("New Quote window: the words, the page and what the quote is linked with",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    using gui::dialogs::NewQuoteDialog;
    const NewQuoteDialog dialog(QStringLiteral("  The caravan goes.  "), QStringLiteral("45"),
                          QStringLiteral("Atlas pustyń świata – Wiśniewska, 2011"),
                          QStringLiteral("Atlas"), QStringLiteral("The Storm"), true);
    CHECK(dialog.text() == QStringLiteral("The caravan goes."));
    CHECK(dialog.page() == QStringLiteral("45"));
    CHECK(dialog.link() == NewQuoteDialog::Link::Chapter);
    CHECK(dialog.linkBox()->count() == 3);
    CHECK(dialog.linkBox()->itemText(0) == QStringLiteral("The Storm (open)"));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.insertButton()->isEnabled());
    CHECK(dialog.textEdit()->tabChangesFocus());

    // No words, no quote
    dialog.textEdit()->setPlainText(QStringLiteral("   "));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(dialog.insertButton()->isEnabled());
    dialog.textEdit()->setPlainText(QStringLiteral("The caravan"));
    dialog.pageEdit()->setText(QStringLiteral(" 46 "));
    dialog.linkBox()->setCurrentIndex(1);
    CHECK(dialog.link() == NewQuoteDialog::Link::None);
    CHECK(dialog.page() == QStringLiteral("46"));

    // Create and Insert into Chapter
    CHECK_FALSE(dialog.insertIntoChapter());
    dialog.insertButton()->click();
    CHECK(dialog.result() == QDialog::Accepted);
    CHECK(dialog.insertIntoChapter());

    // Without a chapter open: no link with it, and nowhere to insert
    const NewQuoteDialog alone(QStringLiteral("The caravan"), QString(), QStringLiteral("Atlas"),
                               QStringLiteral("Atlas"), QString(), true);
    CHECK(alone.linkBox()->count() == 2);
    CHECK(alone.link() == NewQuoteDialog::Link::None);
    CHECK_FALSE(alone.insertButton()->isEnabled());
    CHECK(alone.insertButton()->toolTip() == QStringLiteral("No chapter is open"));
}

TEST_CASE("PDF preview: a PDF opens on its preview tab, or in the system program",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    auto& settings = core::SettingsManager::getInstance();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    TestFileOpener opener;

    test::Window window;
    window.navigator.onRequestOpenMaterial(id);
    CHECK(opener.opened.isEmpty());
    auto* preview = qobject_cast<PdfPreviewPanel*>(window.tabs->currentWidget());
    REQUIRE(preview != nullptr);
    CHECK(preview->elementId() == id);
    const int tabs = window.tabs->count();

    // Opened again: its tab; Show Page: at the page
    window.tabs->setCurrentIndex(0);
    window.navigator.onRequestShowPage(id, QStringLiteral("3"));
    CHECK(window.tabs->count() == tabs);
    CHECK(window.tabs->currentWidget() == preview);
    CHECK(preview->view()->currentPage() == 2);
    window.navigator.onRequestShowPage(id, QStringLiteral("30"));
    CHECK(window.statusBar.currentMessage() == QStringLiteral("The file has no page 30"));

    // A bookmark added in the preview shows on the card of the material
    window.navigator.onRequestOpenMaterialCard(id);
    auto* card = qobject_cast<gui::MaterialCardPanel*>(window.tabs->currentWidget());
    REQUIRE(card != nullptr);
    preview->addBookmark();
    CHECK(card->page()->bookmarksBox()->table()->rowCount() == 1);
    // ... and one deleted on the card, in the preview
    REQUIRE(pm.setBookmarks(id, {}));
    window.navigator.onMaterialChanged(id);
    CHECK(preview->bookmarksList()->topLevelItemCount() == 0);

    // As the settings say, the system program
    settings.set<std::string>(PdfFiles::OPEN_SETTING, PdfFiles::SYSTEM);
    window.navigator.onRequestOpenMaterial(id);
    CHECK(opener.opened == QStringList{pm.filePathOf(*pm.findElement(id))});
}

TEST_CASE("PDF preview: a quote from the selection, linked with the chapter and inserted in it",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    REQUIRE(pm.setSource(id, atlas()));

    test::Window window;
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    gui::EditorPanel* chapter = window.editor(window.tabs->currentIndex());
    window.navigator.onRequestPreview(id);
    REQUIRE(qobject_cast<PdfPreviewPanel*>(window.tabs->currentWidget()) != nullptr);

    // Create: the quote, with the words and the page, linked with the chapter
    QString shownChapter;
    whenNewQuote([&shownChapter](gui::dialogs::NewQuoteDialog& dialog) {
        shownChapter = dialog.linkBox()->currentText();
        dialog.acceptButton()->click();
    });
    window.navigator.onRequestQuoteFromPreview(id, proverb(), QStringLiteral("2"));
    CHECK(shownChapter == QStringLiteral("The Storm (open)"));
    QList<const core::ProjectElement*> quotes = pm.quotesOf(id);
    REQUIRE(quotes.size() == 1);
    CHECK(quotes.first()->description == proverb());
    CHECK(quotes.first()->quote.page == QStringLiteral("2"));
    CHECK(quotes.first()->linkTargets() == QStringList{novel.chapterId});
    CHECK(qobject_cast<PdfPreviewPanel*>(window.tabs->currentWidget()) != nullptr);

    // Create and Insert into Chapter, without a link: the chapter comes to the front with it
    whenNewQuote([](gui::dialogs::NewQuoteDialog& dialog) {
        dialog.linkBox()->setCurrentIndex(dialog.linkBox()->findData(
            static_cast<int>(gui::dialogs::NewQuoteDialog::Link::None)));
        dialog.insertButton()->click();
    });
    window.navigator.onRequestQuoteFromPreview(id, proverb(), QStringLiteral("2"));
    quotes = pm.quotesOf(id);
    REQUIRE(quotes.size() == 2);
    CHECK(quotes.last()->linkTargets().isEmpty());
    CHECK(window.tabs->currentWidget() == chapter);
    CHECK(chapter->getBookEditor()->plainText().contains(
        gui::SourceNames::quotedWithReference(pm, id, proverb(), QStringLiteral("2"))));

    // Given up: no quote
    whenNewQuote([](gui::dialogs::NewQuoteDialog& dialog) { dialog.reject(); });
    window.navigator.onRequestQuoteFromPreview(id, proverb(), QStringLiteral("2"));
    CHECK(pm.quotesOf(id).size() == 2);
}

TEST_CASE("PDF file on the card: its first page, its pages, Show Page and Fill In from File",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    const QString path = pm.filePathOf(*pm.findElement(id));

    // What the card says of the file, and its first page as a picture
    const gui::MaterialFile file = gui::MaterialFile::of(path);
    CHECK(file.pdf);
    CHECK(file.readable);
    CHECK(file.format == QStringLiteral("PDF"));
    CHECK(file.pages == 3);
    CHECK(file.summary().contains(QStringLiteral("3 page")));
    CHECK(file.problem().isEmpty());
    const QImage first = gui::readPicture(path, 0, QSize(200, 200));
    REQUIRE_FALSE(first.isNull());
    CHECK(first.height() == 200);
    CHECK(first.width() < first.height());
    const QImage turned = gui::readPicture(path, 90, QSize(200, 200));
    CHECK(turned.width() == 200);
    CHECK(turned.height() < turned.width());

    // Show Page at a bookmark
    core::PageBookmark bookmark;
    bookmark.page = 2;
    REQUIRE(pm.setBookmarks(id, {bookmark}));
    gui::MaterialPage page(pm, nullptr, true);
    page.showMaterial(id);
    gui::BookmarksBox* bookmarks = page.bookmarksBox();
    REQUIRE_FALSE(bookmarks->showButton()->isHidden());
    QStringList shown;
    QObject::connect(&page, &gui::MaterialPage::requestShowPage, &page,
                     [&shown](const QString& material, const QString& at) {
                         shown = {material, at};
                     });
    bookmarks->table()->setCurrentCell(0, 0);
    bookmarks->showButton()->click();
    CHECK(shown == QStringList{id, QStringLiteral("2")});

    // Fill In from File: the title of the file in the empty field
    auto* description = page.findChild<gui::SourceDescriptionBox*>();
    REQUIRE(description != nullptr);
    REQUIRE_FALSE(description->fillButton()->isHidden());
    description->fillFromFile();
    CHECK(pm.findElement(id)->source.value(core::SourceDescription::TITLE) ==
          QStringLiteral("Atlas of the Deserts"));
    CHECK(pm.findElement(id)->source.value(core::SourceDescription::PAGES) ==
          QStringLiteral("3"));
    // Again: nothing more to fill in, and what was typed stays
    description->field(QString::fromLatin1(core::SourceDescription::TITLE))
        ->setText(QStringLiteral("Atlas pustyń świata"));
    emit description->field(QString::fromLatin1(core::SourceDescription::TITLE))
        ->editingFinished();
    CHECK(test::accepting([description]() { description->fillFromFile(); }) == 1);
    CHECK(pm.findElement(id)->source.value(core::SourceDescription::TITLE) ==
          QStringLiteral("Atlas pustyń świata"));

    // The authors and the year a file has
    const QString wells =
        addPublication(pm, QStringLiteral("Wells"), pdfWithData(), QStringLiteral("wells.pdf"));
    const std::optional<gui::PdfMetaData> data =
        PdfFiles::metaData(pm.filePathOf(*pm.findElement(wells)));
    REQUIRE(data.has_value());
    CHECK(data->title == QStringLiteral("Wells on the Salt Roads"));
    CHECK(data->year == QStringLiteral("2019"));
    page.showMaterial(wells);
    description->fillFromFile();
    const core::SourceDescription filled = pm.findElement(wells)->source;
    REQUIRE(filled.people.size() == 2);
    CHECK(filled.people.at(0).name == QStringLiteral("Ewa Zielinska"));
    CHECK(filled.people.at(1).name == QStringLiteral("Jan Nowak"));
    CHECK(filled.value(core::SourceDescription::YEAR) == QStringLiteral("2019"));

    // A picture has none of them
    const QString picture = addPublication(pm, QStringLiteral("Cover"), QByteArray("not a pdf"),
                                           QStringLiteral("cover.png"));
    page.showMaterial(picture);
    CHECK_FALSE(description->fillButton()->isVisibleTo(&page));
    CHECK_FALSE(bookmarks->showButton()->isVisibleTo(&page));
}

TEST_CASE("Quote card: Show Page opens the source at the page of the quote",
          "[gui][materials][pdf]") {
    gui::registerAllIcons();
    const UsualPdfSettings usual;
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addPublication(pm, QStringLiteral("Atlas"), pdfFile());
    REQUIRE(pm.setSource(id, atlas()));
    core::NewMaterial quote;
    quote.title = QStringLiteral("Caravan");
    quote.quote.source = id;
    quote.quote.page = QStringLiteral("2");
    const QString quoteId = pm.addMaterial(
        quote, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                       QString::fromLatin1(core::MaterialKinds::QUOTE)));
    REQUIRE_FALSE(quoteId.isEmpty());

    gui::MaterialPage page(pm, nullptr, true);
    page.showMaterial(quoteId);
    gui::QuoteSourceBox* box = page.quoteBox();
    REQUIRE_FALSE(box->showPageButton()->isHidden());
    CHECK(box->showPageButton()->isEnabled());
    QStringList shown;
    QObject::connect(&page, &gui::MaterialPage::requestShowPage, &page,
                     [&shown](const QString& material, const QString& at) {
                         shown = {material, at};
                     });
    box->showPageButton()->click();
    CHECK(shown == QStringList{id, QStringLiteral("2")});

    // Without a page: nothing to show
    box->pageEdit()->clear();
    emit box->pageEdit()->textEdited(QString());
    CHECK_FALSE(box->showPageButton()->isEnabled());
}
