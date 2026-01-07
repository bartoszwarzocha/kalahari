/// @file editor_benchmark.cpp
/// @brief Performance benchmarks for BookEditor (OpenSpec #00043)

#include <kalahari/editor/editor_benchmark.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <algorithm>
#include <numeric>

namespace kalahari {
namespace editor {

// =============================================================================
// BenchmarkResult
// =============================================================================

QString BenchmarkResult::toString() const
{
    return QString("%1: %2 ops in %3 ms (avg: %4 ms/op, %5 ops/sec, min: %6 ms, max: %7 ms)")
        .arg(name)
        .arg(iterations)
        .arg(totalMs, 0, 'f', 2)
        .arg(avgMs, 0, 'f', 4)
        .arg(opsPerSecond, 0, 'f', 0)
        .arg(minMs, 0, 'f', 4)
        .arg(maxMs, 0, 'f', 4);
}

// =============================================================================
// EditorBenchmark
// =============================================================================

EditorBenchmark::EditorBenchmark(BookEditor* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
{
}

// =============================================================================
// Individual Benchmarks
// =============================================================================

BenchmarkResult EditorBenchmark::benchCursorLeftRight()
{
    return runBenchmark(
        QStringLiteral("Cursor Left/Right"),
        [this]() {
            // Setup: Use SAME structure as Up/Down (200 short paragraphs)
            // This isolates whether slowness is from movement type or document structure
            QString text;
            for (int i = 0; i < 200; ++i) {
                text += QStringLiteral("Line %1: This is test content for cursor navigation.\n").arg(i);
            }
            prepareEditor(text);
            m_editor->setCursorPosition({100, 20});  // Same as Up/Down benchmark
        },
        [this]() {
            // Alternate left and right
            static bool goLeft = true;
            if (goLeft) {
                m_editor->moveCursorLeft();
            } else {
                m_editor->moveCursorRight();
            }
            goLeft = !goLeft;
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchCursorUpDown()
{
    return runBenchmark(
        QStringLiteral("Cursor Up/Down"),
        [this]() {
            // Setup: Create multiple paragraphs
            QString text;
            for (int i = 0; i < 200; ++i) {
                text += QStringLiteral("Line %1: This is test content for up/down navigation.\n").arg(i);
            }
            prepareEditor(text);

            // FORCE layout creation - Qt layouts are lazy, created during paint
            // We need to ensure the document is fully laid out before benchmarking
            QTextDocument* doc = m_editor->textDocument();
            if (doc) {
                // Method 1: Set page size to force layout computation
                doc->setPageSize(QSizeF(m_editor->width() - 40, 100000));

                // Method 2: Request document size - requires full layout
                if (doc->documentLayout()) {
                    doc->documentLayout()->documentSize();
                }

                // Method 3: Force layout on individual blocks we'll navigate
                for (int i = 99; i <= 101 && i < doc->blockCount(); ++i) {
                    QTextBlock block = doc->findBlockByNumber(i);
                    if (block.isValid() && block.layout()) {
                        // If layout exists but has no lines, force line creation
                        if (block.layout()->lineCount() == 0) {
                            block.layout()->beginLayout();
                            while (true) {
                                QTextLine line = block.layout()->createLine();
                                if (!line.isValid()) break;
                                line.setLineWidth(m_editor->width() - 40);
                            }
                            block.layout()->endLayout();
                        }
                    }
                }
            }

            // Position cursor at paragraph 100
            m_editor->setCursorPosition({100, 20});
            m_editor->repaint();
            QApplication::processEvents();

            // Debug: Log initial state and verify layout
            auto& logger = core::Logger::getInstance();
            auto pos = m_editor->cursorPosition();
            if (doc) {
                QTextBlock block = doc->findBlockByNumber(100);
                int lineCount = (block.isValid() && block.layout()) ? block.layout()->lineCount() : -1;
                logger.info("Up/Down setup: cursor at ({},{}), block 100 lineCount={}",
                    pos.paragraph, pos.offset, lineCount);
            }
        },
        [this]() {
            // Alternate up and down - with position tracking
            static bool goUp = true;
            static int debugCount = 0;
            auto posBefore = m_editor->cursorPosition();

            if (goUp) {
                m_editor->moveCursorUp();
            } else {
                m_editor->moveCursorDown();
            }
            goUp = !goUp;

            // Log first few position changes
            if (debugCount < 5) {
                auto posAfter = m_editor->cursorPosition();
                auto& logger = core::Logger::getInstance();
                logger.info("Up/Down #{}: ({},{}) -> ({},{})",
                    debugCount, posBefore.paragraph, posBefore.offset,
                    posAfter.paragraph, posAfter.offset);
                debugCount++;
            }
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchCursorWordNavigation()
{
    return runBenchmark(
        QStringLiteral("Cursor Word Navigation"),
        [this]() {
            // Setup: Create text with many words
            QString text;
            for (int i = 0; i < 50; ++i) {
                text += QStringLiteral("word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 ");
            }
            prepareEditor(text);
            m_editor->setCursorPosition({0, static_cast<int>(text.length() / 2)});
        },
        [this]() {
            // Alternate word left and word right
            static bool goLeft = true;
            if (goLeft) {
                m_editor->moveCursorWordLeft();
            } else {
                m_editor->moveCursorWordRight();
            }
            goLeft = !goLeft;
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchCharacterInsertion()
{
    return runBenchmark(
        QStringLiteral("Character Insertion"),
        [this]() {
            // Setup: Start with some text
            prepareEditor(QStringLiteral("Initial text for insertion benchmark. "));
            m_editor->setCursorPosition({0, 0});
        },
        [this]() {
            // Insert single character
            m_editor->insertText(QStringLiteral("x"));
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchBackspaceDeletion()
{
    return runBenchmark(
        QStringLiteral("Backspace Deletion"),
        [this]() {
            // Setup: Create text to delete
            QString text(m_iterations + m_warmupIterations + 100, 'x');
            prepareEditor(text);
            m_editor->setCursorPosition({0, static_cast<int>(text.length())});
        },
        [this]() {
            m_editor->deleteBackward();
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchDeleteKeyDeletion()
{
    return runBenchmark(
        QStringLiteral("Delete Key Deletion"),
        [this]() {
            // Setup: Create text to delete
            QString text(m_iterations + m_warmupIterations + 100, 'x');
            prepareEditor(text);
            m_editor->setCursorPosition({0, 0});
        },
        [this]() {
            m_editor->deleteForward();
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchRapidTyping()
{
    // Simulate typing at ~60 WPM (5 chars/word, 300 chars/min, 5 chars/sec)
    // We're measuring how fast the editor CAN handle typing, not limiting to 60 WPM
    return runBenchmark(
        QStringLiteral("Rapid Typing (simulated)"),
        [this]() {
            prepareEditor(QString());
            m_editor->setCursorPosition({0, 0});
        },
        [this]() {
            // Cycle through common characters
            static const QString chars = QStringLiteral("The quick brown fox jumps over the lazy dog. ");
            static int charIndex = 0;
            m_editor->insertText(QString(chars[charIndex % chars.length()]));
            charIndex++;
        },
        [this]() {
            restoreEditor();
        });
}

BenchmarkResult EditorBenchmark::benchScrolling()
{
    return runBenchmark(
        QStringLiteral("Scrolling"),
        [this]() {
            // Setup: Create large document
            QString text;
            for (int i = 0; i < 1000; ++i) {
                text += QStringLiteral("Line %1: This is content for scroll benchmarking.\n").arg(i);
            }
            prepareEditor(text);
            m_editor->setScrollOffset(0);
        },
        [this]() {
            // Scroll up and down
            static double scrollPos = 0;
            static bool scrollDown = true;
            if (scrollDown) {
                scrollPos += 20;
                if (scrollPos > 5000) scrollDown = false;
            } else {
                scrollPos -= 20;
                if (scrollPos < 0) scrollDown = true;
            }
            m_editor->setScrollOffset(scrollPos);
        },
        [this]() {
            restoreEditor();
        });
}

// =============================================================================
// Batch Operations
// =============================================================================

std::vector<BenchmarkResult> EditorBenchmark::runAll()
{
    m_lastResults.clear();

    auto& logger = core::Logger::getInstance();
    logger.info("========================================");
    logger.info("EDITOR BENCHMARK - Running all benchmarks");
    logger.info("Iterations: {}, Warmup: {}", m_iterations, m_warmupIterations);
    logger.info("========================================");

    // Run cursor benchmarks
    auto cursorResults = runCursorBenchmarks();
    m_lastResults.insert(m_lastResults.end(), cursorResults.begin(), cursorResults.end());

    // Run editing benchmarks
    auto editResults = runEditingBenchmarks();
    m_lastResults.insert(m_lastResults.end(), editResults.begin(), editResults.end());

    // Run scroll benchmark
    m_lastResults.push_back(benchScrolling());

    // Summary
    logger.info("========================================");
    logger.info("BENCHMARK SUMMARY");
    logger.info("========================================");
    for (const auto& result : m_lastResults) {
        double refOps = referenceOpsPerSecond(result.name);
        QString comparison;
        if (refOps > 0) {
            double ratio = result.opsPerSecond / refOps;
            if (ratio >= 1.0) {
                comparison = QString(" [OK: %.1fx reference]").arg(ratio);
            } else {
                comparison = QString(" [SLOW: %.1fx reference]").arg(ratio);
            }
        }
        logger.info("{}{}", result.toString().toStdString(), comparison.toStdString());
    }
    logger.info("========================================");

    return m_lastResults;
}

std::vector<BenchmarkResult> EditorBenchmark::runCursorBenchmarks()
{
    std::vector<BenchmarkResult> results;

    results.push_back(benchCursorLeftRight());
    results.push_back(benchCursorUpDown());
    results.push_back(benchCursorWordNavigation());

    return results;
}

std::vector<BenchmarkResult> EditorBenchmark::runEditingBenchmarks()
{
    std::vector<BenchmarkResult> results;

    results.push_back(benchCharacterInsertion());
    results.push_back(benchBackspaceDeletion());
    results.push_back(benchDeleteKeyDeletion());
    results.push_back(benchRapidTyping());

    return results;
}

// =============================================================================
// Reference Values
// =============================================================================

double EditorBenchmark::referenceOpsPerSecond(const QString& benchmarkName)
{
    // Reference values based on achieved performance with 30% headroom
    //
    // These are AMBITIOUS targets based on actual benchmark results:
    // - Cursor Left/Right achieved: ~3,000 ops/sec
    // - Cursor Up/Down achieved: ~2,200 ops/sec
    // - Cursor Word Nav achieved: ~2,150 ops/sec
    // - Character Insertion achieved: ~1,380 ops/sec
    // - Backspace Deletion achieved: ~1,320 ops/sec
    // - Delete Key Deletion achieved: ~1,740 ops/sec
    // - Rapid Typing achieved: ~1,590 ops/sec
    // - Scrolling achieved: ~3,350 ops/sec
    //
    // Reference = ~70% of achieved (leaves 30% headroom for system variations)
    // Below these = performance regression detected!
    //
    static const QMap<QString, double> references = {
        // Cursor operations: ~70% of achieved performance
        {QStringLiteral("Cursor Left/Right"), 2000},       // achieved ~3,000
        {QStringLiteral("Cursor Up/Down"), 1500},          // achieved ~2,200
        {QStringLiteral("Cursor Word Navigation"), 1500},  // achieved ~2,150

        // Editing operations: ~70% of achieved performance
        {QStringLiteral("Character Insertion"), 1000},     // achieved ~1,380
        {QStringLiteral("Backspace Deletion"), 900},       // achieved ~1,320
        {QStringLiteral("Delete Key Deletion"), 1200},     // achieved ~1,740
        {QStringLiteral("Rapid Typing (simulated)"), 1100},// achieved ~1,590

        // Scrolling: ~70% of achieved performance
        {QStringLiteral("Scrolling"), 2300}                // achieved ~3,350
    };

    return references.value(benchmarkName, 0);
}

// =============================================================================
// Private Methods
// =============================================================================

BenchmarkResult EditorBenchmark::runBenchmark(
    const QString& name,
    std::function<void()> setup,
    std::function<void()> operation,
    std::function<void()> teardown)
{
    auto& logger = core::Logger::getInstance();
    logger.info("Starting benchmark: {}", name.toStdString());

    emit benchmarkStarted(name);

    // Run setup
    if (setup) {
        setup();
    }

    // Process events to ensure editor is ready
    QApplication::processEvents();

    // Warmup iterations (not measured)
    for (int i = 0; i < m_warmupIterations; ++i) {
        operation();
        if (i % 10 == 0) {
            QApplication::processEvents();
        }
    }

    // Benchmark iterations
    std::vector<double> times;
    times.reserve(m_iterations);

    QElapsedTimer totalTimer;
    QElapsedTimer opTimer;

    totalTimer.start();

    for (int i = 0; i < m_iterations; ++i) {
        opTimer.start();
        operation();
        times.push_back(opTimer.nsecsElapsed() / 1000000.0);  // Convert to ms

        // Process events periodically to keep UI responsive
        if (i % 100 == 0) {
            QApplication::processEvents();
            emit progressUpdated(i, m_iterations);
        }
    }

    double totalMs = totalTimer.nsecsElapsed() / 1000000.0;

    // Calculate statistics
    BenchmarkResult result;
    result.name = name;
    result.iterations = m_iterations;
    result.totalMs = totalMs;
    result.avgMs = totalMs / m_iterations;
    result.opsPerSecond = m_iterations / (totalMs / 1000.0);
    result.minMs = *std::min_element(times.begin(), times.end());
    result.maxMs = *std::max_element(times.begin(), times.end());

    // Teardown
    if (teardown) {
        teardown();
    }

    logger.info("  Result: {}", result.toString().toStdString());

    emit benchmarkCompleted(result);

    return result;
}

void EditorBenchmark::prepareEditor(const QString& textContent)
{
    // Save current state
    auto pos = m_editor->cursorPosition();
    m_savedCursorPara = pos.paragraph;
    m_savedCursorOffset = pos.offset;

    // For benchmark, we use existing editor content modification methods
    // Select all and delete, then insert new content
    m_editor->selectAll();
    if (m_editor->hasSelection()) {
        m_editor->deleteBackward();  // deleteBackward removes selection if one exists
    }

    // Insert test content
    if (!textContent.isEmpty()) {
        m_editor->insertText(textContent);
    }

    // Move cursor to start
    m_editor->setCursorPosition({0, 0});

    // CRITICAL: Force layout calculation
    // QTextDocument layouts are created lazily during paint.
    // We need to ensure the widget is fully painted before benchmarking.
    m_editor->repaint();
    QApplication::processEvents();
}

void EditorBenchmark::restoreEditor()
{
    // Restore is simplified - benchmarks are typically run in isolation
    // Clear and add message
    m_editor->selectAll();
    if (m_editor->hasSelection()) {
        m_editor->deleteBackward();  // deleteBackward removes selection if one exists
    }
    m_editor->insertText(QStringLiteral("Benchmark complete. Original content not preserved."));
    m_editor->setCursorPosition({0, 0});
}

} // namespace editor
} // namespace kalahari
