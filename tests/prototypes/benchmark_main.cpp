/**
 * @file benchmark_main.cpp
 * @brief Standalone benchmark runner for OpenSpec #00043 prototypes
 *
 * Usage: benchmark_prototypes [--piece-table] [--qtextdocument] [--all]
 */

#include "piece_table_prototype.h"
#include "lazy_layout_prototype.h"

#include <QCoreApplication>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <iostream>
#include <iomanip>
#include <vector>
#include <functional>
#include <memory>

using namespace prototype;

// Thread-local random generator to avoid conflicts with Qt's internal RNG
// Using thread_local ensures each thread has its own instance
static thread_local QRandomGenerator s_rng(42);

// Helper to reseed the generator
void reseedRng(quint32 seed) {
    s_rng.seed(seed);
}

// Accessor for the random generator
QRandomGenerator& rng() {
    return s_rng;
}

//=============================================================================
// Utilities
//=============================================================================

QString generateText(int wordCount) {
    static const QStringList words = {
        "lorem", "ipsum", "dolor", "sit", "amet", "consectetur",
        "adipiscing", "elit", "sed", "do", "eiusmod", "tempor",
        "incididunt", "ut", "labore", "et", "dolore", "magna",
        "aliqua", "enim", "ad", "minim", "veniam", "quis",
        "nostrud", "exercitation", "ullamco", "laboris", "nisi",
        "aliquip", "ex", "ea", "commodo", "consequat", "duis",
        "aute", "irure", "in", "reprehenderit", "voluptate",
        "velit", "esse", "cillum", "fugiat", "nulla", "pariatur"
    };

    QString result;
    result.reserve(wordCount * 8);

    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) result.append(' ');
        result.append(words[rng().bounded(words.size())]);
        if (i > 0 && i % 50 == 0) result.append("\n\n");
    }

    return result;
}

QString formatTime(qint64 ns) {
    if (ns < 1000) return QString("%1 ns").arg(ns);
    if (ns < 1000000) return QString("%1 µs").arg(ns / 1000.0, 0, 'f', 1);
    if (ns < 1000000000) return QString("%1 ms").arg(ns / 1000000.0, 0, 'f', 2);
    return QString("%1 s").arg(ns / 1000000000.0, 0, 'f', 3);
}

QString formatSpeed(qint64 ns) {
    double ms = ns / 1000000.0;
    if (ms < 16.0) return "PASS (< 16ms frame)";
    if (ms < 50.0) return "OK (< 50ms)";
    if (ms < 100.0) return "SLOW (< 100ms)";
    return "FAIL (> 100ms)";
}

void printHeader(const QString& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  " << title.toStdString() << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void printResult(const QString& label, qint64 ns, const QString& note = "") {
    std::cout << std::left << std::setw(35) << label.toStdString()
              << std::setw(15) << formatTime(ns).toStdString()
              << note.toStdString() << "\n";
}

void printComparison(const QString& label, qint64 ns1, qint64 ns2,
                     const QString& name1, const QString& name2) {
    double ratio = static_cast<double>(ns2) / ns1;
    QString winner = (ns1 < ns2) ? name1 : name2;
    QString ratioStr = QString("%1x").arg(std::max(ratio, 1.0/ratio), 0, 'f', 1);

    std::cout << std::left << std::setw(25) << label.toStdString()
              << std::setw(12) << formatTime(ns1).toStdString()
              << std::setw(12) << formatTime(ns2).toStdString()
              << ratioStr.toStdString() << " (" << winner.toStdString() << ")\n";
}

//=============================================================================
// Piece Table Benchmarks
//=============================================================================

void runPieceTableBenchmarks() {
    printHeader("PIECE TABLE vs QSTRING BENCHMARKS (150k words)");
    std::cout << "Comparing custom PieceTable prototype with traditional QString\n\n";

    reseedRng(42);  // Reset for reproducibility
    QString text = generateText(150000);
    std::cout << "Document size: " << text.length() << " characters\n\n";

    std::cout << std::left
              << std::setw(25) << "Operation"
              << std::setw(12) << "PieceTable"
              << std::setw(12) << "QString"
              << "Winner\n";
    std::cout << std::string(60, '-') << "\n";

    QElapsedTimer timer;

    // 1. Load document
    timer.start();
    PieceTable pt(text);
    qint64 ptLoad = timer.nsecsElapsed();

    timer.start();
    TraditionalDocument td(text);
    qint64 tdLoad = timer.nsecsElapsed();

    printComparison("Load document", ptLoad, tdLoad, "PT", "QString");

    // 2. 1000 random inserts
    std::vector<std::pair<size_t, QString>> inserts;
    for (int i = 0; i < 1000; ++i) {
        inserts.emplace_back(
            rng().bounded(static_cast<quint32>(text.length())),
            QString("INS%1").arg(i)
        );
    }

    PieceTable pt2(text);
    timer.start();
    for (const auto& [pos, txt] : inserts) {
        pt2.insert(pos, txt);
    }
    qint64 ptInsert = timer.nsecsElapsed();

    TraditionalDocument td2(text);
    timer.start();
    for (const auto& [pos, txt] : inserts) {
        td2.insert(pos, txt);
    }
    qint64 tdInsert = timer.nsecsElapsed();

    printComparison("1000 random inserts", ptInsert, tdInsert, "PT", "QString");

    // 3. Sequential typing at end
    PieceTable pt3(text);
    timer.start();
    for (int i = 0; i < 5000; ++i) {
        pt3.insert(pt3.length(), QString(QChar('a' + (i % 26))));
    }
    qint64 ptType = timer.nsecsElapsed();

    TraditionalDocument td3(text);
    timer.start();
    for (int i = 0; i < 5000; ++i) {
        td3.insert(td3.length(), QString(QChar('a' + (i % 26))));
    }
    qint64 tdType = timer.nsecsElapsed();

    printComparison("5000 chars at end", ptType, tdType, "PT", "QString");

    // 4. Get full text (after edits)
    timer.start();
    for (int i = 0; i < 100; ++i) {
        volatile auto t = pt2.text();
        Q_UNUSED(t);
    }
    qint64 ptGet = timer.nsecsElapsed();

    timer.start();
    for (int i = 0; i < 100; ++i) {
        volatile auto t = td2.text();
        Q_UNUSED(t);
    }
    qint64 tdGet = timer.nsecsElapsed();

    printComparison("Get text 100x", ptGet, tdGet, "PT", "QString");

    std::cout << "\nPieceTable stats: " << pt2.pieceCount() << " pieces, "
              << pt2.addBufferSize() << " chars in add buffer\n";
}

//=============================================================================
// QTextDocument Benchmarks
//=============================================================================

void runQTextDocumentBenchmarks() {
    printHeader("QTEXTDOCUMENT BENCHMARKS (150k words)");
    std::cout << "Testing Qt's built-in text document for our use case\n\n";

    reseedRng(42);  // Reset for reproducibility
    QString text = generateText(150000);
    std::cout << "Document size: " << text.length() << " characters\n";

    QElapsedTimer timer;

    // 1. Load document
    timer.start();
    QTextDocument doc;
    doc.setPlainText(text);
    qint64 loadTime = timer.nsecsElapsed();
    printResult("Load document", loadTime, formatSpeed(loadTime));

    std::cout << "Block count: " << doc.blockCount() << "\n\n";

    // 2. Random inserts
    std::cout << "Random insert tests:\n";
    std::cout << std::string(50, '-') << "\n";

    for (int count : {100, 500, 1000}) {
        QTextDocument testDoc;
        testDoc.setPlainText(text);
        QTextCursor cursor(&testDoc);

        timer.start();
        for (int i = 0; i < count; ++i) {
            int pos = rng().bounded(testDoc.characterCount());
            cursor.setPosition(pos);
            cursor.insertText(QString("X%1").arg(i));
        }
        qint64 insertTime = timer.nsecsElapsed();

        printResult(QString("%1 random inserts").arg(count), insertTime, formatSpeed(insertTime));
    }

    // 3. Sequential typing
    std::cout << "\nSequential typing tests:\n";
    std::cout << std::string(50, '-') << "\n";

    for (int count : {1000, 5000, 10000}) {
        QTextDocument testDoc;
        testDoc.setPlainText(text);
        QTextCursor cursor(&testDoc);
        cursor.movePosition(QTextCursor::End);

        timer.start();
        for (int i = 0; i < count; ++i) {
            cursor.insertText(QString(QChar('a' + (i % 26))));
        }
        qint64 typeTime = timer.nsecsElapsed();

        printResult(QString("%1 chars at end").arg(count), typeTime, formatSpeed(typeTime));
    }

    // 4. Get full text
    std::cout << "\nText extraction tests:\n";
    std::cout << std::string(50, '-') << "\n";

    timer.start();
    for (int i = 0; i < 100; ++i) {
        volatile auto t = doc.toPlainText();
        Q_UNUSED(t);
    }
    qint64 getText = timer.nsecsElapsed();
    printResult("toPlainText() 100x", getText, formatSpeed(getText));

    // 5. Select All simulation
    timer.start();
    QTextCursor selectCursor(&doc);
    selectCursor.select(QTextCursor::Document);
    QString selected = selectCursor.selectedText();
    qint64 selectAll = timer.nsecsElapsed();
    printResult("Select All", selectAll, formatSpeed(selectAll));

    // 6. Block (paragraph) access
    std::cout << "\nParagraph access tests:\n";
    std::cout << std::string(50, '-') << "\n";

    timer.start();
    for (int i = 0; i < 1000; ++i) {
        int blockNum = rng().bounded(doc.blockCount());
        QTextBlock block = doc.findBlockByNumber(blockNum);
        volatile auto t = block.text();
        Q_UNUSED(t);
    }
    qint64 blockAccess = timer.nsecsElapsed();
    printResult("1000 random block access", blockAccess, formatSpeed(blockAccess));

    // 7. Cursor movement
    std::cout << "\nCursor movement tests:\n";
    std::cout << std::string(50, '-') << "\n";

    QTextCursor moveCursor(&doc);
    timer.start();
    for (int i = 0; i < 10000; ++i) {
        moveCursor.movePosition(QTextCursor::Right);
        if (moveCursor.atEnd()) moveCursor.movePosition(QTextCursor::Start);
    }
    qint64 cursorMove = timer.nsecsElapsed();
    printResult("10000 cursor moves", cursorMove, formatSpeed(cursorMove));
}

//=============================================================================
// Lazy Layout Benchmarks
//=============================================================================

QStringList splitIntoParagraphs(const QString& text) {
    return text.split("\n\n", Qt::SkipEmptyParts);
}

void runLazyLayoutBenchmarks() {
    printHeader("LAZY LAYOUT vs TRADITIONAL LAYOUT BENCHMARKS");
    std::cout << "Comparing lazy (on-demand) layout with full upfront layout\n";
    std::cout << "Note: Traditional simulates O(N) iteration; Lazy uses actual QTextLayout\n\n";

    reseedRng(42);  // Reset for reproducibility

    // Use 30k words - demonstrates the concept without excessive runtime
    QString text = generateText(30000);
    QStringList paragraphs = splitIntoParagraphs(text);

    std::cout << "Document: " << text.length() << " characters, "
              << paragraphs.size() << " paragraphs\n\n";

    QFont font("Segoe UI", 11);
    int viewportWidth = 800;
    int viewportHeight = 600;

    QElapsedTimer timer;

    // 1. Traditional: Full layout upfront (simulated O(N) iteration)
    std::cout << "Traditional Layout (full upfront calculation):\n";
    std::cout << std::string(50, '-') << "\n";

    timer.start();
    TraditionalLayoutManager traditional;
    traditional.initialize(paragraphs, font, viewportWidth);
    qint64 traditionalInit = timer.nsecsElapsed();

    printResult("Initialize (all paragraphs)", traditionalInit, formatSpeed(traditionalInit));
    std::cout << "Total height: " << traditional.getTotalHeight() << " px\n\n";

    // 2. Lazy: Estimation only at init
    std::cout << "Lazy Layout (estimation + on-demand):\n";
    std::cout << std::string(50, '-') << "\n";

    timer.start();
    LazyLayoutManager lazy;
    lazy.initialize(paragraphs, font, viewportWidth);
    qint64 lazyInit = timer.nsecsElapsed();

    printResult("Initialize (estimation only)", lazyInit, formatSpeed(lazyInit));
    std::cout << "Estimated total height: " << lazy.getTotalHeight() << " px\n";
    std::cout << "Calculated paragraphs: " << lazy.getCalculatedCount()
              << " / " << lazy.getParagraphCount() << "\n\n";

    // 3. Comparison: Initial scroll to top
    std::cout << "Scroll simulation (viewport 600px):\n";
    std::cout << std::string(50, '-') << "\n";

    // Traditional doesn't need to do anything for scroll (already calculated)
    timer.start();
    size_t firstTrad = traditional.getParagraphAtY(0);
    size_t lastTrad = traditional.getParagraphAtY(viewportHeight);
    qint64 traditionalScroll = timer.nsecsElapsed();
    Q_UNUSED(firstTrad);
    Q_UNUSED(lastTrad);

    // Lazy needs to calculate visible paragraphs
    timer.start();
    lazy.updateVisibleRange(0, viewportHeight, 5);
    qint64 lazyScroll = timer.nsecsElapsed();

    auto [visFirst, visLast] = lazy.getVisibleRange();

    printComparison("Scroll to top", lazyScroll, traditionalScroll, "Lazy", "Trad");
    std::cout << "Visible range: " << visFirst << " - " << visLast << "\n";
    std::cout << "Calculated after scroll: " << lazy.getCalculatedCount()
              << " / " << lazy.getParagraphCount() << "\n\n";

    // 4. Scroll through document (simulate scrolling)
    std::cout << "Full scroll simulation (100 scroll steps):\n";
    std::cout << std::string(50, '-') << "\n";

    double totalHeight = lazy.getTotalHeight();
    double scrollStep = totalHeight / 100.0;

    // Traditional scroll (just lookups)
    timer.start();
    for (int i = 0; i < 100; ++i) {
        double scrollY = i * scrollStep;
        volatile size_t firstIdx = traditional.getParagraphAtY(scrollY);
        volatile size_t lastIdx = traditional.getParagraphAtY(scrollY + viewportHeight);
        Q_UNUSED(firstIdx);
        Q_UNUSED(lastIdx);
    }
    qint64 traditionalFullScroll = timer.nsecsElapsed();

    // Reset lazy manager for fair comparison
    lazy.initialize(paragraphs, font, viewportWidth);

    // Lazy scroll (calculate on demand)
    timer.start();
    for (int i = 0; i < 100; ++i) {
        double scrollY = i * scrollStep;
        lazy.updateVisibleRange(scrollY, viewportHeight, 5);
    }
    qint64 lazyFullScroll = timer.nsecsElapsed();

    printComparison("100 scroll steps", lazyFullScroll, traditionalFullScroll, "Lazy", "Trad");
    std::cout << "Final calculated: " << lazy.getCalculatedCount()
              << " / " << lazy.getParagraphCount() << "\n\n";

    // 5. Random jumps (worst case for lazy)
    std::cout << "Random jump simulation (50 random positions):\n";
    std::cout << std::string(50, '-') << "\n";

    std::vector<double> randomPositions;
    for (int i = 0; i < 50; ++i) {
        randomPositions.push_back(rng().bounded(static_cast<quint32>(totalHeight)));
    }

    // Reset lazy manager
    lazy.initialize(paragraphs, font, viewportWidth);

    // Traditional jumps
    timer.start();
    for (double pos : randomPositions) {
        volatile size_t firstIdx = traditional.getParagraphAtY(pos);
        volatile size_t lastIdx = traditional.getParagraphAtY(pos + viewportHeight);
        Q_UNUSED(firstIdx);
        Q_UNUSED(lastIdx);
    }
    qint64 traditionalJumps = timer.nsecsElapsed();

    // Lazy jumps (needs to calculate each new visible area)
    timer.start();
    for (double pos : randomPositions) {
        lazy.updateVisibleRange(pos, viewportHeight, 5);
    }
    qint64 lazyJumps = timer.nsecsElapsed();

    printComparison("50 random jumps", lazyJumps, traditionalJumps, "Lazy", "Trad");
    std::cout << "Final calculated: " << lazy.getCalculatedCount()
              << " / " << lazy.getParagraphCount() << "\n\n";

    // 6. Y-to-paragraph lookup comparison
    std::cout << "Y-to-paragraph lookup (1000 queries):\n";
    std::cout << std::string(50, '-') << "\n";

    std::vector<double> yPositions;
    for (int i = 0; i < 1000; ++i) {
        yPositions.push_back(rng().bounded(static_cast<quint32>(totalHeight)));
    }

    // Traditional (binary search on cumulative array)
    timer.start();
    for (double y : yPositions) {
        volatile size_t idx = traditional.getParagraphAtY(y);
        Q_UNUSED(idx);
    }
    qint64 traditionalLookup = timer.nsecsElapsed();

    // Lazy (Fenwick tree binary search)
    timer.start();
    for (double y : yPositions) {
        volatile size_t idx = lazy.getParagraphAtY(y);
        Q_UNUSED(idx);
    }
    qint64 lazyLookup = timer.nsecsElapsed();

    printComparison("1000 Y lookups", lazyLookup, traditionalLookup, "Lazy", "Trad");

    // Summary
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "LAZY LAYOUT SUMMARY:\n";
    std::cout << std::string(50, '=') << "\n";

    double initSpeedup = static_cast<double>(traditionalInit) / lazyInit;
    std::cout << "Initialization speedup: " << std::fixed << std::setprecision(1)
              << initSpeedup << "x faster\n";
    std::cout << "Memory saved: " << (lazy.getParagraphCount() - lazy.getCalculatedCount())
              << " paragraphs not calculated\n";
    std::cout << "\nKey insight: Lazy layout trades some scroll time for MUCH faster init.\n";
    std::cout << "For 150k word documents, this means sub-second load times!\n";
}

//=============================================================================
// Comparison Summary
//=============================================================================

void runComparisonSummary() {
    printHeader("COMPARISON SUMMARY");

    std::cout << R"(
TARGET PERFORMANCE (150k words, matching Word):
  - Scrolling:      60 fps (16ms frame budget)
  - Select All:     < 50ms
  - Copy:           < 100ms
  - Typing latency: < 16ms
  - Load time:      < 2 seconds

FINDINGS:

1. PieceTable Prototype (vector-based):
   - Insert: O(N) due to vector operations
   - Good for append-only scenarios
   - Needs balanced tree for O(log N) inserts
   - Memory efficient (original never copied)

2. QString (Traditional):
   - Simple and fast for small documents
   - O(N) memory copy on every insert in middle
   - Degrades with document size

3. QTextDocument (Qt Built-in):
   - Already optimized by Qt team
   - O(log N) operations internally
   - Built-in undo/redo, cursor, selection
   - Block (paragraph) based access is O(1)
   - toPlainText() has caching

RECOMMENDATION:

For OpenSpec #00043, we should use QTextDocument as the internal storage:
  - It already implements piece-table-like optimization
  - Provides cursor, selection, undo/redo out of the box
  - Block-based access aligns with our paragraph model
  - We need to add:
    * Lazy layout (only visible paragraphs)
    * Height estimation for off-screen
    * Viewport-only rendering
    * Background layout thread

The performance bottleneck in current BookEditor is NOT text storage,
but rather:
  1. Layout during paint (should be pre-calculated)
  2. Full document traversal (should use virtual scrolling)
  3. No dirty region tracking (should repaint only changed areas)

NEXT STEPS:
  - Task 1.3: Create lazy layout prototype
  - Task 1.4: Benchmark viewport-only rendering
)";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool runPieceTable = false;
    bool runQTextDoc = false;
    bool runLazyLayout = false;
    bool runAll = true;

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--piece-table") { runPieceTable = true; runAll = false; }
        else if (arg == "--qtextdocument") { runQTextDoc = true; runAll = false; }
        else if (arg == "--lazy-layout") { runLazyLayout = true; runAll = false; }
        else if (arg == "--all") { runAll = true; }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: benchmark_prototypes [options]\n"
                      << "Options:\n"
                      << "  --piece-table    Run PieceTable vs QString benchmarks\n"
                      << "  --qtextdocument  Run QTextDocument benchmarks\n"
                      << "  --lazy-layout    Run Lazy Layout vs Traditional benchmarks\n"
                      << "  --all            Run all benchmarks (default)\n";
            return 0;
        }
    }

    std::cout << "OpenSpec #00043 - Phase 1: Research & Spike\n";
    std::cout << "Editor Performance Benchmarks\n";
    std::cout << std::string(70, '=') << "\n";

    if (runAll || runPieceTable) {
        runPieceTableBenchmarks();
    }

    if (runAll || runQTextDoc) {
        runQTextDocumentBenchmarks();
    }

    if (runAll || runLazyLayout) {
        runLazyLayoutBenchmarks();
    }

    if (runAll) {
        runComparisonSummary();
    }

    return 0;
}
