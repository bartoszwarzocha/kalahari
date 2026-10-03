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
/// NOTE: the "Select All + copy" step overwrites the system clipboard.
///
/// Every timing goes through the real BookEditor entry points. Painting is measured with
/// QWidget::render() into a QPixmap, which calls BookEditor::paintEvent() without showing
/// a window. A full relayout is one KalahariTextDocumentLayout::blocksLaidOut() covering
/// every block of the document.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/viewport_manager.h>
#include "../editor/editor_test_utils.h"
#include "test_document_generator.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPixmap>
#include <QScreen>
#include <QTextDocument>
#include <QTextStream>
#include <QWheelEvent>

#include <functional>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;
using kalahari::benchmarks::TestDocumentGenerator;

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
    config.headingRatio = 0.0;  // <h> is not a KML paragraph; the loader would drop it
    TestDocumentGenerator generator(config);
    return generator.generateKml();
}

void paint(QWidget& widget) {
    QPixmap target(widget.size());
    widget.render(&target);
}

void typeChar(QWidget& widget, QChar c) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QString(c));
    QCoreApplication::sendEvent(&widget, &press);
}

void report(const std::vector<BenchRow>& rows, const QStringList& header) {
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
    if (path.isEmpty()) path = QStringLiteral("stage0_benchmark_results.md");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(out.toUtf8());
    }
}

}  // anonymous namespace

TEST_CASE("Stage0 benchmark: editor operations on a 150k-word document",
          "[.][benchmark][stage0]") {
    std::vector<BenchRow> rows;
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

    // -------------------------------------------------------------------------
    // 1. Load scaling: fromKml at growing sizes (quadratic => ~4x per 2x)
    // -------------------------------------------------------------------------
    for (int words : {25000, 50000, 100000, 150000}) {
        const QString kml = generateKml(words);

        KmlDocumentModel model;
        const double parseMs = timeMs([&] { model.loadKml(kml); });

        BookEditor editor;
        resizeWidget(editor, QSize(1000, 800));
        const double loadMs = timeMs([&] { editor.fromKml(kml); });

        rows.push_back({QStringLiteral("fromKml – %1k słów").arg(words / 1000), loadMs,
                        QStringLiteral("%1 akapitów; w tym parsowanie KmlDocumentModel %2 ms")
                            .arg(editor.paragraphCount())
                            .arg(parseMs, 0, 'f', 1)});
    }

    // -------------------------------------------------------------------------
    // 2. Operations on the full 150k document
    // -------------------------------------------------------------------------
    const QString kml = generateKml(150000);
    BookEditor editor;
    resizeWidget(editor, QSize(1000, 800));
    editor.fromKml(kml);
    header << QStringLiteral("Dokument: %1 słów, %2 akapitów, %3 znaków")
                  .arg(editor.wordCount())
                  .arg(editor.paragraphCount())
                  .arg(editor.characterCount());

    paint(editor);  // warm-up
    rows.push_back({QStringLiteral("paintEvent – Continuous (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0, QString()});

    // Resize (width change) + count full relayouts. The widget is hidden, so the width is
    // applied at once (a visible editor waits for the resize to settle).
    {
        FullRelayoutCounter relayouts(editor);
        const double ms = timeMs([&] { resizeWidget(editor, QSize(1200, 800)); });
        rows.push_back({QStringLiteral("Zmiana szerokości 1000→1200 px"), ms,
                        QStringLiteral("pełnych przełożeń dokumentu: %1").arg(relayouts.count())});
        rows.push_back({QStringLiteral("paintEvent po zmianie szerokości"),
                        timeMs([&] { paint(editor); }),
                        QStringLiteral("pełnych przełożeń łącznie: %1").arg(relayouts.count())});
    }

    // Zoom in Continuous mode
    {
        FullRelayoutCounter relayouts(editor);
        const double ms = timeMs([&] { editor.setZoomFactor(1.25); });
        rows.push_back({QStringLiteral("Zmiana powiększenia 100→125% (Continuous)"), ms,
                        QStringLiteral("pełnych przełożeń dokumentu: %1").arg(relayouts.count())});
        rows.push_back({QStringLiteral("paintEvent po zmianie powiększenia"),
                        timeMs([&] { paint(editor); }), QString()});
        editor.setZoomFactor(1.0);
        paint(editor);
    }

    // Ctrl+wheel zoom: five notches in a row, applied once the wheel stops
    {
        FullRelayoutCounter relayouts(editor);
        const double wheelMs = timeMs([&] {
            for (int notch = 0; notch < 5; ++notch) {
                QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, 120),
                                  Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
                QCoreApplication::sendEvent(&editor, &wheel);
            }
        });
        const int duringWheel = relayouts.count();
        QElapsedTimer settle;
        settle.start();
        while (relayouts.count() == duringWheel && settle.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        rows.push_back({QStringLiteral("Ctrl+kółko – 5 ząbków (Continuous)"), wheelMs,
                        QStringLiteral("pełnych przełożeń w trakcie: %1, po ustaniu: %2")
                            .arg(duringWheel)
                            .arg(relayouts.count() - duringWheel)});
        editor.setZoomFactor(1.0);
        paint(editor);
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
                    timeMs([&] { paint(editor); }), QString()});
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
    editor.setViewMode(ViewMode::DistractionFree);
    paint(editor);
    rows.push_back({QStringLiteral("paintEvent – DistractionFree (średnio z 10)"),
                    timeMs([&] { for (int i = 0; i < 10; ++i) paint(editor); }) / 10.0,
                    QStringLiteral("porównaj z Continuous")});
    editor.setViewMode(ViewMode::Continuous);

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

    // Select All + copy
    rows.push_back({QStringLiteral("Select All"), timeMs([&] { editor.selectAll(); }), QString()});
    rows.push_back({QStringLiteral("Copy (cały dokument)"), timeMs([&] { editor.copy(); }),
                    QStringLiteral("nadpisuje schowek systemowy")});
    rows.push_back({QStringLiteral("paintEvent z zaznaczonym całym dokumentem"),
                    timeMs([&] { paint(editor); }), QString()});

    report(rows, header);
    CHECK(editor.paragraphCount() > 0);
}
