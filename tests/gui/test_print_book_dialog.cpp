// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_print_book_dialog.cpp
/// @brief File > Print: the window of the print, its printers, its pages and sheets, the
///        preview, and the print

#include <catch2/catch_test_macros.hpp>
#include "kalahari_tests/gui/document_window.h"
#include "test_support/dialog_checks.h"
#include "test_support/export_samples.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_pages.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/print_book_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using gui::BookExportCoordinator;
using gui::dialogs::PrintBookDialog;
using test::AppFontOfPixels;
using test::checkFitsSmallScreen;
using test::onItsTab;
using test::Polish;
using Duplex = PrintBookDialog::Duplex;
using editor::SheetLayout;
using editor::SheetSide;

namespace {

/// The settings of a test, as they were before it
class SettingsKept {
public:
    SettingsKept() {
        auto& settings = core::SettingsManager::getInstance();
        for (const char* key : {"print.content", "print.sheet.layout", "export.font",
                                "export.runningHead", "editor.page.size"}) {
            m_strings.emplace_back(key, settings.get<std::string>(key));
        }
        for (const char* key : {"print.collate", "print.sheet.fit", "print.sheet.cropMarks",
                                "export.titles", "export.newPage", "export.pageNumbers"}) {
            m_flags.emplace_back(key, settings.get<bool>(key));
        }
    }
    ~SettingsKept() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            for (const auto& [key, value] : m_strings) {
                settings.set<std::string>(key, value);
            }
            for (const auto& [key, value] : m_flags) {
                settings.set<bool>(key, value);
            }
        } catch (...) {
            std::fputs("SettingsKept: the settings of the test stay\n", stderr);
        }
    }
    SettingsKept(const SettingsKept&) = delete;
    SettingsKept& operator=(const SettingsKept&) = delete;

private:
    std::vector<std::pair<std::string, std::string>> m_strings;
    std::vector<std::pair<std::string, bool>> m_flags;
};

/// A book of the window: a part of two chapters, an epilogue
PrintBookDialog::Book sampleBook() {
    using Row = PrintBookDialog::Row;
    PrintBookDialog::Book book;
    book.title = QStringLiteral("Wind over the Oasis");
    book.rows = {
        Row{QStringLiteral("p"), QStringLiteral("Part One"), QString(), QString(), 0, false},
        Row{QStringLiteral("c1"), QStringLiteral("The Caravan"), QStringLiteral("final"),
            QString(), 1, true},
        Row{QStringLiteral("c2"), QStringLiteral("The Well"), QStringLiteral("draft"), QString(),
            1, true},
        Row{QStringLiteral("e"), QStringLiteral("Epilogue"), QStringLiteral("final"), QString(),
            0, true},
    };
    book.frontTitle = QStringLiteral("The Well");
    book.frontId = QStringLiteral("c2");
    return book;
}

/// A printer of the tests: A4 at first, also A5 and Letter
PrintBookDialog::Printer printerOf(const QString& name, bool isDefault, bool duplex,
                                   bool color) {
    PrintBookDialog::Printer printer;
    printer.name = name;
    printer.isDefault = isDefault;
    printer.duplex = duplex;
    printer.color = color;
    printer.papers = {QPageSize(QPageSize::A5), QPageSize(QPageSize::A4),
                      QPageSize(QPageSize::Letter)};
    printer.paper = QPageSize(QPageSize::A4);
    return printer;
}

/// The printers of the tests: a black and white one that prints on both sides, the default
/// one in color on one side
QList<PrintBookDialog::Printer> samplePrinters() {
    return {printerOf(QStringLiteral("Study"), false, true, false),
            printerOf(QStringLiteral("Office"), true, false, true)};
}

/// The pages of a book and the book they need
struct LaidBook {
    editor::BookExport book;
    std::unique_ptr<editor::BookPages> pages;
};

/// The pages of @p texts texts, a page each; the cursor on page @p current
PrintBookDialog::LaidOut laidOut(int texts, int current) {
    auto laid = std::make_shared<LaidBook>();
    std::vector<editor::ExportItem> items;
    items.reserve(static_cast<size_t>(texts));
    for (int i = 0; i < texts; ++i) {
        items.push_back(test::textItem(QStringLiteral("t%1").arg(i),
                                       QStringLiteral("Chapter %1").arg(i + 1),
                                       test::longKml(2)));
    }
    laid->book = test::bookOf(std::move(items));
    laid->pages = std::make_unique<editor::BookPages>(laid->book,
                                                      editor::BookPages::Options{false});
    laid->pages->layOut();
    PrintBookDialog::LaidOut out;
    out.pages = std::shared_ptr<const editor::BookPages>(laid, laid->pages.get());
    out.current = current;
    return out;
}

/// The pages of five texts, the cursor on the third page; how many times they were laid out
struct PageMaker {
    int calls = 0;
    PrintBookDialog::Layout last;

    PrintBookDialog::PageMaker maker() {
        return [this](const PrintBookDialog::Layout& layout) {
            ++calls;
            last = layout;
            return laidOut(layout.content == PrintBookDialog::Content::Chosen
                               ? static_cast<int>(layout.chosen.size())
                               : 5,
                           2);
        };
    }
};

/// What the print gave the printer: a run of sides for each call
struct Printed {
    std::vector<std::vector<SheetSide>> runs;
    std::vector<PrintBookDialog::Choice> choices;
    QStringList progressTexts;  ///< The sentences under the bar while it printed
    bool fail = false;

    PrintBookDialog::Printing printing(PrintBookDialog* const* dialog) {
        return [this, dialog](const PrintBookDialog::Choice& choice, const editor::BookPages&,
                              const std::vector<SheetSide>& sides,
                              const std::function<bool(int)>& progress, QStringList* problem) {
            runs.push_back(sides);
            choices.push_back(choice);
            for (size_t i = 0; i < sides.size(); ++i) {
                progress(static_cast<int>(i) + 1);
                if (*dialog != nullptr) {
                    progressTexts << (*dialog)->progressText();
                }
            }
            if (fail) {
                *problem << QStringLiteral("QPrinter: the printer Office cannot print");
                return false;
            }
            return true;
        };
    }
};

/// The texts of the items of a combo box
QStringList itemsOf(const QComboBox* box) {
    QStringList items;
    for (int i = 0; i < box->count(); ++i) {
        items << box->itemText(i);
    }
    return items;
}

/// Choose the item of @p box with the data of @p duplex
void chooseDuplex(PrintBookDialog& dialog, Duplex duplex) {
    const int index = dialog.duplexBox()->findData(static_cast<int>(duplex));
    REQUIRE(index >= 0);
    dialog.duplexBox()->setCurrentIndex(index);
}

/// Press @p key in the preview
void press(QWidget* widget, int key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

}  // namespace

TEST_CASE("Print: the tabs, the keys and the size of the window", "[gui][dialogs][print]") {
    SettingsKept kept;
    PrintBookDialog::Book book = sampleBook();
    for (int i = 0; i < 30; ++i) {
        book.rows.append(PrintBookDialog::Row{QStringLiteral("x%1").arg(i),
                                              QStringLiteral("Chapter %1").arg(i + 3),
                                              QStringLiteral("draft"), QString(), 1, true});
    }
    book.frontTitle = QStringLiteral("The Well, where the caravan waited three days for the wind");
    // With the set for a publisher beside the whole book and the chosen texts
    book.setTitle = QStringLiteral("Wind over the Oasis – set for the publisher");
    book.setParts = 4;
    PageMaker pages;

    const auto check = [&]() {
        PrintBookDialog dialog(book, samplePrinters(), pages.maker(), {});
        INFO(test::keyProblems(dialog, *dialog.tabs()).toStdString());
        CHECK(test::keyProblems(dialog, *dialog.tabs()).isEmpty());
        QStringList shown;
        for (int tab = 0; tab < dialog.tabs()->count(); ++tab) {
            shown << dialog.tabs()->tabText(tab);
            dialog.tabs()->setCurrentIndex(tab);
            INFO("The tab " << dialog.tabs()->tabText(tab).toStdString());
            checkFitsSmallScreen(dialog);
        }
        // The longest sentences: the print by hand, the booklet, the chosen texts
        dialog.tabs()->setCurrentIndex(0);
        chooseDuplex(dialog, Duplex::Manual);
        CHECK_FALSE(dialog.duplexNote().isEmpty());
        checkFitsSmallScreen(dialog);
        dialog.printerBox()->setCurrentIndex(1);
        dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
        CHECK_FALSE(dialog.duplexNote().isEmpty());
        checkFitsSmallScreen(dialog);
        dialog.setContent(PrintBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
        return shown.join(QStringLiteral(" | "));
    };

    SECTION("In English") {
        CHECK(check() == QStringLiteral("Pri&nter | Text&s | La&yout | On the Sh&eet"));
    }

    SECTION("In Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        CHECK(check() == QStringLiteral("&Drukarka | T&eksty | U&kład | &Na kartce"));
    }

    SECTION("In Polish, with the font of 13 pixels of Windows and macOS") {
        Polish polish;
        REQUIRE(polish.loaded());
        AppFontOfPixels font(13);
        check();
    }

    // The result of the print shows under the tabs, in the place of the bar
    const auto checkResult = [&]() {
        PrintBookDialog* shown = nullptr;
        Printed printed;
        PrintBookDialog dialog(book, samplePrinters(), pages.maker(), printed.printing(&shown));
        shown = &dialog;
        dialog.tabs()->setCurrentIndex(2);
        dialog.accept();
        REQUIRE(dialog.isDone());
        CHECK_FALSE(dialog.progressBar()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.openLink()->isVisibleTo(&dialog));
        checkFitsSmallScreen(dialog);
    };

    SECTION("With the result of the print, in English") {
        checkResult();
    }

    SECTION("With the result of the print, in Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        checkResult();
    }
}

TEST_CASE("Print: the printers of the system and what each can do", "[gui][dialogs][print]") {
    SettingsKept kept;
    PageMaker pages;

    SECTION("The default printer first, its color, its sides and its paper") {
        PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});
        CHECK(itemsOf(dialog.printerBox()) ==
              QStringList{QStringLiteral("Office (default)"), QStringLiteral("Study")});
        CHECK(dialog.choice().printer == QStringLiteral("Office"));
        CHECK(dialog.printerNote() == QStringLiteral("Ready · color · prints on one side"));
        // Without both sides by the printer: by hand, which the window says how it goes
        CHECK(itemsOf(dialog.duplexBox()) ==
              QStringList{QStringLiteral("No"),
                          QStringLiteral("By hand – Kalahari asks to turn the sheets over")});
        CHECK(dialog.choice().duplex == Duplex::None);
        CHECK(dialog.duplexNote().isEmpty());
        chooseDuplex(dialog, Duplex::Manual);
        CHECK(dialog.duplexNote().startsWith(QStringLiteral("Kalahari prints the fronts")));
        CHECK(itemsOf(dialog.colorBox()) ==
              QStringList{QStringLiteral("In color"), QStringLiteral("In shades of gray")});
        CHECK(dialog.choice().color);
        CHECK(dialog.paperBox()->currentText() == QPageSize(QPageSize::A4).name());
        CHECK(dialog.choice().paper.id() == QPageSize::A4);
        CHECK(dialog.headingDescription() ==
              QStringLiteral("Prints the text \"The Well\" on the printer \"Office\"."));

        // A printer in black and white that prints on both sides, along the long edge at first
        dialog.printerBox()->setCurrentIndex(1);
        CHECK(dialog.printerNote() ==
              QStringLiteral("Ready · black and white · prints on both sides"));
        CHECK(itemsOf(dialog.duplexBox()) ==
              QStringList{QStringLiteral("No"), QStringLiteral("Along the long edge"),
                          QStringLiteral("Along the short edge")});
        CHECK(dialog.choice().duplex == Duplex::LongEdge);
        CHECK(itemsOf(dialog.colorBox()) == QStringList{QStringLiteral("In shades of gray")});
        CHECK_FALSE(dialog.choice().color);
        CHECK(dialog.headingDescription() ==
              QStringLiteral("Prints the text \"The Well\" on the printer \"Study\"."));
        dialog.setContent(PrintBookDialog::Content::Book);
        CHECK(dialog.headingDescription() ==
              QStringLiteral("Prints the book \"Wind over the Oasis\" on the printer \"Study\"."));
    }

    SECTION("A printer with an error says so") {
        QList<PrintBookDialog::Printer> printers = samplePrinters();
        printers[1].state = PrintBookDialog::PrinterState::Error;
        PrintBookDialog dialog(sampleBook(), printers, pages.maker(), {});
        CHECK(dialog.printerNote() == QStringLiteral("Error · color · prints on one side"));
    }

    SECTION("Without a printer the window says so and does not print") {
        PrintBookDialog dialog(sampleBook(), {}, pages.maker(), [](auto&&...) { return true; });
        CHECK(dialog.printerNote() ==
              QStringLiteral("The system has no printer: add one in the settings of the "
                             "system."));
        CHECK(dialog.statusText() == QStringLiteral("There is no printer to print on."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        CHECK(dialog.headingDescription() == QStringLiteral("Prints the text \"The Well\"."));
        // The preview shows the sheets of A4
        CHECK(dialog.shownSide() == 0);
    }
}

TEST_CASE("Print: the pages chosen, the sheets and the preview", "[gui][dialogs][print]") {
    SettingsKept kept;
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(),
                           [](auto&&...) { return true; });
    REQUIRE(pages.calls == 1);
    REQUIRE(dialog.chosenPages() == std::vector<int>{0, 1, 2, 3, 4});

    SECTION("All the pages, a page on each sheet") {
        CHECK(dialog.sheetCount() == 5);
        CHECK(dialog.statusText() ==
              QStringLiteral("5 pages on 5 sheets. The progress and the result will show "
                             "here."));
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 5 · page 1"));
        CHECK(dialog.previewMessage().isEmpty());
        CHECK(dialog.acceptButton()->isEnabled());
        dialog.copiesBox()->setValue(3);
        CHECK(dialog.statusText() ==
              QStringLiteral("5 pages on 5 sheets, 3 copies. The progress and the result will "
                             "show here."));
        CHECK(dialog.collateBox()->isEnabled());
        dialog.copiesBox()->setValue(1);
        CHECK_FALSE(dialog.collateBox()->isEnabled());
    }

    SECTION("The page of the cursor") {
        CHECK(dialog.currentPageButton()->text() == QStringLiteral("C&urrent (3)"));
        CHECK(dialog.currentPageButton()->isEnabled());
        dialog.currentPageButton()->setChecked(true);
        CHECK(dialog.chosenPages() == std::vector<int>{2});
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 1 · page 3"));
        CHECK(dialog.choice().range == PrintBookDialog::PageRange::Current);
    }

    SECTION("A range typed as the pages show their numbers, and the odd or the even pages") {
        dialog.rangeButton()->setChecked(true);
        CHECK(dialog.rangeField()->isEnabled());
        CHECK(dialog.statusText() ==
              QStringLiteral("Nothing to print: type the pages of the range."));
        CHECK(dialog.previewMessage() == QStringLiteral("Type the pages of the range."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        dialog.rangeField()->setText(QStringLiteral("2-4"));
        CHECK(dialog.chosenPages() == std::vector<int>{1, 2, 3});
        CHECK(dialog.acceptButton()->isEnabled());
        dialog.parityBox()->setCurrentIndex(1);  // odd
        CHECK(dialog.chosenPages() == std::vector<int>{2});
        dialog.parityBox()->setCurrentIndex(2);  // even
        CHECK(dialog.chosenPages() == std::vector<int>{1, 3});

        dialog.rangeField()->setText(QStringLiteral("2 to 4"));
        CHECK(dialog.rangeNote().startsWith(QStringLiteral("This is not a range of the pages")));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());

        dialog.rangeField()->setText(QStringLiteral("9-12"));
        CHECK(dialog.rangeNote().isEmpty());
        CHECK(dialog.previewMessage() == QStringLiteral("The range has no page."));
        CHECK(dialog.statusText() ==
              QStringLiteral("Nothing to print: change the range of the pages."));
        CHECK(dialog.shownSide() == -1);
        CHECK(dialog.previewText().isEmpty());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }

    SECTION("Both sides: the fronts and the backs, as the keys of the preview show them") {
        dialog.printerBox()->setCurrentIndex(1);  // both sides, along the long edge
        CHECK(dialog.sheetCount() == 3);
        CHECK(dialog.statusText().startsWith(QStringLiteral("5 pages on 3 sheets.")));
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 3, front · page 1"));
        press(dialog.preview(), Qt::Key_PageDown);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 3, back · page 2"));
        press(dialog.preview(), Qt::Key_End);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 3 of 3, front · page 5"));
        press(dialog.preview(), Qt::Key_PageDown);
        CHECK(dialog.shownSide() == 4);
        press(dialog.preview(), Qt::Key_Home);
        CHECK(dialog.shownSide() == 0);
        press(dialog.preview(), Qt::Key_PageUp);
        CHECK(dialog.shownSide() == 0);
    }

    SECTION("Two pages side by side, and a booklet folded in half") {
        dialog.sheetButton(SheetLayout::TwoPages)->setChecked(true);
        CHECK(dialog.sheetSides() == std::vector<SheetSide>{{0, 1}, {2, 3}, {4, -1}});
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 3 · pages 1 and 2"));
        dialog.showSide(2);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 3 of 3 · page 5"));

        dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
        CHECK(dialog.sheetSides() ==
              std::vector<SheetSide>{{-1, 0}, {1, -1}, {-1, 2}, {3, 4}});
        CHECK(dialog.choice().duplex == Duplex::Manual);
        CHECK(dialog.sheetCount() == 2);
        dialog.showSide(1);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 2, back · page 2"));
        dialog.showSide(5);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 2 of 2, back · pages 4 and 5"));
    }

    SECTION("The preview draws the sheet on the desk") {
        dialog.resize(dialog.sizeHint());
        dialog.preview()->resize(240, 300);
        // The size the layout of the window gives the preview, which differs between the
        // systems: the sheet in its middle, the desk at its corners
        const QImage image = dialog.preview()->grab().toImage();
        INFO("The preview " << image.width() << "x" << image.height());
        REQUIRE(image.width() > 20);
        REQUIRE(image.height() > 20);
        const QColor desk = image.pixelColor(2, 2);
        int paper = 0;
        for (int y = 0; y < image.height(); ++y) {
            paper = std::max(paper, image.pixelColor(image.width() / 2, y).lightness());
        }
        CHECK(paper > desk.lightness());
    }

    SECTION("The texts chosen lay the pages out again, once for many changes") {
        const int before = pages.calls;
        dialog.setContent(PrintBookDialog::Content::Chosen);
        dialog.checkNone();
        CHECK(dialog.statusText() ==
              QStringLiteral("Nothing to print: choose the texts on the tab \"Texts\"."));
        CHECK(dialog.previewMessage() == QStringLiteral("No text is chosen."));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        dialog.checkAll();
        CHECK(pages.calls == before);
        QCoreApplication::processEvents();
        CHECK(pages.calls == before + 1);
        CHECK(pages.last.content == PrintBookDialog::Content::Chosen);
        CHECK(pages.last.chosen.size() == 3);
        CHECK(dialog.chosenPages() == std::vector<int>{0, 1, 2});
    }
}

TEST_CASE("Print: the pages on a sheet and their size", "[gui][dialogs][print]") {
    SettingsKept kept;
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});
    dialog.sheetButton(SheetLayout::OnePage)->setChecked(true);
    dialog.fitButton()->setChecked(true);

    // The size only for one page on a sheet; the crop marks only for a page of its own size
    CHECK(dialog.fitButton()->isEnabled());
    CHECK_FALSE(dialog.cropMarksBox()->isEnabled());
    dialog.actualSizeButton()->setChecked(true);
    CHECK(dialog.cropMarksBox()->isEnabled());
    dialog.cropMarksBox()->setChecked(true);
    CHECK_FALSE(dialog.choice().sheet.fit);
    CHECK(dialog.choice().sheet.cropMarks);
    CHECK(dialog.actualSizeButton()->text().startsWith(QStringLiteral("&Actual size (")));
    dialog.sheetButton(SheetLayout::TwoPages)->setChecked(true);
    CHECK_FALSE(dialog.fitButton()->isEnabled());
    CHECK_FALSE(dialog.cropMarksBox()->isEnabled());
    CHECK(dialog.choice().sheet.fit);
    CHECK_FALSE(dialog.choice().sheet.cropMarks);

    // A booklet prints on both sides, along the short edge; by hand without such a printer.
    // The other sheets as the writer chose.
    dialog.printerBox()->setCurrentIndex(1);
    chooseDuplex(dialog, Duplex::None);
    dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
    CHECK(dialog.choice().duplex == Duplex::ShortEdge);
    CHECK_FALSE(dialog.duplexBox()->isEnabled());
    CHECK(dialog.duplexNote() ==
          QStringLiteral("A booklet prints on both sides, along the short edge."));
    dialog.printerBox()->setCurrentIndex(0);
    CHECK(dialog.choice().duplex == Duplex::Manual);
    CHECK(dialog.duplexNote().startsWith(QStringLiteral("A booklet: the fronts")));
    dialog.sheetButton(SheetLayout::OnePage)->setChecked(true);
    CHECK(dialog.duplexBox()->isEnabled());
    CHECK(dialog.choice().duplex == Duplex::None);
    CHECK(dialog.duplexNote().isEmpty());
}

TEST_CASE("Print: the print, its progress and its result", "[gui][dialogs][print]") {
    SettingsKept kept;
    PageMaker pages;
    PrintBookDialog* shown = nullptr;
    Printed printed;
    int turns = 0;
    bool turn = true;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(),
                           printed.printing(&shown), [&](int sheets) {
                               ++turns;
                               CHECK(sheets == 3);
                               return turn;
                           });
    shown = &dialog;

    SECTION("On one side, the copies whole") {
        dialog.copiesBox()->setValue(2);
        dialog.accept();
        REQUIRE(dialog.isDone());
        REQUIRE(printed.runs.size() == 1);
        CHECK(printed.runs[0] ==
              std::vector<SheetSide>{{0}, {1}, {2}, {3}, {4}, {0}, {1}, {2}, {3}, {4}});
        CHECK(printed.choices[0].printer == QStringLiteral("Office"));
        CHECK(printed.choices[0].copies == 2);
        CHECK(printed.progressTexts.first() ==
              QStringLiteral("Sending to the printer: sheet 2 of 10..."));
        CHECK(printed.progressTexts.last() ==
              QStringLiteral("Sending to the printer: sheet 10 of 10..."));
        CHECK(dialog.resultText() ==
              QStringLiteral("Sent to the printer \"Office\": 5 pages on 5 sheets, 2 copies."));
        CHECK(dialog.progressText().isEmpty());
        CHECK(turns == 0);
    }

    SECTION("On both sides by the printer") {
        dialog.printerBox()->setCurrentIndex(1);
        dialog.accept();
        REQUIRE(printed.runs.size() == 1);
        CHECK(printed.runs[0] == std::vector<SheetSide>{{0}, {1}, {2}, {3}, {4}});
        CHECK(printed.choices[0].duplex == Duplex::LongEdge);
        CHECK(printed.progressTexts.at(1) ==
              QStringLiteral("Sending to the printer: sheet 2 of 3..."));
        CHECK(dialog.resultText() ==
              QStringLiteral("Sent to the printer \"Study\": 5 pages on 3 sheets."));
    }

    SECTION("On both sides by hand: the fronts, the question, the backs") {
        chooseDuplex(dialog, Duplex::Manual);
        dialog.accept();
        REQUIRE(printed.runs.size() == 2);
        CHECK(printed.runs[0] == std::vector<SheetSide>{{0}, {2}, {4}});
        CHECK(printed.runs[1] == std::vector<SheetSide>{{1}, {3}, {}});
        CHECK(turns == 1);
        CHECK(printed.progressTexts.at(3) ==
              QStringLiteral("Sending to the printer: sheet 2 of 3..."));
        CHECK(dialog.resultText() ==
              QStringLiteral("Sent to the printer \"Office\": 5 pages on 3 sheets."));
    }

    SECTION("By hand, without the backs") {
        turn = false;
        chooseDuplex(dialog, Duplex::Manual);
        dialog.accept();
        REQUIRE(printed.runs.size() == 1);
        CHECK(dialog.isDone());
        CHECK(dialog.resultText() ==
              QStringLiteral("Sent to the printer \"Office\" the fronts of 3 sheets. The backs "
                             "were not printed."));
    }

    SECTION("A print that fails says so, and can be tried again") {
        printed.fail = true;
        dialog.accept();
        CHECK_FALSE(dialog.isDone());
        CHECK(dialog.resultText() == QStringLiteral("The print could not be sent to the printer."));
        CHECK(dialog.detailsText().contains(QStringLiteral("cannot print")));
        CHECK(dialog.copyButton()->isVisibleTo(&dialog));
        CHECK(dialog.acceptButton()->isEnabled());
    }
}

TEST_CASE("Print: the window keeps its choices", "[gui][dialogs][print]") {
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("print.content", "front");
    settings.set<std::string>("print.sheet.layout", "onePage");
    settings.set<bool>("print.sheet.fit", true);
    PageMaker pages;
    {
        PrintBookDialog* shown = nullptr;
        Printed printed;
        PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(),
                               printed.printing(&shown), [](int) { return true; });
        shown = &dialog;
        // The first time the text in front, as Ctrl+P in the editors of text
        CHECK(dialog.frontButton()->isChecked());
        CHECK(dialog.choice().content == PrintBookDialog::Content::Front);
        dialog.setContent(PrintBookDialog::Content::Book);
        dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
        dialog.accept();
        REQUIRE(dialog.isDone());
    }
    CHECK(settings.get<std::string>("print.content") == "book");
    CHECK(settings.get<std::string>("print.sheet.layout") == "booklet");
    PrintBookDialog again(sampleBook(), samplePrinters(), pages.maker(), {});
    CHECK(again.bookButton()->isChecked());
    CHECK(again.sheetButton(SheetLayout::Booklet)->isChecked());

    // Without a book, the text in front
    PrintBookDialog::Book noBook;
    noBook.frontTitle = QStringLiteral("Notes");
    PrintBookDialog alone(noBook, samplePrinters(), pages.maker(), {});
    CHECK(alone.frontButton()->isChecked());
    CHECK_FALSE(onItsTab(alone.bookButton()));
}


TEST_CASE("Print: the pages of the book as the program has it, and the page of the cursor",
          "[gui][print]") {
    SettingsKept kept;
    test::OpenNovel novel(QStringLiteral("Wind"));
    test::Window window;
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    gui::EditorPanel* storm = window.editor(window.tabs->currentIndex());
    // An unsaved text of many pages
    QString kml = QStringLiteral("<kml>");
    for (int i = 0; i < 300; ++i) {
        kml += QStringLiteral("<p>The storm of the night, line %1.</p>").arg(i);
    }
    storm->setContent(kml + QStringLiteral("</kml>"));
    BookExportCoordinator coordinator(
        novel.pm, kalahari::test::commands(), window.tabs,
        [&window]() { return qobject_cast<gui::EditorPanel*>(window.tabs->currentWidget()); },
        nullptr, nullptr, &window.window);
    CHECK(coordinator.whyNotPrint().isEmpty());

    PrintBookDialog::Layout layout;
    layout.content = PrintBookDialog::Content::Front;
    SECTION("The text in front: the page of its cursor") {
        storm->getBookEditor()->setCursorPosition({0, 0});
        const PrintBookDialog::LaidOut first = coordinator.printedPages(layout);
        REQUIRE(first.pages != nullptr);
        REQUIRE(first.pages->pageCount() > 3);
        CHECK(first.current == 0);

        storm->getBookEditor()->setCursorPosition({299, 3});
        const PrintBookDialog::LaidOut last = coordinator.printedPages(layout);
        REQUIRE(last.pages != nullptr);
        CHECK(last.current == last.pages->pageCount() - 1);
    }

    SECTION("The whole book: the page of the cursor after the pages before the text") {
        storm->getBookEditor()->setCursorPosition({0, 0});
        layout.content = PrintBookDialog::Content::Book;
        const PrintBookDialog::LaidOut book = coordinator.printedPages(layout);
        REQUIRE(book.pages != nullptr);
        // The title page and the first chapter come before it
        CHECK(book.current > 0);
        CHECK(book.current == book.pages->pageAt(novel.chapterId, 0));
    }

    SECTION("The texts chosen without the text in front: no page of the cursor") {
        layout.content = PrintBookDialog::Content::Chosen;
        const QString other = novel.pm.addElement(
            novel.pm.chapterKindFor(core::BookPlace::Main), QStringLiteral("The Calm"),
            core::BookPlace::Main);
        REQUIRE_FALSE(other.isEmpty());
        layout.chosen = {other};
        const PrintBookDialog::LaidOut chosen = coordinator.printedPages(layout);
        REQUIRE(chosen.pages != nullptr);
        CHECK(chosen.current == -1);
    }

    SECTION("Nothing chosen: no pages") {
        layout.content = PrintBookDialog::Content::Chosen;
        CHECK(coordinator.printedPages(layout).pages == nullptr);
    }
}

TEST_CASE("Print: the sheets on a printer: the paper, its side, both sides, the color",
          "[gui][print]") {
    SettingsKept kept;
    test::OpenNovel novel(QStringLiteral("Wind"));
    QString kml = QStringLiteral("<kml>");
    for (int i = 0; i < 300; ++i) {
        kml += QStringLiteral("<p>The storm of the night, line %1.</p>").arg(i);
    }
    novel.pm.setChapterContent(novel.chapterId, kml + QStringLiteral("</kml>"));
    BookExportCoordinator coordinator(novel.pm, kalahari::test::commands(), nullptr,
                                      []() { return nullptr; }, nullptr, nullptr, nullptr);
    PrintBookDialog::Layout layout;
    const PrintBookDialog::LaidOut laidOut = coordinator.printedPages(layout);
    REQUIRE(laidOut.pages != nullptr);
    REQUIRE(laidOut.pages->pageCount() >= 4);
    CHECK(laidOut.current == -1);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("print.pdf"));
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    PrintBookDialog::Choice choice;
    choice.paper = QPageSize(QPageSize::A4);
    std::vector<int> printed;
    const auto progress = [&printed](int sides) {
        printed.push_back(sides);
        return true;
    };
    QStringList problem;
    // The pages of a PDF and its MediaBoxes
    const auto pdfPages = [&path]() {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QString pdf = QString::fromLatin1(file.readAll());
        QStringList boxes;
        const QRegularExpression box(QStringLiteral("/MediaBox \\[0 0 ([0-9.]+) ([0-9.]+)\\]"));
        for (auto match = box.globalMatch(pdf); match.hasNext();) {
            const QRegularExpressionMatch each = match.next();
            boxes << (each.captured(1).toDouble() > each.captured(2).toDouble()
                          ? QStringLiteral("sideways")
                          : QStringLiteral("upright"));
        }
        return boxes;
    };

    // The PDF writer of the tests keeps no printing on both sides: a printer of the system does
    SECTION("Two pages side by side, both sides by the printer, in shades of gray") {
        choice.sheet.layout = SheetLayout::TwoPages;
        choice.duplex = Duplex::LongEdge;
        choice.color = false;
        const std::vector<SheetSide> sides =
            editor::sheetSides({0, 1, 2, 3}, SheetLayout::TwoPages);
        REQUIRE(sides.size() == 2);
        CHECK(BookExportCoordinator::printOn(printer, choice, *laidOut.pages, sides, progress,
                                             &problem));
        CHECK(problem.isEmpty());
        CHECK(printed == std::vector<int>{1, 2});
        CHECK(printer.fullPage());
        CHECK(printer.pageLayout().orientation() == QPageLayout::Landscape);
        CHECK(printer.pageLayout().pageSize().isEquivalentTo(QPageSize(QPageSize::A4)));
        CHECK(printer.colorMode() == QPrinter::GrayScale);
        CHECK(printer.copyCount() == 1);
        CHECK(pdfPages() == QStringList({QStringLiteral("sideways"), QStringLiteral("sideways")}));
    }

    SECTION("One page on a sheet, by hand, in color: the printer prints one side") {
        choice.duplex = Duplex::Manual;
        const std::vector<SheetSide> fronts = {{0}, {2}, {4}};
        CHECK(BookExportCoordinator::printOn(printer, choice, *laidOut.pages, fronts, progress,
                                             &problem));
        CHECK(printed == std::vector<int>{1, 2, 3});
        CHECK(printer.pageLayout().orientation() == QPageLayout::Portrait);
        CHECK(printer.colorMode() == QPrinter::Color);
        CHECK(pdfPages() == QStringList({QStringLiteral("upright"), QStringLiteral("upright"),
                                         QStringLiteral("upright")}));
    }

    SECTION("A booklet: the sheet sideways") {
        choice.sheet.layout = SheetLayout::Booklet;
        choice.duplex = Duplex::ShortEdge;
        const std::vector<SheetSide> sides =
            editor::sheetSides({0, 1, 2, 3}, SheetLayout::Booklet);
        CHECK(BookExportCoordinator::printOn(printer, choice, *laidOut.pages, sides, progress,
                                             &problem));
        CHECK(printer.pageLayout().orientation() == QPageLayout::Landscape);
    }

    SECTION("The print stopped: what went wrong") {
        const auto stop = [](int) { return false; };
        CHECK_FALSE(BookExportCoordinator::printOn(printer, choice, *laidOut.pages, {{0}, {1}},
                                                   stop, &problem));
        CHECK(problem.size() == 1);
    }
}

TEST_CASE("Print: the printers of the system", "[gui][print]") {
    // The system of the tests may have none; each one has its paper among its papers
    for (const PrintBookDialog::Printer& printer : BookExportCoordinator::systemPrinters()) {
        INFO(printer.name.toStdString());
        CHECK_FALSE(printer.name.isEmpty());
        CHECK(printer.paper.isValid());
        CHECK_FALSE(printer.papers.isEmpty());
        CHECK(printer.margins);
    }
}

TEST_CASE("Print: the command waits for a book or a text", "[gui][print]") {
    BookExportCoordinator coordinator(kalahari::test::book(), kalahari::test::commands(), nullptr,
                                      []() { return nullptr; }, nullptr, nullptr, nullptr);
    CHECK(coordinator.whyNotPrint() == QStringLiteral("Open a book or a text to print"));
}
