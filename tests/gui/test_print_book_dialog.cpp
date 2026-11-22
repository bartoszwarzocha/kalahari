// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_print_book_dialog.cpp
/// @brief File > Print: the window of the print, its printers, its pages and sheets, the
///        preview, and the print

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "kalahari_tests/gui/document_window.h"
#include "test_support/dialog_checks.h"
#include "test_support/export_samples.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_pages.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/print_book_dialog.h"
#include "kalahari/gui/widgets/collapsible_section.h"
#include "kalahari/gui/widgets/length_spin_box.h"

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
#include <QScrollBar>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <optional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using gui::BookExportCoordinator;
using gui::SectionHeader;
using gui::dialogs::PrintBookDialog;
using test::AppFontOfPixels;
using test::checkFitsSmallScreen;
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
        for (const char* key : {"print.content", "print.sheet.layout", "print.openSections",
                                "print.previewZoom", "export.font", "export.runningHead",
                                "editor.page.size"}) {
            m_strings.emplace_back(key, settings.get<std::string>(key));
        }
        for (const char* key : {"print.collate", "print.reverse", "print.sheet.fit",
                                "print.sheet.cropMarks", "export.titles", "export.newPage",
                                "export.pageNumbers", "editor.margins.mirrorEnabled"}) {
            m_flags.emplace_back(key, settings.get<bool>(key));
        }
        for (const char* key :
             {"editor.page.customWidth", "editor.page.customHeight", "editor.margins.pageTop",
              "editor.margins.pageBottom", "editor.margins.pageLeft", "editor.margins.pageRight",
              "editor.margins.pageInner", "editor.margins.pageOuter"}) {
            m_lengths.emplace_back(key, settings.get<double>(key));
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
            for (const auto& [key, value] : m_lengths) {
                settings.set<double>(key, value);
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
    std::vector<std::pair<std::string, double>> m_lengths;
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

/// The sections of the window, from the top
std::vector<PrintBookDialog::Section> allSections() {
    using Section = PrintBookDialog::Section;
    return {Section::Printer, Section::Content, Section::Pages,
            Section::Sheet,   Section::BookPage, Section::Look};
}

/// Open all the sections of @p dialog: their titles, from the top
QStringList openSections(PrintBookDialog& dialog) {
    QStringList titles;
    for (const auto which : allSections()) {
        gui::CollapsibleSection* section = dialog.section(which);
        REQUIRE(section != nullptr);
        section->setExpanded(true);
        titles << section->header()->text();
    }
    return titles;
}

/// Press @p key in the preview
void press(QWidget* widget, int key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

}  // namespace

TEST_CASE("Print: the sections, the keys and the size of the window", "[gui][dialogs][print]") {
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
        // As it opens, and with all its sections open: the column of the sections scrolls
        checkFitsSmallScreen(dialog);
        const QString shown = openSections(dialog).join(QStringLiteral(" | "));
        INFO(test::sectionKeyProblems(dialog).toStdString());
        CHECK(test::sectionKeyProblems(dialog).isEmpty());
        checkFitsSmallScreen(dialog);
        // The longest sentences: the print by hand, the booklet, the chosen texts
        chooseDuplex(dialog, Duplex::Manual);
        CHECK_FALSE(dialog.duplexNote().isEmpty());
        checkFitsSmallScreen(dialog);
        dialog.printerBox()->setCurrentIndex(1);
        dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
        CHECK_FALSE(dialog.duplexNote().isEmpty());
        checkFitsSmallScreen(dialog);
        dialog.setContent(PrintBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
        // The column as wide as its widest field: nothing in it scrolls sideways
        dialog.show();
        QCoreApplication::processEvents();
        for (const auto which : allSections()) {
            gui::CollapsibleSection* section = dialog.section(which);
            INFO(section->header()->text().toStdString());
            CHECK(section->body()->minimumSizeHint().width() <= section->width());
        }
        return shown;
    };

    SECTION("In English") {
        CHECK(check() == QStringLiteral("Pri&nter | &What to Print | Pa&ges | &Sheet | Boo&k Page | "
                                        "&Look of the Text"));
    }

    SECTION("In Polish") {
        Polish polish;
        REQUIRE(polish.loaded());
        CHECK(check() == QStringLiteral("&Drukarka | C&o drukować | Stro&ny | &Kartka | "
                                        "Strona ks&iążki | Wyg&ląd tekstu"));
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
        openSections(dialog);
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

TEST_CASE("Print: a series, the list Volume, its keys and the size of the window",
          "[gui][dialogs][print][series]") {
    const SettingsKept kept;
    using Row = PrintBookDialog::Row;
    PrintBookDialog::Book book = sampleBook();
    book.volumes = {{QStringLiteral("v1"), QStringLiteral("Volume 1: Wind over the Oasis")},
                    {QStringLiteral("v2"), QStringLiteral("Volume 2: Dunes")}};
    book.volume = QStringLiteral("v1");
    PageMaker pages;

    // Another volume chosen: its texts go to the pages
    {
        PrintBookDialog dialog(book, samplePrinters(), pages.maker(), {});
        dialog.setVolumeBook([](const QString& volumeId) {
            PrintBookDialog::Book dunes;
            dunes.title = QStringLiteral("Dunes");
            dunes.rows = {Row{QStringLiteral("d1"), QStringLiteral("The Ridge"),
                              QStringLiteral("draft"), QString(), 0, true},
                          Row{QStringLiteral("d2"), QStringLiteral("The Hollow"),
                              QStringLiteral("final"), QString(), 0, true}};
            dunes.volume = volumeId;
            return dunes;
        });
        dialog.setContent(PrintBookDialog::Content::Book);
        QComboBox* volumes = dialog.volumeBox();
        REQUIRE(volumes != nullptr);
        CHECK(volumes->currentText() == QStringLiteral("Volume 1: Wind over the Oasis"));
        CHECK(dialog.layoutChoice().volume == QStringLiteral("v1"));
        volumes->setCurrentIndex(1);
        CHECK(dialog.layoutChoice().volume == QStringLiteral("v2"));
        CHECK(dialog.bookButton()->text() == QStringLiteral("Whole &book (2 texts)"));
        CHECK(dialog.textList()->topLevelItemCount() == 2);
        QCoreApplication::processEvents();  // the pages are laid out again
        CHECK(pages.last.volume == QStringLiteral("v2"));
        INFO(dialog.headingDescription().toStdString());
        CHECK(dialog.headingDescription().contains(QStringLiteral("\"Dunes\"")));
    }

    // The keys of the window and its size with the list
    for (int i = 0; i < 30; ++i) {
        book.rows.append(Row{QStringLiteral("x%1").arg(i), QStringLiteral("Chapter %1").arg(i + 3),
                             QStringLiteral("draft"), QString(), 1, true});
    }
    book.volumes.append({QStringLiteral("v3"),
                         QStringLiteral("Volume 3: The Long Road Through the Great Sand Sea "
                                        "and Back Again")});
    const auto check = [&]() {
        PrintBookDialog dialog(book, samplePrinters(), pages.maker(), {});
        openSections(dialog);
        INFO(test::sectionKeyProblems(dialog).toStdString());
        CHECK(test::sectionKeyProblems(dialog).isEmpty());
        checkFitsSmallScreen(dialog);
        dialog.setContent(PrintBookDialog::Content::Chosen);
        checkFitsSmallScreen(dialog);
    };

    SECTION("In English") {
        check();
    }

    SECTION("In Polish") {
        const Polish polish;
        REQUIRE(polish.loaded());
        check();
    }

    SECTION("In Polish, with the font of 13 pixels of Windows and macOS") {
        const Polish polish;
        REQUIRE(polish.loaded());
        const AppFontOfPixels font(13);
        check();
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

    SECTION("In the reverse order: the last sheet first, a sheet with its front and its back") {
        CHECK_FALSE(dialog.reverseBox()->isChecked());
        dialog.reverseBox()->setChecked(true);
        CHECK(dialog.choice().reverse);
        CHECK(dialog.sheetSides() == std::vector<SheetSide>{{4}, {3}, {2}, {1}, {0}});
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 5 · page 5"));
        CHECK(dialog.statusText().startsWith(QStringLiteral("5 pages on 5 sheets.")));
        dialog.printerBox()->setCurrentIndex(1);  // both sides, along the long edge
        CHECK(dialog.sheetSides() ==
              std::vector<SheetSide>{{4}, {}, {2}, {3}, {0}, {1}});
        CHECK(dialog.sheetCount() == 3);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 3, front · page 5"));
        press(dialog.preview(), Qt::Key_PageDown);
        CHECK(dialog.previewText() == QStringLiteral("Sheet 1 of 3, back · empty"));
        dialog.reverseBox()->setChecked(false);
        CHECK(dialog.sheetSides() == std::vector<SheetSide>{{0}, {1}, {2}, {3}, {4}});
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
              QStringLiteral("Nothing to print: choose the texts in \"What to Print\"."));
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

    SECTION("By hand in the reverse order: the fronts of the last sheet first, then the backs") {
        chooseDuplex(dialog, Duplex::Manual);
        dialog.reverseBox()->setChecked(true);
        dialog.accept();
        REQUIRE(printed.runs.size() == 2);
        CHECK(printed.runs[0] == std::vector<SheetSide>{{4}, {2}, {0}});
        CHECK(printed.runs[1] == std::vector<SheetSide>{{}, {3}, {1}});
        CHECK(printed.choices[0].reverse);
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
    settings.set<bool>("print.reverse", false);
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
        dialog.reverseBox()->setChecked(true);
        dialog.accept();
        REQUIRE(dialog.isDone());
    }
    CHECK(settings.get<std::string>("print.content") == "book");
    CHECK(settings.get<std::string>("print.sheet.layout") == "booklet");
    CHECK(settings.get<bool>("print.reverse"));
    PrintBookDialog again(sampleBook(), samplePrinters(), pages.maker(), {});
    CHECK(again.bookButton()->isChecked());
    CHECK(again.sheetButton(SheetLayout::Booklet)->isChecked());
    CHECK(again.reverseBox()->isChecked());

    // Without a book, the text in front
    PrintBookDialog::Book noBook;
    noBook.frontTitle = QStringLiteral("Notes");
    PrintBookDialog alone(noBook, samplePrinters(), pages.maker(), {});
    CHECK(alone.frontButton()->isChecked());
    CHECK(alone.bookButton()->isHidden());
    CHECK(alone.chooseTextsButton()->isHidden());
}


TEST_CASE("Print: the sections, what they say while closed, and the keys that open them",
          "[gui][dialogs][print]") {
    using Section = PrintBookDialog::Section;
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("print.openSections", "printer,content,pages");
    settings.set<std::string>("editor.page.size", "A4");
    settings.set<bool>("editor.margins.mirrorEnabled", false);
    for (const char* key : {"editor.margins.pageTop", "editor.margins.pageBottom",
                            "editor.margins.pageLeft", "editor.margins.pageRight"}) {
        settings.set<double>(key, 20.0);
    }
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});

    // The printer, the texts and the pages open at first; the sheet, the page and the look
    // of the text say what they hold
    CHECK(dialog.section(Section::Printer)->isExpanded());
    CHECK(dialog.section(Section::Content)->isExpanded());
    CHECK(dialog.section(Section::Pages)->isExpanded());
    CHECK_FALSE(dialog.section(Section::Sheet)->isExpanded());
    CHECK_FALSE(dialog.section(Section::BookPage)->isExpanded());
    CHECK_FALSE(dialog.section(Section::Look)->isExpanded());
    CHECK(dialog.section(Section::Sheet)->summary() == QStringLiteral("A4 · one page · fitted"));
    const QString margins = dialog.section(Section::BookPage)->summary();
    INFO(margins.toStdString());
    CHECK(margins.startsWith(QStringLiteral("A4 · margins ")));
    CHECK_FALSE(margins.contains(QLatin1Char('/')));
    CHECK_FALSE(dialog.section(Section::Look)->summary().isEmpty());
    // The summary is the description of the header for screen readers
    CHECK(dialog.section(Section::Sheet)->header()->accessibleDescription() ==
          dialog.section(Section::Sheet)->summary());

    SECTION("The summaries follow the fields") {
        CHECK(dialog.section(Section::Printer)->summary() == QStringLiteral("Office"));
        dialog.copiesBox()->setValue(3);
        CHECK(dialog.section(Section::Printer)->summary().startsWith(QStringLiteral("Office · 3")));
        CHECK(dialog.section(Section::Content)->summary() == QStringLiteral("Text in front"));
        dialog.setContent(PrintBookDialog::Content::Book);
        CHECK(dialog.section(Section::Content)->summary() == QStringLiteral("Whole book"));
        CHECK(dialog.section(Section::Pages)->summary() == QStringLiteral("All"));
        dialog.rangeButton()->setChecked(true);
        dialog.rangeField()->setText(QStringLiteral("2-4"));
        dialog.parityBox()->setCurrentIndex(1);  // odd
        dialog.reverseBox()->setChecked(true);
        CHECK(dialog.section(Section::Pages)->summary() ==
              QStringLiteral("Range: 2-4 · odd only · from the end"));
        dialog.sheetButton(SheetLayout::TwoPages)->setChecked(true);
        dialog.printerBox()->setCurrentIndex(1);  // both sides
        CHECK(dialog.section(Section::Sheet)->summary() ==
              QStringLiteral("A4 · two pages side by side · two-sided"));
        dialog.marginBox(PrintBookDialog::Margin::Left)->setStoredValue(25.0);
        QCoreApplication::processEvents();  // the page follows its settings
        QCoreApplication::processEvents();  // and the pages are laid out again
        CHECK(dialog.section(Section::BookPage)->summary().count(QLatin1Char('/')) == 3);
    }

    SECTION("A click, Enter and Space open and close a section") {
        SectionHeader* header = dialog.section(Section::Sheet)->header();
        header->click();
        CHECK(dialog.section(Section::Sheet)->isExpanded());
        CHECK(dialog.section(Section::Sheet)->body()->isVisibleTo(&dialog));
        press(header, Qt::Key_Return);
        CHECK_FALSE(dialog.section(Section::Sheet)->isExpanded());
        CHECK_FALSE(dialog.section(Section::Sheet)->body()->isVisibleTo(&dialog));
    }

    SECTION("Alt and the key of the title open a section and go to its first field") {
        gui::CollapsibleSection* sheet = dialog.section(Section::Sheet);
        CHECK(sheet->firstField() == dialog.paperBox());
        sheet->openAndFocus();
        CHECK(sheet->isExpanded());
        // Of radio buttons that go together, the one checked
        CHECK(dialog.section(Section::Pages)->firstField() == dialog.allPagesButton());
        dialog.rangeButton()->setChecked(true);
        CHECK(dialog.section(Section::Pages)->firstField() == dialog.rangeButton());
        CHECK(dialog.section(Section::BookPage)->firstField() == dialog.pageSizeBox());
    }

    SECTION("The window remembers the sections open") {
        dialog.section(Section::Printer)->setExpanded(false);
        dialog.section(Section::BookPage)->setExpanded(true);
        CHECK(settings.get<std::string>("print.openSections") == "content,pages,bookPage");
        PrintBookDialog again(sampleBook(), samplePrinters(), pages.maker(), {});
        CHECK_FALSE(again.section(Section::Printer)->isExpanded());
        CHECK(again.section(Section::BookPage)->isExpanded());
    }
}

TEST_CASE("Print: the texts chosen in a window of their own", "[gui][dialogs][print]") {
    SettingsKept kept;
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});
    REQUIRE(dialog.chooseTextsButton() != nullptr);
    REQUIRE(dialog.chosenTextsWindow() != nullptr);
    CHECK(dialog.chooseTextsButton()->isVisibleTo(&dialog));
    // The list is in its window, not in the section
    CHECK_FALSE(dialog.textList()->isVisibleTo(&dialog));
    dialog.setContent(PrintBookDialog::Content::Book);

    SECTION("Cancelled: the texts stay as they were") {
        const QStringList before = dialog.layoutChoice().chosen;
        QTimer::singleShot(0, &dialog, [&dialog]() {
            dialog.checkNone();
            dialog.chosenTextsWindow()->reject();
        });
        dialog.chooseTextsButton()->click();
        CHECK(dialog.bookButton()->isChecked());
        CHECK(dialog.layoutChoice().chosen == before);
    }

    SECTION("Chosen: the texts chosen print") {
        QString heading;
        QTimer::singleShot(0, &dialog, [&dialog, &heading]() {
            heading = dialog.chosenTextsWindow()->windowTitle();
            dialog.checkNone();
            dialog.textList()->topLevelItem(0)->child(0)->setCheckState(0, Qt::Checked);
            dialog.chosenTextsWindow()->accept();
        });
        dialog.chooseTextsButton()->click();
        CHECK(dialog.chosenButton()->isChecked());
        CHECK(dialog.layoutChoice().content == PrintBookDialog::Content::Chosen);
        CHECK(dialog.layoutChoice().chosen == QStringList{QStringLiteral("c1")});
        QCoreApplication::processEvents();
        CHECK(pages.last.chosen == QStringList{QStringLiteral("c1")});
    }
}

TEST_CASE("Print: the paper sources, the qualities and the window of the driver",
          "[gui][dialogs][print]") {
    using DriverChoice = PrintBookDialog::DriverChoice;
    SettingsKept kept;
    PageMaker pages;
    QList<PrintBookDialog::Printer> printers = samplePrinters();
    // The default printer "Office" lists its trays and its qualities
    printers[1].sources = {{7, QStringLiteral("Automatically Select")},
                           {1, QStringLiteral("Tray 1")},
                           {2, QStringLiteral("Tray 2")}};
    printers[1].source = 2;
    printers[1].qualities = {QSize(300, 300), QSize(600, 600), QSize(1200, 600)};
    printers[1].quality = QSize(600, 600);
    PrintBookDialog dialog(sampleBook(), printers, pages.maker(), {});

    CHECK(itemsOf(dialog.sourceBox()) ==
          QStringList{QStringLiteral("Automatically Select"), QStringLiteral("Tray 1"),
                      QStringLiteral("Tray 2")});
    CHECK(dialog.choice().source == 2);
    CHECK(itemsOf(dialog.qualityBox()) ==
          QStringList{QStringLiteral("As the printer sets it"), QStringLiteral("300 dpi"),
                      QStringLiteral("600 dpi"), QStringLiteral("1200 x 600 dpi")});
    CHECK(dialog.choice().quality == QSize(600, 600));
    dialog.qualityBox()->setCurrentIndex(0);
    CHECK_FALSE(dialog.choice().quality.isValid());
    // A printer that lists none: the rows go
    dialog.printerBox()->setCurrentIndex(1);
    CHECK(dialog.sourceBox()->parentWidget()->isHidden());
    CHECK(dialog.qualityBox()->parentWidget()->isHidden());
    CHECK(dialog.choice().source == -1);
    CHECK_FALSE(dialog.choice().quality.isValid());
    dialog.printerBox()->setCurrentIndex(0);
    CHECK_FALSE(dialog.sourceBox()->parentWidget()->isHidden());

    // Without the window of the driver (a system without one) the button waits
    CHECK_FALSE(dialog.driverButton()->isEnabled());

    PrintBookDialog::Choice shown;
    std::optional<DriverChoice> answer;
    dialog.setDriverWindow([&shown, &answer](const PrintBookDialog::Choice& choice, QWidget*) {
        shown = choice;
        return answer;
    });
    REQUIRE(dialog.driverButton()->isEnabled());

    SECTION("Its choices go to the fields of the window and to the print") {
        dialog.copiesBox()->setValue(2);
        DriverChoice chosen;
        chosen.settings = QByteArrayLiteral("the settings of the driver");
        chosen.paper = QPageSize(QPageSize::A5);
        chosen.copies = 3;
        chosen.collate = false;
        chosen.color = false;
        chosen.source = 1;
        chosen.quality = QSize(300, 300);
        answer = chosen;
        dialog.driverButton()->click();
        // It showed the choices of the window
        CHECK(shown.printer == QStringLiteral("Office"));
        CHECK(shown.copies == 2);
        CHECK(shown.source == 2);
        const PrintBookDialog::Choice now = dialog.choice();
        CHECK(now.paper.id() == QPageSize::A5);
        CHECK(now.copies == 3);
        CHECK_FALSE(now.collate);
        CHECK_FALSE(now.color);
        CHECK(now.source == 1);
        CHECK(now.quality == QSize(300, 300));
        CHECK(now.driverSettings == QByteArrayLiteral("the settings of the driver"));
        // Another printer: the settings of the driver go
        dialog.printerBox()->setCurrentIndex(1);
        CHECK(dialog.choice().driverSettings.isEmpty());
    }

    SECTION("Both sides as the driver chose, unless the sheets are a booklet") {
        dialog.printerBox()->setCurrentIndex(1);  // "Study" prints on both sides
        DriverChoice chosen;
        chosen.duplex = Duplex::ShortEdge;
        answer = chosen;
        dialog.driverButton()->click();
        CHECK(dialog.choice().duplex == Duplex::ShortEdge);
        dialog.sheetButton(SheetLayout::Booklet)->setChecked(true);
        chosen.duplex = Duplex::None;
        answer = chosen;
        dialog.driverButton()->click();
        CHECK(dialog.choice().duplex == Duplex::ShortEdge);
    }

    SECTION("Cancelled: nothing changes") {
        answer.reset();
        const PrintBookDialog::Choice before = dialog.choice();
        dialog.driverButton()->click();
        CHECK(dialog.choice().paper.isEquivalentTo(before.paper));
        CHECK(dialog.choice().source == before.source);
        CHECK(dialog.choice().driverSettings.isEmpty());
    }
}

TEST_CASE("Print: the page of the book is the page of Settings", "[gui][dialogs][print]") {
    using Margin = PrintBookDialog::Margin;
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("print.openSections", "bookPage");
    settings.set<std::string>("editor.page.size", "A5");
    settings.set<bool>("editor.margins.mirrorEnabled", false);
    settings.set<double>("editor.margins.pageTop", 20.0);
    settings.set<double>("editor.margins.pageBottom", 22.0);
    settings.set<double>("editor.margins.pageLeft", 25.0);
    settings.set<double>("editor.margins.pageRight", 15.0);
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});
    const auto stored = [&dialog](Margin margin) {
        return dialog.marginBox(margin)->storedValue();
    };

    CHECK(dialog.pageSizeBox()->currentData().toString() == QStringLiteral("A5"));
    CHECK(dialog.pageWidthBox()->isHidden());
    CHECK(dialog.pageHeightBox()->isHidden());
    CHECK(stored(Margin::Top) == Catch::Approx(20.0));
    CHECK(stored(Margin::Bottom) == Catch::Approx(22.0));
    CHECK(stored(Margin::Left) == Catch::Approx(25.0));
    CHECK(stored(Margin::Right) == Catch::Approx(15.0));
    CHECK_FALSE(dialog.marginBox(Margin::Left)->isHidden());
    CHECK(dialog.marginBox(Margin::Inner)->isHidden());
    CHECK(dialog.actualSizeButton()->text() == QStringLiteral("&Actual size (A5)"));

    // The fields write the settings, and the pages are laid out again
    const int calls = pages.calls;
    dialog.marginBox(Margin::Top)->setStoredValue(30.0);
    CHECK(settings.get<double>("editor.margins.pageTop") == Catch::Approx(30.0));
    QCoreApplication::processEvents();  // the page follows its settings
    QCoreApplication::processEvents();  // and the pages are laid out again
    CHECK(pages.calls > calls);

    // Mirror margins: the inner one and the outer one instead of the left and the right one
    dialog.mirrorBox()->setChecked(true);
    CHECK(settings.get<bool>("editor.margins.mirrorEnabled"));
    CHECK(dialog.marginBox(Margin::Left)->isHidden());
    CHECK_FALSE(dialog.marginBox(Margin::Inner)->isHidden());
    CHECK_FALSE(dialog.marginBox(Margin::Outer)->isHidden());

    // A size of its own: its width and its height
    dialog.pageSizeBox()->setCurrentIndex(dialog.pageSizeBox()->findData(QStringLiteral("Custom")));
    CHECK(settings.get<std::string>("editor.page.size") == "Custom");
    CHECK_FALSE(dialog.pageWidthBox()->isHidden());
    dialog.pageWidthBox()->setStoredValue(170.0);
    CHECK(settings.get<double>("editor.page.customWidth") == Catch::Approx(170.0));

    // A change in Settings shows in the window
    settings.set<double>("editor.margins.pageBottom", 12.0);
    settings.set<std::string>("editor.page.size", "B5");
    QCoreApplication::processEvents();
    CHECK(stored(Margin::Bottom) == Catch::Approx(12.0));
    CHECK(dialog.pageSizeBox()->currentData().toString() == QStringLiteral("B5"));
    CHECK(dialog.pageWidthBox()->isHidden());
}

TEST_CASE("Print: the preview of the whole sheet and of its width", "[gui][dialogs][print]") {
    using Zoom = PrintBookDialog::Zoom;
    SettingsKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("print.previewZoom", "wholeSheet");
    PageMaker pages;
    PrintBookDialog dialog(sampleBook(), samplePrinters(), pages.maker(), {});
    dialog.show();
    QCoreApplication::processEvents();
    QScrollBar* bar = dialog.previewScrollBar();
    REQUIRE(bar != nullptr);

    CHECK(dialog.zoom() == Zoom::WholeSheet);
    CHECK(dialog.zoomButton(Zoom::WholeSheet)->isChecked());
    CHECK_FALSE(bar->isVisibleTo(&dialog));

    // As wide as the preview: the sheet scrolls, with its bar and with the keys
    dialog.zoomButton(Zoom::Width)->click();
    QCoreApplication::processEvents();
    CHECK(dialog.zoom() == Zoom::Width);
    CHECK(settings.get<std::string>("print.previewZoom") == "width");
    CHECK(bar->isVisibleTo(&dialog));
    REQUIRE(bar->maximum() > 0);
    press(dialog.preview(), Qt::Key_Down);
    CHECK(bar->value() > 0);
    press(dialog.preview(), Qt::Key_Up);
    CHECK(bar->value() == 0);
    bar->setValue(bar->maximum());
    // Another sheet shows from its top
    press(dialog.preview(), Qt::Key_PageDown);
    CHECK(dialog.shownSide() == 1);
    CHECK(bar->value() == 0);

    // The window remembers it
    PrintBookDialog again(sampleBook(), samplePrinters(), pages.maker(), {});
    CHECK(again.zoom() == Zoom::Width);
    again.setZoom(Zoom::WholeSheet);
    CHECK(settings.get<std::string>("print.previewZoom") == "wholeSheet");
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
