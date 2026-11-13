/// @file test_editor_stage0_benchmark.cpp
/// @brief Editor benchmark on a ~150k-word document (written in Stage 0, kept up to date)
///
/// Hidden from the normal suite (tag [.]). Run explicitly:
///   build-windows\bin\kalahari-tests.exe "[benchmark][stage0]"
///
/// Results are printed (Catch2 WARN) as a Markdown table and also written to
/// stage0_benchmark_results.md in the current directory (override with the
/// KALAHARI_BENCH_OUT environment variable).
///
/// The copy step builds the clipboard data without putting it on the system clipboard,
/// so a run leaves the user's clipboard alone.
///
/// Every timing goes through the real BookEditor entry points. Painting is measured with
/// QWidget::render() into a QPixmap, which calls BookEditor::paintEvent() without showing
/// a window. Since Stage 2 a load, a width or a font change leaves the paragraphs waiting
/// for layout: the paint lays out the ones it shows, a background pass the others (here
/// run at once with layoutPendingBlocks(), as there is no event loop).
///
/// The document has headings, dialog in the style Dialogue and scene breaks, and the editor
/// the paragraph styles of the base book type, as a chapter of a book in the program.
///
/// The benchmark of tables ("[benchmark][tables]", results in tables_benchmark_results.md)
/// takes the same chapter with a table every 200 paragraphs and a long table in the middle:
/// loading, the pages (the header rows repeated, a row higher than a page split), typing in a
/// cell, and the commands of the Table menu.
///
/// The benchmark of notes ("[benchmark][footnotes]", results in footnotes_benchmark_results.md)
/// takes the chapter with a footnote at the end of every 5th paragraph and an endnote of every
/// 50th: loading, the footnotes under their paragraphs and at the bottom of the pages, typing
/// before a note, a new note at the start (all the numbers after it change), copying all of it
/// and pasting its HTML.
///
/// The benchmark of Find in Book ("[benchmark][search]", results in
/// find_in_book_benchmark_results.md) takes the chapter with the same notes: reading it closed
/// (its KML) and open (its editor's document) as the search reads it, the searches with each
/// option, the lines around the matches the Search panel lists, and Replace All in Book in a
/// closed chapter and in an open one, with its undo.
///
/// The benchmark of Chapter Break ("[benchmark][chapterbreak]", results in
/// chapter_break_benchmark_results.md) splits the chapter with the same notes in the middle:
/// in the editor (the text from the cursor as KML, taken away, and back with its undo), and as
/// the whole command in a book, with its saving, the tab of the new text and Undo.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/text_sorting.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_tables.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/gui/book_search_coordinator.h>
#include <kalahari/gui/chapter_break_coordinator.h>
#include "../editor/editor_test_utils.h"
#include "../gui/document_window.h"
#include "test_document_generator.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QPixmap>
#include <QScreen>
#include <QTextDocument>
#include <QTextTable>
#include <QTextStream>
#include <QWheelEvent>

#include <functional>
#include <vector>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "psapi.lib")
#endif
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

using namespace kalahari::editor;
using namespace kalahari::test;
using kalahari::benchmarks::TestDocumentGenerator;
using kalahari::gui::BookSearchCoordinator;
using kalahari::gui::ChapterBreakCoordinator;
using kalahari::gui::chapterBreakTitle;
using kalahari::gui::SearchPanelHit;
using kalahari::gui::shownSearchHit;

namespace {

struct BenchRow {
    QString operation;
    double ms;
    QString note;
};

double timeMs(const std::function<void()>& fn) {
    QElapsedTimer timer;
    timer.start();
    fn();
    return static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
}

QString generateKml(int words) {
    TestDocumentGenerator::Config config;
    config.targetWordCount = words;
    TestDocumentGenerator generator(config);
    return generator.generateKml();
}

/// The paragraph styles of the base book type, which every book type of the program uses
const ParagraphStyleSet& bookStyles() {
    static const ParagraphStyleSet styles = [] {
        kalahari::core::BookTypeRegistry registry;
        registry.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        return ParagraphStyleSet::fromPackage(
            registry.paragraphStyles(QString::fromLatin1(kalahari::core::ProjectManager::BASE_PACKAGE)),
            QStringLiteral("pl"));
    }();
    return styles;
}

/// Show or hide the formatting marks (View > Show Formatting Marks)
void showFormattingMarks(BookEditor& editor, bool show) {
    EditorAppearance appearance = editor.appearance();
    appearance.showFormattingMarks = show;
    editor.setAppearance(appearance);
}

void paint(QWidget& widget) {
    QPixmap target(widget.size());
    widget.render(&target);
}

KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// Memory the process holds in RAM, in MB (-1 where the platform is not covered)
double residentMemoryMb() {
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0);
    }
#elif defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> fields = statm.readAll().split(' ');
        if (fields.size() > 1) {
            return fields[1].toDouble() * static_cast<double>(sysconf(_SC_PAGESIZE)) /
                   (1024.0 * 1024.0);
        }
    }
#endif
    return -1.0;
}

void typeChar(QWidget& widget, QChar c) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QString(c));
    QCoreApplication::sendEvent(&widget, &press);
}

void report(const std::vector<BenchRow>& rows, const QStringList& header,
            const QString& defaultPath = QStringLiteral("stage0_benchmark_results.md")) {
    QString out;
    QTextStream s(&out);
    for (const auto& line : header) s << line << "\n";
    s << "\n| Operacja | Czas [ms] | Uwagi |\n|---|---:|---|\n";
    for (const auto& r : rows) {
        s << "| " << r.operation << " | " << QString::number(r.ms, 'f', 1) << " | " << r.note
          << " |\n";
    }
    s.flush();

    WARN(out.toStdString());  // Catch2 always prints WARN messages

    QString path = qEnvironmentVariable("KALAHARI_BENCH_OUT");
    if (path.isEmpty()) path = defaultPath;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(out.toUtf8());
    }
}

/// The configuration and the screen, the first lines of a report
QStringList reportHeader() {
    QStringList header;
#ifdef NDEBUG
    header << QStringLiteral("Konfiguracja: Release");
#else
    header << QStringLiteral("Konfiguracja: Debug (czasy zawyżone względem Release)");
#endif
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        header << QStringLiteral("Ekran: physicalDotsPerInch=%1, logicalDotsPerInch=%2, "
                                 "devicePixelRatio=%3")
                      .arg(screen->physicalDotsPerInch(), 0, 'f', 1)
                      .arg(screen->logicalDotsPerInch(), 0, 'f', 1)
                      .arg(screen->devicePixelRatio(), 0, 'f', 2);
    }
    return header;
}

/// A table of KML: a header row and @p rows rows of four cells (a name, a description of two
/// paragraphs, a date and a number); with @p tallRow, a row of a cell of 120 paragraphs, higher
/// than a page
QString tableKml(int rows, int number, bool tallRow) {
    QString kml = QStringLiteral("<table><col width=\"20%\"/><col width=\"50%\"/>"
                                 "<col width=\"15%\"/><col width=\"15%\"/>"
                                 "<tr><th><p>Postać</p></th><th><p>Opis</p></th>"
                                 "<th><p>Data</p></th><th><p>Liczba</p></th></tr>");
    for (int row = 0; row < rows; ++row) {
        const int value = (row * 7919 + number * 31) % 1000;
        kml += QStringLiteral("<tr><td><p>Postać %1</p></td><td><p>Opis postaci %1, która "
                              "pojawia się w rozdziale i ma swoją rolę w opowieści.</p>"
                              "<p>Drugi akapit opisu.</p></td><td><p>%2.%3.2026</p></td>"
                              "<td><p>%4</p></td></tr>")
                   .arg(value)
                   .arg(row % 28 + 1)
                   .arg(row % 12 + 1)
                   .arg(value * 3);
        if (tallRow && row == rows / 2) {
            kml += QStringLiteral("<tr><td><p>Wysoki wiersz</p></td><td>") +
                   QStringLiteral("<p>Akapit wysokiego wiersza, który nie mieści się na stronie.</p>")
                       .repeated(120) +
                   QStringLiteral("</td><td><p>1.1.2026</p></td><td><p>1</p></td></tr>");
        }
    }
    return kml + QStringLiteral("</table>");
}

/// The chapter with a table of 12 rows after every 200th paragraph, and one of @p longRows rows
/// (with a row higher than a page) in the middle; @p tables gets their number
QString withTables(const QString& kml, int longRows, int* tables) {
    const QString end = QStringLiteral("</p>");
    const qsizetype total = kml.count(end);
    QString result;
    result.reserve(kml.size() * 3 / 2);
    qsizetype from = 0;
    qsizetype paragraph = 0;
    int count = 0;
    for (qsizetype at = kml.indexOf(end); at >= 0; at = kml.indexOf(end, from)) {
        const qsizetype after = at + end.size();
        result += QStringView(kml).mid(from, after - from);
        from = after;
        ++paragraph;
        if (paragraph == total / 2) {
            result += tableKml(longRows, count++, true);
        } else if (paragraph % 200 == 0) {
            result += tableKml(12, count++, false);
        }
    }
    result += QStringView(kml).mid(from);
    *tables = count;
    return result;
}

/// The chapter with a footnote at the end of every 5th paragraph and an endnote at the end of
/// every 50th; @p notes gets their number
QString withNotes(const QString& kml, int* notes) {
    const QString end = QStringLiteral("</p>");
    QString result;
    result.reserve(kml.size() * 5 / 4);
    qsizetype from = 0;
    int paragraph = 0;
    int count = 0;
    for (qsizetype at = kml.indexOf(end); at >= 0; at = kml.indexOf(end, from)) {
        result += QStringView(kml).mid(from, at - from);
        ++paragraph;
        if (paragraph % 50 == 0) {
            result += QStringLiteral("<endnote id=\"e%1\">Przypis końcowy %1, ze źródłem i "
                                     "rokiem wydania.</endnote>")
                          .arg(count++);
        } else if (paragraph % 5 == 0) {
            result += QStringLiteral("<footnote id=\"f%1\">Przypis %1 objaśnia słowo z akapitu "
                                     "i podaje <i>tytuł</i> dzieła, z którego pochodzi.</footnote>")
                          .arg(count++);
        }
        result += end;
        from = at + end.size();
    }
    result += QStringView(kml).mid(from);
    *notes = count;
    return result;
}

/// The first paragraph from the given one on with a note
int paragraphWithNote(const BookEditor& editor, int from) {
    for (QTextBlock block = editor.textDocument()->findBlockByNumber(from); block.isValid();
         block = block.next()) {
        if (hasNotes(block)) {
            return block.blockNumber();
        }
    }
    return from;
}

/// The first table with at least @p rows rows
QTextTable* tableWithRows(const BookEditor& editor, int rows) {
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid(); block = block.next()) {
        if (QTextTable* table = tableOf(block); table && table->rows() >= rows) {
            return table;
        }
    }
    return nullptr;
}

/// The first paragraph from the given one on that is not in a table
int paragraphOutsideTables(const BookEditor& editor, int from) {
    for (QTextBlock block = editor.textDocument()->findBlockByNumber(from); block.isValid();
         block = block.next()) {
        if (tableOf(block) == nullptr && block.length() > 1) {
            return block.blockNumber();
        }
    }
    return from;
}

/// The place of the start of a cell, for the cursor
CursorPosition cellPlace(const QTextTable& table, int row, int column) {
    const QTextBlock block = table.document()->findBlock(table.cellAt(row, column).firstPosition());
    return {block.blockNumber(), 0};
}

}  // anonymous namespace

TEST_CASE("Stage0 benchmark: editor operations on a 150k-word document",
          "[.][benchmark][stage0]") {
    std::vector<BenchRow> rows;
    QStringList header = reportHeader();

    // -------------------------------------------------------------------------
    // 1. Load scaling: fromKml at growing sizes (quadratic => ~4x per 2x)
    // -------------------------------------------------------------------------
    // Memory is taken before the first document: memory a document frees stays with the
    // process and is reused by the next one, so a later baseline would hide it
    const double memoryBefore = residentMemoryMb();
    double memoryLoaded = -1.0;
    for (int words : {25000, 50000, 100000, 150000}) {
        const QString kml = generateKml(words);

        KmlDocumentModel model;
        const double parseMs = timeMs([&] { model.loadKml(kml); });

        BookEditor editor;
        resizeWidget(editor, QSize(1000, 800));
        editor.setParagraphStyles(bookStyles());
        const double loadMs = timeMs([&] { editor.fromKml(kml); });
        const double firstPaintMs = timeMs([&] { paint(editor); });
        const double restMs = timeMs([&] { layoutOf(editor)->layoutPendingBlocks(); });
        if (words == 150000) {
            memoryLoaded = residentMemoryMb();
        }

        rows.push_back({QStringLiteral("fromKml – %1k słów").arg(words / 1000), loadMs,
                        QStringLiteral("%1 akapitów; w tym parsowanie KmlDocumentModel %2 ms; "
                                       "pierwszy paintEvent %3 ms; łamanie reszty w tle %4 ms")
                            .arg(editor.paragraphCount())
                            .arg(parseMs, 0, 'f', 1)
                            .arg(firstPaintMs, 0, 'f', 1)
                            .arg(restMs, 0, 'f', 1)});
    }
    rows.push_back({QStringLiteral("Pamięć zajęta przez rozdział 150k słów (wczytany i złamany "
                                   "w całości)"),
                    memoryLoaded - memoryBefore,
                    QStringLiteral("MB, a nie ms; proces przed wczytaniem %1 MB, potem %2 MB")
                        .arg(memoryBefore, 0, 'f', 0)
                        .arg(memoryLoaded, 0, 'f', 0)});

    // -------------------------------------------------------------------------
    // 2. Operations on the full 150k document
    // -------------------------------------------------------------------------
    const QString kml = generateKml(150000);
    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.setParagraphStyles(bookStyles());
    editor.fromKml(kml);
    KalahariTextDocumentLayout* layout = layoutOf(editor);
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów, %3 znaków; w tym %4 nagłówków, "
                             "%5 akapitów w stylu Dialog i %6 podziałów sceny")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(editor.characterCount())
                  .arg(kml.count(QStringLiteral("<h2>")))
                  .arg(kml.count(QStringLiteral("style=\"dialogue\"")))
                  .arg(kml.count(QStringLiteral("<scene-break/>")));

    rows.push_back({QStringLiteral("Ctrl+End zaraz po wczytaniu (+ paintEvent)"),
                    timeMs([&] {
                        editor.moveCursorToDocEnd();
                        paint(editor);
                    }),
                    QStringLiteral("akapitów czekających na złamanie: %1")
                        .arg(layout->pendingBlockCount())});
    editor.moveCursorToDocStart();
    layout->layoutPendingBlocks();  // what the background pass does after a load
    paint(editor);  // warm-up
    rows.push_back({QStringLiteral("paintEvent – Continuous (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});

    // View > Show Formatting Marks: drawn over the text, the lines stay as they are
    {
        LaidOutBlockCounter laidOut(editor);
        rows.push_back({QStringLiteral("Pokazanie znaków formatowania (Ctrl+Shift+8) + paintEvent"),
                        timeMs([&] {
                            showFormattingMarks(editor, true);
                            paint(editor);
                        }),
                        QStringLiteral("złamanych akapitów: %1").arg(laidOut.count())});
        rows.push_back({QStringLiteral("paintEvent ze znakami formatowania – Continuous (średnio z 10)"),
                        timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0,
                        QStringLiteral("porównaj z Continuous bez znaków")});
        showFormattingMarks(editor, false);
        paint(editor);
    }

    // Format > Text Style: one paragraph (Alt+1), and the whole text at once
    {
        const int middleParagraph = static_cast<int>(editor.paragraphCount() / 2);
        editor.setCursorPosition({middleParagraph, 0});
        rows.push_back({QStringLiteral("Nagłówek 1 dla akapitu w środku (Alt+1) + paintEvent"),
                        timeMs([&] {
                            editor.applyParagraphStyle(QStringLiteral("heading_1"));
                            paint(editor);
                        }),
                        QString()});
        CHECK(editor.currentParagraphStyle() == QStringLiteral("heading_1"));
        editor.undo();
        editor.selectAll();
        rows.push_back({QStringLiteral("Styl Cytat dla całego tekstu (Ctrl+A, Format > Styl tekstu)"),
                        timeMs([&] { editor.applyParagraphStyle(QStringLiteral("quote")); }),
                        QStringLiteral("jeden krok cofania")});
        CHECK(editor.currentParagraphStyle() == QStringLiteral("quote"));
        rows.push_back({QStringLiteral("paintEvent po zmianie stylu całego tekstu"),
                        timeMs([&] { paint(editor); }), QString()});
        rows.push_back({QStringLiteral("Łamanie pozostałych akapitów po zmianie stylu"),
                        timeMs([&] { layout->layoutPendingBlocks(); }), QString()});
        rows.push_back({QStringLiteral("Cofnięcie zmiany stylu całego tekstu (Ctrl+Z)"),
                        timeMs([&] { editor.undo(); }), QString()});
        editor.clearSelection();
        editor.moveCursorToDocStart();
        paint(editor);
        layout->layoutPendingBlocks();
    }

    // Resize (width change): applied at once; the paint lays out what it shows
    {
        LaidOutBlockCounter laidOut(editor);
        const double ms = timeMs([&] { resizeWidget(editor, QSize(1200, 800)); });
        rows.push_back({QStringLiteral("Zmiana szerokości 1000→1200 px"), ms,
                        QStringLiteral("złamanych akapitów: %1").arg(laidOut.count())});
        rows.push_back({QStringLiteral("paintEvent po zmianie szerokości"),
                        timeMs([&] { paint(editor); }),
                        QStringLiteral("złamanych akapitów łącznie: %1").arg(laidOut.count())});
        rows.push_back({QStringLiteral("Łamanie pozostałych akapitów (cała praca tła)"),
                        timeMs([&] { layout->layoutPendingBlocks(); }),
                        QStringLiteral("tło dzieli ją na kroki po 4 ms")});
    }

    // Zoom in Continuous mode
    {
        LaidOutBlockCounter laidOut(editor);
        const double ms = timeMs([&] { editor.setZoomFactor(1.25); });
        rows.push_back({QStringLiteral("Zmiana powiększenia 100→125% (Continuous)"), ms,
                        QStringLiteral("złamanych akapitów: %1").arg(laidOut.count())});
        rows.push_back({QStringLiteral("paintEvent po zmianie powiększenia"),
                        timeMs([&] { paint(editor); }),
                        QStringLiteral("złamanych akapitów łącznie: %1").arg(laidOut.count())});
        rows.push_back({QStringLiteral("Łamanie pozostałych akapitów (cała praca tła)"),
                        timeMs([&] { layout->layoutPendingBlocks(); }), QString()});
        editor.setZoomFactor(1.0);
        paint(editor);
        layout->layoutPendingBlocks();
    }

    // Ctrl+wheel zoom: five notches in a row, each applied and painted at once
    {
        LaidOutBlockCounter laidOut(editor);
        const double wheelMs = timeMs([&] {
            for (int notch = 0; notch < 5; ++notch) {
                QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, 120),
                                  Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
                QCoreApplication::sendEvent(&editor, &wheel);
                paint(editor);
            }
        });
        rows.push_back({QStringLiteral("Ctrl+kółko – 5 ząbków, paintEvent po każdym (Continuous)"),
                        wheelMs, QStringLiteral("złamanych akapitów: %1").arg(laidOut.count())});
        editor.setZoomFactor(1.0);
        paint(editor);
        layout->layoutPendingBlocks();
    }

    // Typing 100 characters in the middle of the document
    const QString typed = QStringLiteral("lorem ipsum dolor sit amet ").repeated(4).left(100);
    auto typeHundred = [&](bool withPaint) {
        return timeMs([&] {
            for (QChar c : typed) {
                typeChar(editor, c);
                if (withPaint) paint(editor);
            }
        });
    };
    const int middle = static_cast<int>(editor.paragraphCount() / 2);

    editor.setCursorPosition({middle, 0});
    rows.push_back({QStringLiteral("Wpisanie 100 znaków – Continuous (bez malowania)"),
                    typeHundred(false), QString()});
    editor.setCursorPosition({middle, 0});
    {
        const double ms = typeHundred(true);
        rows.push_back({QStringLiteral("Wpisanie 100 znaków – Continuous (z paintEvent po każdym)"), ms,
                        QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }

    editor.setViewMode(ViewMode::Page);
    rows.push_back({QStringLiteral("Przełączenie na Page + pierwszy paintEvent (paginacja)"),
                    timeMs([&] { paint(editor); }),
                    QStringLiteral("nagłówki nie zostają same u dołu strony")});
    editor.setCursorPosition({middle, 0});
    const size_t paragraphsBefore = editor.paragraphCount();
    rows.push_back({QStringLiteral("Podział strony w środku (Ctrl+Enter) + paintEvent – Page"),
                    timeMs([&] {
                        editor.insertTextObject(TextObject::PageBreak);
                        paint(editor);
                    }),
                    QStringLiteral("strony za nim przesuwają się")});
    CHECK(editor.paragraphCount() == paragraphsBefore + 1);
    editor.undo();
    CHECK(editor.paragraphCount() == paragraphsBefore);
    paint(editor);
    showFormattingMarks(editor, true);
    paint(editor);
    rows.push_back({QStringLiteral("paintEvent ze znakami formatowania – Page (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});
    showFormattingMarks(editor, false);
    editor.setCursorPosition({middle, 0});
    rows.push_back({QStringLiteral("Wpisanie 100 znaków – Page (bez malowania)"),
                    typeHundred(false), QString()});
    editor.setCursorPosition({middle, 0});
    {
        const double ms = typeHundred(true);
        rows.push_back({QStringLiteral("Wpisanie 100 znaków – Page (z paintEvent po każdym)"), ms,
                        QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }
    editor.setViewMode(ViewMode::Continuous);
    paint(editor);

    // Scroll to the end
    rows.push_back({QStringLiteral("Przewinięcie na koniec (scrollTo + paintEvent)"),
                    timeMs([&] {
                        editor.scrollTo(1.0e12);
                        paint(editor);
                    }),
                    QString()});
    rows.push_back({QStringLiteral("Ctrl+End (kursor na koniec + paintEvent)"),
                    timeMs([&] {
                        editor.scrollTo(0.0);
                        editor.moveCursorToDocEnd();
                        paint(editor);
                    }),
                    QString()});

    // ViewportManager linear scans (same document, standalone manager)
    {
        ViewportManager vm;
        vm.setViewportSize(editor.size());
        vm.setDocument(editor.textDocument());
        const size_t last = editor.paragraphCount() - 1;
        double sink = 0.0;
        const double yMs = timeMs([&] { for (int i = 0; i < 100; ++i) sink += vm.paragraphY(last); });
        const double total = vm.totalDocumentHeight();
        const double atMs = timeMs([&] { for (int i = 0; i < 100; ++i) sink += static_cast<double>(vm.paragraphAtY(total - 1.0)); });
        const double scrollMs = timeMs([&] { vm.setScrollPosition(total); });
        rows.push_back({QStringLiteral("ViewportManager::paragraphY(ostatni) ×100"), yMs,
                        QStringLiteral("%1 ms/wywołanie").arg(yMs / 100.0, 0, 'f', 3)});
        rows.push_back({QStringLiteral("ViewportManager::paragraphAtY(koniec) ×100"), atMs,
                        QStringLiteral("%1 ms/wywołanie").arg(atMs / 100.0, 0, 'f', 3)});
        rows.push_back({QStringLiteral("ViewportManager::setScrollPosition(koniec)"), scrollMs,
                        QStringLiteral("updateVisibleRange skanuje od początku")});
        CHECK(sink > 0.0);
    }

    // Distraction-free painting (word count on every paint?)
    editor.scrollTo(0.0);
    editor.setDistractionFree(true);
    paint(editor);
    rows.push_back({QStringLiteral("paintEvent – DistractionFree (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0,
                    QStringLiteral("porównaj z Continuous")});
    editor.setDistractionFree(false);

    // Search highlights: a very common letter gives many thousands of matches
    {
        editor.showFind();  // creates the search engine, as Ctrl+F does
        SearchEngine* search = editor.searchEngine();
        search->setSearchText(QStringLiteral("a"));
        int matchCount = 0;
        const double searchMs = timeMs([&] { matchCount = search->totalMatchCount(); });
        rows.push_back({QStringLiteral("Wyszukanie „a” (lista wszystkich trafień)"), searchMs,
                        QStringLiteral("%1 trafień").arg(matchCount)});
        paint(editor);
        rows.push_back({QStringLiteral("paintEvent z wyróżnieniem wyników wyszukiwania"),
                        timeMs([&] { paint(editor); }), QString()});
        editor.hideFindReplace();
        paint(editor);
    }

    // Select All + copy: the clipboard data (KML, HTML, text) is the work, putting it on
    // the system clipboard would overwrite what the user has there
    rows.push_back({QStringLiteral("Select All"), timeMs([&] { editor.selectAll(); }), QString()});
    rows.push_back({QStringLiteral("Copy (cały dokument)"),
                    timeMs([&] { editor.createMimeDataFromSelection(); }),
                    QStringLiteral("dane dla schowka (KML, HTML, tekst), bez zapisu do schowka")});
    rows.push_back({QStringLiteral("paintEvent z zaznaczonym całym dokumentem"),
                    timeMs([&] { paint(editor); }), QString()});

    // Replacing the whole text as one edit (restoring a snapshot), and undoing it
    editor.clearSelection();
    rows.push_back({QStringLiteral("Podmiana całej treści (replaceWithKml)"),
                    timeMs([&] { editor.replaceWithKml(kml); }),
                    QStringLiteral("jeden krok cofania")});
    rows.push_back({QStringLiteral("paintEvent po podmianie"), timeMs([&] { paint(editor); }),
                    QString()});
    rows.push_back({QStringLiteral("Cofnięcie podmiany (Ctrl+Z)"),
                    timeMs([&] { editor.undo(); }), QString()});
    rows.push_back({QStringLiteral("Ponowienie podmiany (Ctrl+Y)"),
                    timeMs([&] { editor.redo(); }), QString()});

    report(rows, header);
    CHECK(editor.paragraphCount() > 0);
}

TEST_CASE("Tables benchmark: the Table menu on a 150k-word chapter with tables",
          "[.][benchmark][tables]") {
    std::vector<BenchRow> rows;
    QStringList header = reportHeader();
    constexpr int LONG_ROWS = 400;

    // -------------------------------------------------------------------------
    // 1. Loading
    // -------------------------------------------------------------------------
    int tables = 0;
    const QString kml = withTables(generateKml(150000), LONG_ROWS, &tables);
    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.setParagraphStyles(bookStyles());
    const double loadMs = timeMs([&] { editor.fromKml(kml); });
    const double firstPaintMs = timeMs([&] { paint(editor); });
    KalahariTextDocumentLayout* layout = layoutOf(editor);
    const double restMs = timeMs([&] { layout->layoutPendingBlocks(); });
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów (z akapitami komórek), %3 tabel: po "
                             "12 wierszy co 200 akapitów i jedna z %4 wierszami, a w niej wiersz "
                             "wyższy od strony (komórka ze 120 akapitami)")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(tables)
                  .arg(LONG_ROWS);
    rows.push_back({QStringLiteral("fromKml – 150k słów z tabelami"), loadMs,
                    QStringLiteral("pierwszy paintEvent %1 ms; łamanie reszty w tle %2 ms")
                        .arg(firstPaintMs, 0, 'f', 1)
                        .arg(restMs, 0, 'f', 1)});

    QTextTable* big = tableWithRows(editor, LONG_ROWS);
    REQUIRE(big != nullptr);
    const int bigRows = big->rows();
    const auto toCell = [&](int row, int column) {
        editor.clearSelection();
        editor.setCursorPosition(cellPlace(*big, row, column));
        paint(editor);
    };

    // -------------------------------------------------------------------------
    // 2. Continuous: painting and typing in a cell
    // -------------------------------------------------------------------------
    const QString typed = QStringLiteral("lorem ipsum dolor sit amet ").repeated(4).left(100);
    const auto typeHundred = [&]() {
        return timeMs([&] {
            for (QChar c : typed) {
                typeChar(editor, c);
                paint(editor);
            }
        });
    };
    // For comparison: a paragraph outside the tables, and a cell of a table of 12 rows
    {
        const int outside = paragraphOutsideTables(editor, static_cast<int>(editor.paragraphCount() / 4));
        REQUIRE(tableOf(editor.textDocument()->findBlockByNumber(outside)) == nullptr);
        editor.setCursorPosition({outside, 0});
        paint(editor);
        const double ms = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w akapicie poza tabelą – Continuous (z "
                                       "paintEvent po każdym)"),
                        ms, QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
        // ("small" is a macro of the Windows headers)
        QTextTable* shortTable = tableWithRows(editor, 12);
        REQUIRE(shortTable != nullptr);
        editor.setCursorPosition(cellPlace(*shortTable, 5, 1));
        paint(editor);
        const double cellMs = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w komórce tabeli z 12 wierszami – "
                                       "Continuous (z paintEvent po każdym)"),
                        cellMs, QStringLiteral("%1 ms/znak").arg(cellMs / 100.0, 0, 'f', 2)});
    }
    toCell(100, 1);
    rows.push_back({QStringLiteral("paintEvent – Continuous, tabela w widoku (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});
    {
        const double ms = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w komórce tabeli z %1 wierszami – "
                                       "Continuous (z paintEvent po każdym)")
                            .arg(bigRows),
                        ms, QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }

    // -------------------------------------------------------------------------
    // 3. Page view: the header rows on each page, the row higher than a page split
    // -------------------------------------------------------------------------
    editor.setViewMode(ViewMode::Page);
    {
        const double ms = timeMs([&] { paint(editor); });
        int splitRows = 0;
        for (const auto& row : layout->tableRows(*big)) {
            splitRows += row.parts.size() > 1 ? 1 : 0;
        }
        rows.push_back({QStringLiteral("Przełączenie na Page + pierwszy paintEvent (paginacja)"), ms,
                        QStringLiteral("nagłówek długiej tabeli powtórzony %1 razy; wierszy "
                                       "podzielonych między strony: %2")
                            .arg(layout->headerRepeats(*big).size())
                            .arg(splitRows)});
        CHECK(!layout->headerRepeats(*big).empty());
        CHECK(splitRows >= 1);
    }
    toCell(300, 1);
    rows.push_back({QStringLiteral("paintEvent – Page, tabela z powtórzonym nagłówkiem (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});
    {
        const double ms = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w komórce – Page (z paintEvent po każdym)"),
                        ms, QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }

    // -------------------------------------------------------------------------
    // 4. The commands of the Table menu on the long table, in the page view
    // -------------------------------------------------------------------------
    const auto step = [&](const QString& name, const std::function<void()>& command,
                          const QString& note = QString()) {
        rows.push_back({name + QStringLiteral(" + paintEvent"), timeMs([&] {
                            command();
                            paint(editor);
                        }),
                        note});
        rows.push_back({QStringLiteral("    cofnięcie (Ctrl+Z) + paintEvent"), timeMs([&] {
                            editor.undo();
                            paint(editor);
                        }),
                        QString()});
        big = tableWithRows(editor, LONG_ROWS);
        REQUIRE(big != nullptr);
        CHECK(big->rows() == bigRows);
    };

    toCell(50, 1);
    step(QStringLiteral("Wstaw > Wiersze poniżej"), [&] { editor.insertTableRows(true); },
         QStringLiteral("strony za nim przesuwają się"));
    toCell(50, 1);
    step(QStringLiteral("Usuń > Wiersze"), [&] { editor.deleteTableRows(); });
    toCell(50, 1);
    step(QStringLiteral("Wstaw > Kolumny z prawej"), [&] { editor.insertTableColumns(true); },
         QStringLiteral("%1 nowych komórek").arg(bigRows));

    // A rectangle of 100 rows: Shift+Down, copy, Delete
    toCell(10, 0);
    rows.push_back({QStringLiteral("Zaznaczenie prostokąta komórek: Shift+↓ ×100 (paintEvent po każdym)"),
                    timeMs([&] {
                        for (int i = 0; i < 100; ++i) {
                            QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
                            QCoreApplication::sendEvent(&editor, &down);
                            paint(editor);
                        }
                    }),
                    QString()});
    {
        const std::optional<TableCellRange> cells = editor.selectedCellRange();
        REQUIRE(cells.has_value());
        CHECK(cells->rows() > 50);
        rows.push_back({QStringLiteral("Kopiowanie zaznaczonych komórek"),
                        timeMs([&] { editor.createMimeDataFromSelection(); }),
                        QStringLiteral("%1 wierszy; dane dla schowka (KML, HTML, tekst)")
                            .arg(cells->rows())});
    }
    step(QStringLiteral("Delete – wyczyszczenie zaznaczonych komórek"), [&] {
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QCoreApplication::sendEvent(&editor, &key);
    });

    toCell(50, 1);
    step(QStringLiteral("Sortuj… – według kolumny Liczba, malejąco"), [&] {
        SortOptions options;
        options.keys.push_back({3, SortType::Numbers, true});
        editor.sortSelection(options);
    }, QStringLiteral("%1 wierszy pod nagłówkiem").arg(bigRows - 1));
    toCell(50, 1);
    step(QStringLiteral("Sortuj… – według kolumny Postać (tekst), potem Data"), [&] {
        SortOptions options;
        options.keys.push_back({0, SortType::Text, false});
        options.keys.push_back({2, SortType::Dates, false});
        editor.sortSelection(options);
    });

    toCell(50, 1);
    rows.push_back({QStringLiteral("Alt+Shift+↓ ×10 (paintEvent po każdym)"), timeMs([&] {
                        for (int i = 0; i < 10; ++i) {
                            editor.moveParagraphsOrRows(true);
                            paint(editor);
                        }
                    }),
                    QStringLiteral("każde przesunięcie to krok cofania")});
    for (int i = 0; i < 10; ++i) {
        editor.undo();
    }

    toCell(50, 1);
    step(QStringLiteral("Właściwości tabeli… – do środka, ramka, wiersz wysoki na 1 cm"), [&] {
        std::optional<TablePropertiesData> data = editor.tablePropertiesData();
        REQUIRE(data.has_value());
        data->table.alignment = TableAlignment::Center;
        data->table.width = 360.0;
        data->table.borders.lines = BORDER_BOX;
        data->rows[50].minHeight = 72.0 / 2.54;
        editor.setTablePropertiesData(*data);
    });
    toCell(50, 1);
    step(QStringLiteral("Przeciągnięcie krawędzi kolumny"), [&] {
        const std::vector<qreal> edges = layout->tableColumnEdges(*big);
        editor.setColumnEdge(*big, 1, edges[1] + 40.0);
    });
    toCell(50, 1);
    step(QStringLiteral("Rozłóż kolumny równomiernie"), [&] { editor.distributeTableColumns(); });

    // Table to text and back
    toCell(50, 1);
    int firstParagraph = 0;
    rows.push_back({QStringLiteral("Tabelę na tekst… (tabulatory) + paintEvent"), timeMs([&] {
                        editor.convertTableToText(QLatin1Char('\t'));
                        paint(editor);
                    }),
                    QStringLiteral("%1 akapitów").arg(bigRows)});
    firstParagraph = editor.cursorPosition().paragraph;
    CHECK(tableWithRows(editor, LONG_ROWS) == nullptr);
    {
        const int last = firstParagraph + bigRows - 1;
        const QTextBlock lastBlock = editor.textDocument()->findBlockByNumber(last);
        editor.setCursorPosition({last, lastBlock.length() - 1});
        editor.setSelection({{firstParagraph, 0}, {last, lastBlock.length() - 1}});
    }
    rows.push_back({QStringLiteral("Tekst na tabelę… (tabulatory) + paintEvent"), timeMs([&] {
                        editor.convertTextToTable(QLatin1Char('\t'), 0, true);
                        paint(editor);
                    }),
                    QString()});
    CHECK(tableWithRows(editor, LONG_ROWS) != nullptr);
    rows.push_back({QStringLiteral("Cofnięcie obu zamian (Ctrl+Z ×2) + paintEvent"), timeMs([&] {
                        editor.undo();
                        editor.undo();
                        paint(editor);
                    }),
                    QString()});
    big = tableWithRows(editor, LONG_ROWS);
    REQUIRE(big != nullptr);
    CHECK(big->rows() == bigRows);

    // Saving: the whole chapter with its tables as KML
    rows.push_back({QStringLiteral("toKml – cały rozdział z tabelami"),
                    timeMs([&] { CHECK(!editor.toKml().isEmpty()); }), QString()});

    report(rows, header, QStringLiteral("tables_benchmark_results.md"));
}

TEST_CASE("Notes benchmark: a 150k-word chapter with footnotes and endnotes",
          "[.][benchmark][footnotes]") {
    std::vector<BenchRow> rows;
    QStringList header = reportHeader();

    // -------------------------------------------------------------------------
    // 1. Loading
    // -------------------------------------------------------------------------
    int notes = 0;
    const QString kml = withNotes(generateKml(150000), &notes);
    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.setParagraphStyles(bookStyles());
    const double loadMs = timeMs([&] { editor.fromKml(kml); });
    const double firstPaintMs = timeMs([&] { paint(editor); });
    KalahariTextDocumentLayout* layout = layoutOf(editor);
    const double restMs = timeMs([&] { layout->layoutPendingBlocks(); });
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów, %3 przypisów: dolny na końcu co "
                             "5. akapitu, końcowy co 50.")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(notes);
    CHECK(notesIn(*editor.textDocument()).size() == static_cast<size_t>(notes));
    rows.push_back({QStringLiteral("fromKml – 150k słów z przypisami"), loadMs,
                    QStringLiteral("pierwszy paintEvent %1 ms; łamanie reszty w tle %2 ms")
                        .arg(firstPaintMs, 0, 'f', 1)
                        .arg(restMs, 0, 'f', 1)});

    const QString typed = QStringLiteral("lorem ipsum dolor sit amet ").repeated(4).left(100);
    const auto typeHundred = [&]() {
        return timeMs([&] {
            for (QChar c : typed) {
                typeChar(editor, c);
                paint(editor);
            }
        });
    };
    const auto beforeNote = [&](int paragraph) {
        const int block = paragraphWithNote(editor, paragraph);
        editor.clearSelection();
        editor.setCursorPosition({block, 10});
        paint(editor);
    };

    // -------------------------------------------------------------------------
    // 2. Continuous: the footnotes under their paragraphs
    // -------------------------------------------------------------------------
    beforeNote(static_cast<int>(editor.paragraphCount() / 2));
    rows.push_back({QStringLiteral("paintEvent – Continuous, przypisy pod akapitami (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});
    {
        const double ms = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w akapicie z przypisem – Continuous "
                                       "(z paintEvent po każdym)"),
                        ms, QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }

    // A new note at the start: the numbers of all the others change
    editor.clearSelection();
    editor.setCursorPosition({0, 3});
    paint(editor);
    QString added;
    rows.push_back({QStringLiteral("Wstaw > Przypis dolny na początku + tekst + paintEvent"),
                    timeMs([&] {
                        added = editor.insertNote(NoteKind::Footnote);
                        editor.setNoteContent(added, QStringLiteral("Nowy przypis."), true);
                        paint(editor);
                    }),
                    QStringLiteral("numery %1 przypisów za nim się zmieniają").arg(notes)});
    CHECK(editor.noteNumber(added) == QStringLiteral("1"));
    rows.push_back({QStringLiteral("    cofnięcie (Ctrl+Z) + paintEvent"), timeMs([&] {
                        editor.undo();
                        paint(editor);
                    }),
                    QString()});
    CHECK(notesIn(*editor.textDocument()).size() == static_cast<size_t>(notes));

    // -------------------------------------------------------------------------
    // 3. Page view: the footnotes at the bottom of the pages
    // -------------------------------------------------------------------------
    rows.push_back({QStringLiteral("Przełączenie na Page + pierwszy paintEvent (paginacja)"),
                    timeMs([&] {
                        editor.setViewMode(ViewMode::Page);
                        paint(editor);
                    }),
                    QString()});
    beforeNote(static_cast<int>(editor.paragraphCount() / 2));
    rows.push_back({QStringLiteral("paintEvent – Page, przypisy u dołu stron (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});
    {
        const double ms = typeHundred();
        rows.push_back({QStringLiteral("Wpisanie 100 znaków w akapicie z przypisem – Page (z "
                                       "paintEvent po każdym)"),
                        ms, QStringLiteral("%1 ms/znak").arg(ms / 100.0, 0, 'f', 2)});
    }

    // -------------------------------------------------------------------------
    // 4. The words and the search of the notes' texts (Page view)
    // -------------------------------------------------------------------------
    {
        kalahari::core::WordCountRules rules = editor.wordCountRules();
        rules.notesAreWords = false;
        size_t without = 0;
        const double offMs = timeMs([&] {
            editor.setWordCountRules(rules);
            without = editor.wordCount();
        });
        rules.notesAreWords = true;
        size_t with = 0;
        const double onMs = timeMs([&] {
            editor.setWordCountRules(rules);
            with = editor.wordCount();
        });
        CHECK(with > without);
        rows.push_back({QStringLiteral("Liczba słów po włączeniu „Licz słowa w przypisach”"), onMs,
                        QStringLiteral("%1 słów, bez przypisów %2 (wyłączenie %3 ms)")
                            .arg(with)
                            .arg(without)
                            .arg(offMs, 0, 'f', 1)});
    }
    {
        // A word only the footnotes have: every match is in the text of a note
        const QString word = QStringLiteral("objaśnia");
        editor.showFind();  // creates the search engine, as Ctrl+F does
        SearchEngine* search = editor.searchEngine();
        search->setSearchText(word);
        int matchCount = 0;
        rows.push_back({QStringLiteral("Wyszukanie słowa z treści przypisów (lista trafień)"),
                        timeMs([&] { matchCount = search->totalMatchCount(); }),
                        QStringLiteral("%1 trafień w przypisach").arg(matchCount)});
        CHECK(matchCount == static_cast<int>(kml.count(word)));
        rows.push_back({QStringLiteral("paintEvent z wyróżnieniem trafień w przypisach (Page)"),
                        timeMs([&] { paint(editor); }), QString()});
        editor.clearSelection();
        editor.setCursorPosition({0, 0});
        const double nextMs = timeMs([&] {
            for (int i = 0; i < 10; ++i) {
                editor.findNext();
                paint(editor);
            }
        });
        CHECK(editor.noteSelection().has_value());
        rows.push_back({QStringLiteral("Znajdź następny – 10 × w treści przypisów (z paintEvent)"),
                        nextMs, QStringLiteral("%1 ms/krok").arg(nextMs / 10.0, 0, 'f', 2)});
        editor.hideFindReplace();
        paint(editor);
    }

    // -------------------------------------------------------------------------
    // 5. The clipboard and saving
    // -------------------------------------------------------------------------
    editor.clearSelection();
    editor.selectAll();
    std::unique_ptr<QMimeData> copied;
    rows.push_back({QStringLiteral("Kopiowanie całości (Ctrl+A, Ctrl+C)"),
                    timeMs([&] { copied = editor.createMimeDataFromSelection(); }),
                    QStringLiteral("dane dla schowka (KML, HTML z przypisami jak LibreOffice, tekst "
                                   "z przypisami na końcu)")});
    REQUIRE(copied);
    QString pasted;
    rows.push_back({QStringLiteral("Odczyt HTML całości z przypisami (wklejenie z innego programu)"),
                    timeMs([&] { pasted = ClipboardHandler::htmlToKml(copied->html()); }),
                    QStringLiteral("bez wstawienia do tekstu")});
    CHECK(pasted.count(QStringLiteral("<footnote ")) + pasted.count(QStringLiteral("<endnote ")) ==
          notes);
    editor.clearSelection();
    rows.push_back({QStringLiteral("toKml – cały rozdział z przypisami"),
                    timeMs([&] { CHECK(!editor.toKml().isEmpty()); }), QString()});

    report(rows, header, QStringLiteral("footnotes_benchmark_results.md"));
}

TEST_CASE("Find in Book benchmark: a 150k-word chapter with notes", "[.][benchmark][search]") {
    std::vector<BenchRow> rows;
    QStringList header = reportHeader();
    int notes = 0;
    const QString kml = withNotes(generateKml(150000), &notes);

    // -------------------------------------------------------------------------
    // 1. Reading the chapter: closed (its KML), open (its editor's document)
    // -------------------------------------------------------------------------
    std::vector<SearchableParagraph> closed;
    const double closedMs = timeMs([&] { closed = searchableParagraphsOfKml(kml); });
    rows.push_back({QStringLiteral("Odczyt zamkniętego rozdziału (KML → akapity do szukania)"),
                    closedMs, QStringLiteral("%1 akapitów").arg(closed.size())});

    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.setParagraphStyles(bookStyles());
    editor.fromKml(kml);
    paint(editor);
    layoutOf(editor)->layoutPendingBlocks();
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów, %3 przypisów: dolny na końcu co "
                             "5. akapitu, końcowy co 50.")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(notes);
    std::vector<SearchableParagraph> open;
    const double openMs = timeMs([&] { open = searchableParagraphs(*editor.textDocument()); });
    rows.push_back({QStringLiteral("Odczyt otwartego rozdziału (dokument edytora → akapity)"),
                    openMs, QStringLiteral("po każdej zmianie tekstu, gdy panel ma wyniki")});
    CHECK(open == closed);

    // -------------------------------------------------------------------------
    // 2. Searching the paragraphs read
    // -------------------------------------------------------------------------
    const auto searchRow = [&](const QString& operation, const QString& text,
                               const SearchOptions& options) {
        SearchEngine engine;
        engine.setSearchText(text);
        engine.setOptions(options);
        std::vector<SearchMatch> matches;
        const double ms = timeMs([&] { matches = engine.findIn(closed); });
        rows.push_back({operation, ms, QStringLiteral("%1 trafień").arg(matches.size())});
        return matches;
    };
    const std::vector<SearchMatch> shadows =
        searchRow(QStringLiteral("Szukanie słowa „shadow”"), QStringLiteral("shadow"), {});
    CHECK_FALSE(shadows.empty());
    SearchOptions wholeWord;
    wholeWord.wholeWord = true;
    searchRow(QStringLiteral("Szukanie „light” – tylko całe wyrazy"), QStringLiteral("light"),
              wholeWord);
    SearchOptions regex;
    regex.useRegex = true;
    searchRow(QStringLiteral("Szukanie wyrażenia „\\b\\w+ness\\b”"), QStringLiteral("\\b\\w+ness\\b"),
              regex);
    const std::vector<SearchMatch> inNotes = searchRow(
        QStringLiteral("Szukanie „tytuł” (tylko w przypisach dolnych)"), QStringLiteral("tytuł"), {});
    CHECK(static_cast<qsizetype>(inNotes.size()) == kml.count(QStringLiteral("<footnote ")));
    const std::vector<SearchMatch> common =
        searchRow(QStringLiteral("Szukanie słowa „the” (najczęstszego)"), QStringLiteral("the"), {});
    REQUIRE(common.size() >= static_cast<size_t>(BookSearchCoordinator::MAX_LISTED_MATCHES));

    // The panel's list: the line around each of the first matches
    QList<SearchPanelHit> hits;
    rows.push_back({QStringLiteral("Fragmenty tekstu dla listy panelu (pierwsze %1 trafień)")
                        .arg(BookSearchCoordinator::MAX_LISTED_MATCHES),
                    timeMs([&] {
                        for (int i = 0; i < BookSearchCoordinator::MAX_LISTED_MATCHES; ++i) {
                            const SearchMatch& match = common[static_cast<size_t>(i)];
                            hits.append(shownSearchHit(
                                closed[static_cast<size_t>(match.paragraph)].text,
                                match.paragraphOffset, static_cast<int>(match.length)));
                        }
                    }),
                    QString()});
    CHECK(hits.size() == BookSearchCoordinator::MAX_LISTED_MATCHES);

    // -------------------------------------------------------------------------
    // 3. Replace All in Book: a closed chapter (its KML), an open one (its editor)
    // -------------------------------------------------------------------------
    const QString kept = editor.toKml();
    int replaced = 0;
    QString changed;
    rows.push_back({QStringLiteral("Zamiana „shadow” → „cień” w zamkniętym rozdziale (KML)"),
                    timeMs([&] {
                        changed = replacedInKml(kml, QStringLiteral("shadow"),
                                                QStringLiteral("cień"), {}, &replaced);
                    }),
                    QStringLiteral("odczyt, zamiana i zapis KML")});
    CHECK(replaced == static_cast<int>(shadows.size()));
    rows.push_back({QStringLiteral("Zamiana „shadow” → „cień” w otwartym rozdziale + paintEvent"),
                    timeMs([&] {
                        replaced = editor.replaceAllInText(QStringLiteral("shadow"),
                                                           QStringLiteral("cień"), {});
                        paint(editor);
                    }),
                    QStringLiteral("jeden krok cofania")});
    CHECK(replaced == static_cast<int>(shadows.size()));
    rows.push_back({QStringLiteral("    cofnięcie (Ctrl+Z) + paintEvent"), timeMs([&] {
                        editor.undo();
                        paint(editor);
                    }),
                    QString()});
    CHECK(editor.toKml() == kept);

    report(rows, header, QStringLiteral("find_in_book_benchmark_results.md"));
}

TEST_CASE("Chapter Break benchmark: a 150k-word chapter with notes split in the middle",
          "[.][benchmark][chapterbreak]") {
    std::vector<BenchRow> rows;
    QStringList header = reportHeader();
    int notes = 0;
    const QString kml = withNotes(generateKml(150000), &notes);

    // -------------------------------------------------------------------------
    // 1. The editor: the text from the middle as KML, taken away, and back
    // -------------------------------------------------------------------------
    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.setParagraphStyles(bookStyles());
    editor.fromKml(kml);
    paint(editor);
    layoutOf(editor)->layoutPendingBlocks();
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów, %3 przypisów: dolny na końcu co "
                             "5. akapitu, końcowy co 50; podział w połowie.")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(notes);
    const CursorPosition middle{static_cast<int>(editor.paragraphCount() / 2), 0};
    editor.setCursorPosition(middle);
    paint(editor);
    const int from = editor.textDocument()->findBlockByNumber(middle.paragraph).position();
    QString title;
    rows.push_back({QStringLiteral("Tytuł nowego tekstu (nagłówek od kursora)"),
                    timeMs([&] { title = chapterBreakTitle(*editor.textDocument(), from); }),
                    title.isEmpty() ? QStringLiteral("bez nagłówka")
                                    : QStringLiteral("„%1”").arg(title)});
    const QString kept = editor.toKml();
    QString moved;
    rows.push_back({QStringLiteral("KML tekstu od kursora do końca (kmlFrom)"),
                    timeMs([&] { moved = editor.kmlFrom(middle); }),
                    QStringLiteral("%1 przypisów").arg(moved.count(QStringLiteral("<footnote ")) +
                                                       moved.count(QStringLiteral("<endnote ")))});
    CHECK_FALSE(moved.isEmpty());
    rows.push_back({QStringLiteral("Usunięcie tekstu od kursora (removeTextFrom) + paintEvent"),
                    timeMs([&] {
                        REQUIRE(editor.removeTextFrom(middle));
                        paint(editor);
                    }),
                    QStringLiteral("jeden krok cofania")});
    rows.push_back({QStringLiteral("    cofnięcie (Ctrl+Z) + paintEvent"), timeMs([&] {
                        editor.undo();
                        paint(editor);
                    }),
                    QString()});
    CHECK(editor.toKml() == kept);

    // -------------------------------------------------------------------------
    // 2. The whole command in a book: saving, the new text, its tab, and Undo
    // -------------------------------------------------------------------------
    OpenNovel book(QStringLiteral("Break"));
    book.pm.setChapterContent(book.chapterId, kml);
    REQUIRE(book.pm.saveChapterContent(book.chapterId));
    Window window;
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    text->setCursorPosition(middle);
    ChapterBreakCoordinator coordinator(window.tabs, &window.navigator, {}, {},
                                        &window.statusBar, &window.window);
    QString added;
    rows.push_back({QStringLiteral("Książka > Podział rozdziału – całość"),
                    timeMs([&] { added = coordinator.breakText(text); }),
                    QStringLiteral("zapis obu tekstów, nowy element, otwarcie jego karty")});
    REQUIRE_FALSE(added.isEmpty());
    rows.push_back({QStringLiteral("    „Cofnij” w komunikacie"),
                    timeMs([&] { CHECK(coordinator.undo()); }),
                    QStringLiteral("usunięcie nowego tekstu z plikiem, cofnięcie, zapis")});
    CHECK(book.pm.findElement(added) == nullptr);

    report(rows, header, QStringLiteral("chapter_break_benchmark_results.md"));
}
