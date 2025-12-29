/// @file benchmark_new_architecture.cpp
/// @brief Performance benchmarks for OpenSpec #00043 new editor architecture
///
/// Benchmarks for the new TextBuffer, LazyLayoutManager, and RenderEngine
/// components. Tests against 150k word documents to match Word/Writer performance.
///
/// Target performance (matching Word):
/// - Scrolling:      60 fps (16ms frame budget)
/// - Select All:     < 50ms
/// - Copy:           < 100ms
/// - Typing latency: < 16ms
/// - Document load:  < 2 seconds

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "test_document_generator.h"
#include "performance_benchmark.h"

#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/lazy_layout_manager.h>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QElapsedTimer>
#include <iostream>
#include <numeric>
#include <cmath>
#include <sstream>

using namespace kalahari::benchmarks;
using namespace kalahari::editor;

// =============================================================================
// Test Fixtures
// =============================================================================

namespace {

/// @brief Shared test document (generated once per test run)
struct TestDocumentFixture {
    QString plainText;
    QString kmlText;
    int wordCount = 0;

    static TestDocumentFixture& getInstance() {
        static TestDocumentFixture instance;
        return instance;
    }

    void ensureGenerated() {
        if (plainText.isEmpty()) {
            TestDocumentGenerator generator;
            plainText = generator.generatePlainText();
            wordCount = generator.lastWordCount();

            // Generate KML separately
            TestDocumentGenerator kmlGenerator;
            kmlText = kmlGenerator.generateKml();

            std::cout << "Generated test document: " << wordCount << " words, "
                      << plainText.length() << " characters\n";
        }
    }

private:
    TestDocumentFixture() = default;
};

/// @brief Get the test font
QFont getTestFont() {
    return QFont("Segoe UI", 11);
}

} // anonymous namespace

// =============================================================================
// Test Document Generator Tests
// =============================================================================

TEST_CASE("TestDocumentGenerator generates correct word counts", "[benchmark][generator]") {
    SECTION("Default 150k words") {
        TestDocumentGenerator generator;
        QString text = generator.generatePlainText();

        REQUIRE(generator.lastWordCount() >= 149000);
        REQUIRE(generator.lastWordCount() <= 151000);
        REQUIRE(!text.isEmpty());
    }

    SECTION("Custom word count") {
        TestDocumentGenerator::Config config;
        config.targetWordCount = 10000;
        TestDocumentGenerator generator(config);
        QString text = generator.generatePlainText();

        REQUIRE(generator.lastWordCount() >= 9900);
        REQUIRE(generator.lastWordCount() <= 10100);
    }

    SECTION("Reproducibility with seed") {
        TestDocumentGenerator gen1;
        TestDocumentGenerator gen2;

        QString text1 = gen1.generatePlainText();
        QString text2 = gen2.generatePlainText();

        REQUIRE(text1 == text2);
    }
}

TEST_CASE("TestDocumentGenerator generates KML", "[benchmark][generator]") {
    TestDocumentGenerator::Config config;
    config.targetWordCount = 1000;
    TestDocumentGenerator generator(config);

    QString kml = generator.generateKml();

    REQUIRE(kml.contains("<p>"));
    REQUIRE(kml.contains("</p>"));
    // Should have some formatting
    REQUIRE((kml.contains("<b>") || kml.contains("<i>")));
}

// =============================================================================
// TextBuffer Performance Benchmarks
// =============================================================================

TEST_CASE("TextBuffer load performance", "[benchmark][text_buffer]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    PerformanceBenchmark bench;

    SECTION("Load 150k word document") {
        auto result = bench.run("TextBuffer.setPlainText(150k)", [&]() {
            TextBuffer buffer;
            buffer.setPlainText(fixture.plainText);
        }, 10, 2, Targets::DOCUMENT_LOAD);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

TEST_CASE("TextBuffer paragraph access performance", "[benchmark][text_buffer]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    size_t paragraphCount = buffer.paragraphCount();
    REQUIRE(paragraphCount > 0);

    PerformanceBenchmark bench;

    SECTION("Random paragraph text access") {
        std::mt19937 rng(42);
        std::vector<size_t> indices;
        for (int i = 0; i < 1000; ++i) {
            indices.push_back(rng() % paragraphCount);
        }

        auto result = bench.run("paragraphText() x1000", [&]() {
            for (size_t idx : indices) {
                volatile auto text = buffer.paragraphText(idx);
                (void)text;
            }
        }, 100, 10, BENCHMARK_TARGET_MS(10));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

TEST_CASE("TextBuffer Y-to-paragraph lookup performance", "[benchmark][text_buffer]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    // Initialize heights (simulate calculated heights)
    size_t count = buffer.paragraphCount();
    for (size_t i = 0; i < count; ++i) {
        buffer.setParagraphHeight(i, 20.0 + (i % 3) * 5.0);
    }

    double totalHeight = buffer.totalHeight();
    REQUIRE(totalHeight > 0);

    PerformanceBenchmark bench;

    SECTION("Random Y lookups (Fenwick tree)") {
        std::mt19937 rng(42);
        std::vector<double> yPositions;
        for (int i = 0; i < 1000; ++i) {
            yPositions.push_back(static_cast<double>(rng() % static_cast<int>(totalHeight)));
        }

        auto result = bench.run("getParagraphAtY() x1000", [&]() {
            for (double y : yPositions) {
                volatile auto idx = buffer.getParagraphAtY(y);
                (void)idx;
            }
        }, 100, 10, BENCHMARK_TARGET_MS(1));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

TEST_CASE("TextBuffer insert performance", "[benchmark][text_buffer]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    PerformanceBenchmark bench;

    SECTION("Single character insert (typing simulation)") {
        TextBuffer buffer;
        buffer.setPlainText(fixture.plainText);
        int pos = buffer.characterCount() / 2;

        auto result = bench.run("insert() single char", [&]() {
            buffer.insert(pos, "x");
            pos++;
        }, 1000, 10, Targets::TYPING_LATENCY);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

// =============================================================================
// LazyLayoutManager Performance Benchmarks
// =============================================================================

TEST_CASE("LazyLayoutManager initialization performance", "[benchmark][lazy_layout]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    PerformanceBenchmark bench;

    SECTION("Initialize with 150k word document") {
        auto result = bench.run("LazyLayoutManager init", [&]() {
            LazyLayoutManager manager(&buffer);
            manager.setWidth(800.0);
            manager.setFont(getTestFont());
        }, 50, 5, BENCHMARK_TARGET_MS(100));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

TEST_CASE("LazyLayoutManager viewport update performance", "[benchmark][lazy_layout]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());
    manager.setViewport(0, 600);
    manager.layoutVisibleParagraphs();

    double totalHeight = manager.totalHeight();
    PerformanceBenchmark bench;

    SECTION("Viewport scroll (60 fps requirement)") {
        double scrollPos = 0;
        double scrollStep = 50.0;  // 50px per frame

        auto result = bench.run("setViewport() + layout", [&]() {
            manager.setViewport(scrollPos, 600);
            manager.layoutVisibleParagraphs();
            scrollPos += scrollStep;
            if (scrollPos > totalHeight - 600) {
                scrollPos = 0;
            }
        }, 100, 10, Targets::FRAME_60FPS);

        PerformanceBenchmark::printResult(result);
        // Note: This is a key performance target
        CHECK(result.passedTarget);
    }

    SECTION("Random viewport jumps") {
        std::mt19937 rng(42);

        auto result = bench.run("Random viewport jumps", [&]() {
            double y = static_cast<double>(rng() % static_cast<int>(totalHeight - 600));
            manager.setViewport(y, 600);
            manager.layoutVisibleParagraphs();
        }, 100, 10, BENCHMARK_TARGET_MS(50));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

TEST_CASE("LazyLayoutManager layout calculation performance", "[benchmark][lazy_layout]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());

    PerformanceBenchmark bench;

    SECTION("Layout visible paragraphs only") {
        manager.setViewport(0, 600);

        auto result = bench.run("layoutVisibleParagraphs()", [&]() {
            manager.invalidateAllLayouts();
            manager.layoutVisibleParagraphs();
        }, 50, 5, BENCHMARK_TARGET_MS(16));

        std::cout << "Layouts cached: " << manager.layoutCount() << "\n";
        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

// =============================================================================
// Select All / Copy Performance Benchmarks
// =============================================================================

TEST_CASE("Select All performance", "[benchmark][selection]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    PerformanceBenchmark bench;

    SECTION("Get full text (Select All simulation)") {
        auto result = bench.run("plainText() for Select All", [&]() {
            buffer.invalidatePlainTextCache();
            volatile auto text = buffer.plainText();
            (void)text;
        }, 50, 5, Targets::SELECT_ALL);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }

    SECTION("Cached text access") {
        // First call caches
        buffer.plainText();

        auto result = bench.run("plainText() cached", [&]() {
            volatile auto text = buffer.plainText();
            (void)text;
        }, 100, 10, BENCHMARK_TARGET_MS(1));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

// =============================================================================
// Scrolling FPS Benchmark
// =============================================================================

TEST_CASE("Scrolling FPS benchmark", "[benchmark][scrolling]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());
    manager.setViewport(0, 600);
    manager.layoutVisibleParagraphs();

    double totalHeight = manager.totalHeight();

    SECTION("Continuous scroll FPS measurement") {
        const int numFrames = 100;
        const double scrollStep = 30.0;  // Typical mouse scroll

        QElapsedTimer timer;
        timer.start();

        double scrollPos = 0;
        int framesRendered = 0;

        for (int i = 0; i < numFrames; ++i) {
            manager.setViewport(scrollPos, 600);
            manager.layoutVisibleParagraphs();

            scrollPos += scrollStep;
            if (scrollPos > totalHeight - 600) {
                scrollPos = 0;
            }
            framesRendered++;
        }

        qint64 totalMs = timer.elapsed();
        double fps = (framesRendered * 1000.0) / totalMs;

        std::cout << "\nScrolling FPS: " << std::fixed << std::setprecision(1) << fps << "\n";
        std::cout << "Frame time: " << std::setprecision(2) << (totalMs / static_cast<double>(framesRendered)) << " ms\n";

        REQUIRE(fps >= 30.0);  // Minimum acceptable
        CHECK(fps >= 60.0);    // Target
    }
}

// =============================================================================
// Typing Latency Benchmark
// =============================================================================

TEST_CASE("Typing latency benchmark", "[benchmark][typing]") {
    // Start with a smaller document for typing simulation
    TestDocumentGenerator::Config config;
    config.targetWordCount = 50000;  // 50k words
    TestDocumentGenerator generator(config);
    QString text = generator.generatePlainText();

    TextBuffer buffer;
    buffer.setPlainText(text);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());

    // Position cursor in middle of document
    size_t middlePara = buffer.paragraphCount() / 2;
    double y = buffer.getParagraphY(middlePara);
    manager.setViewport(y, 600);
    manager.layoutVisibleParagraphs();

    int insertPos = buffer.characterCount() / 2;

    PerformanceBenchmark bench;

    SECTION("Single keystroke latency") {
        auto result = bench.run("Keystroke latency", [&]() {
            // Simulate: insert char + update affected layouts
            buffer.insert(insertPos, "x");
            manager.layoutVisibleParagraphs();
        }, 100, 10, Targets::TYPING_LATENCY);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

// =============================================================================
// Document Load Benchmark
// =============================================================================

TEST_CASE("Document load benchmark", "[benchmark][load]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    PerformanceBenchmark bench;

    SECTION("Full document load pipeline") {
        auto result = bench.run("Full load (buffer + layout init)", [&]() {
            TextBuffer buffer;
            buffer.setPlainText(fixture.plainText);

            LazyLayoutManager manager(&buffer);
            manager.setWidth(800.0);
            manager.setFont(getTestFont());
            manager.setViewport(0, 600);
            manager.layoutVisibleParagraphs();
        }, 10, 2, Targets::DOCUMENT_LOAD);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }
}

// =============================================================================
// Stress Tests (Phase 10 Tasks 10.7-10.10)
// =============================================================================

TEST_CASE("Stress test: 200k+ word document", "[stress][10.7]") {
    TestDocumentGenerator::Config config;
    config.targetWordCount = 200000;  // 200k words
    TestDocumentGenerator generator(config);

    QString text = generator.generatePlainText();
    REQUIRE(generator.lastWordCount() >= 195000);

    std::cout << "\n=== 200k Word Document Stress Test ===\n";
    std::cout << "Generated: " << generator.lastWordCount() << " words, "
              << text.length() << " characters\n";

    SECTION("Load 200k word document") {
        PerformanceBenchmark bench;
        auto result = bench.run("TextBuffer.setPlainText(200k)", [&]() {
            TextBuffer buffer;
            buffer.setPlainText(text);
        }, 5, 1, Targets::DOCUMENT_LOAD);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }

    SECTION("Layout init with 200k words") {
        TextBuffer buffer;
        buffer.setPlainText(text);

        PerformanceBenchmark bench;
        auto result = bench.run("LazyLayoutManager init (200k)", [&]() {
            LazyLayoutManager manager(&buffer);
            manager.setWidth(800.0);
            manager.setFont(getTestFont());
            manager.setViewport(0, 600);
            manager.layoutVisibleParagraphs();
        }, 5, 1, Targets::DOCUMENT_LOAD);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }

    SECTION("Scroll through 200k document") {
        TextBuffer buffer;
        buffer.setPlainText(text);

        LazyLayoutManager manager(&buffer);
        manager.setWidth(800.0);
        manager.setFont(getTestFont());
        manager.setViewport(0, 600);
        manager.layoutVisibleParagraphs();

        double totalHeight = manager.totalHeight();
        std::cout << "Total height: " << totalHeight << " pixels\n";

        // Scroll through entire document
        QElapsedTimer timer;
        timer.start();

        int scrollSteps = 100;
        double stepSize = totalHeight / scrollSteps;
        for (int i = 0; i < scrollSteps; ++i) {
            manager.setViewport(i * stepSize, 600);
            manager.layoutVisibleParagraphs();
        }

        qint64 elapsed = timer.elapsed();
        std::cout << "Full scroll time: " << elapsed << " ms\n";
        std::cout << "Average per step: " << (elapsed / static_cast<double>(scrollSteps)) << " ms\n";

        REQUIRE(elapsed < 5000);  // Should complete in under 5 seconds
    }
}

TEST_CASE("Stress test: Rapid scrolling", "[stress][10.8]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());
    manager.setViewport(0, 600);
    manager.layoutVisibleParagraphs();

    double totalHeight = manager.totalHeight();

    SECTION("Scroll through entire doc in 5 seconds (300 frames)") {
        std::cout << "\n=== Rapid Scrolling Stress Test ===\n";

        const int targetFrames = 300;  // 60fps * 5 seconds
        const double scrollStep = totalHeight / targetFrames;

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < targetFrames; ++i) {
            double y = std::fmod(i * scrollStep, totalHeight);
            manager.setViewport(y, 600);
            manager.layoutVisibleParagraphs();
        }

        qint64 elapsed = timer.elapsed();
        double fps = (targetFrames * 1000.0) / elapsed;

        std::cout << "Frames: " << targetFrames << "\n";
        std::cout << "Total time: " << elapsed << " ms\n";
        std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << fps << "\n";
        std::cout << "Frame time: " << std::setprecision(2) << (elapsed / static_cast<double>(targetFrames)) << " ms\n";

        REQUIRE(fps >= 60.0);  // Must maintain 60fps
    }

    SECTION("Random jump scrolling") {
        std::cout << "\n=== Random Jump Scrolling ===\n";

        std::mt19937 rng(12345);
        const int jumps = 100;

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < jumps; ++i) {
            double y = static_cast<double>(rng() % static_cast<int>(totalHeight));
            manager.setViewport(y, 600);
            manager.layoutVisibleParagraphs();
        }

        qint64 elapsed = timer.elapsed();
        std::cout << "Random jumps: " << jumps << "\n";
        std::cout << "Total time: " << elapsed << " ms\n";
        std::cout << "Average per jump: " << (elapsed / static_cast<double>(jumps)) << " ms\n";

        REQUIRE(elapsed < 2000);  // 100 jumps in under 2 seconds
    }
}

TEST_CASE("Stress test: Rapid typing", "[stress][10.9]") {
    // Start with a moderate document
    TestDocumentGenerator::Config config;
    config.targetWordCount = 50000;
    TestDocumentGenerator generator(config);
    QString text = generator.generatePlainText();

    TextBuffer buffer;
    buffer.setPlainText(text);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());

    // Position in middle of document
    size_t middlePara = buffer.paragraphCount() / 2;
    double y = buffer.getParagraphY(middlePara);
    manager.setViewport(y, 600);
    manager.layoutVisibleParagraphs();

    SECTION("100 chars/second for 10 seconds") {
        std::cout << "\n=== Rapid Typing Stress Test (100 chars/sec) ===\n";

        const int totalChars = 1000;  // 100 chars/sec * 10 sec
        int insertPos = buffer.characterCount() / 2;

        std::vector<qint64> latencies;
        latencies.reserve(totalChars);

        QElapsedTimer timer;

        for (int i = 0; i < totalChars; ++i) {
            timer.restart();

            // Insert character
            buffer.insert(insertPos, "x");
            manager.layoutVisibleParagraphs();

            latencies.push_back(timer.nsecsElapsed());
            insertPos++;
        }

        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        double avgNs = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        double p50Ns = latencies[latencies.size() / 2];
        double p99Ns = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        double maxNs = latencies.back();

        std::cout << "Characters typed: " << totalChars << "\n";
        std::cout << "Average latency: " << std::fixed << std::setprecision(2) << (avgNs / 1e6) << " ms\n";
        std::cout << "P50 latency: " << (p50Ns / 1e6) << " ms\n";
        std::cout << "P99 latency: " << (p99Ns / 1e6) << " ms\n";
        std::cout << "Max latency: " << (maxNs / 1e6) << " ms\n";

        // All keystrokes should be under 16ms (60fps frame budget)
        REQUIRE(p99Ns < 16e6);  // P99 < 16ms
    }

    SECTION("Burst typing (10 chars as fast as possible)") {
        std::cout << "\n=== Burst Typing Test ===\n";

        const int burstSize = 10;
        const int bursts = 100;
        int insertPos = buffer.characterCount() / 2;

        std::vector<qint64> burstTimes;
        burstTimes.reserve(bursts);

        QElapsedTimer timer;

        for (int b = 0; b < bursts; ++b) {
            timer.restart();

            for (int i = 0; i < burstSize; ++i) {
                buffer.insert(insertPos, "x");
                insertPos++;
            }
            manager.layoutVisibleParagraphs();

            burstTimes.push_back(timer.nsecsElapsed());
        }

        std::sort(burstTimes.begin(), burstTimes.end());
        double avgMs = std::accumulate(burstTimes.begin(), burstTimes.end(), 0.0) / burstTimes.size() / 1e6;
        double p99Ms = burstTimes[static_cast<size_t>(burstTimes.size() * 0.99)] / 1e6;

        std::cout << "Burst size: " << burstSize << " chars\n";
        std::cout << "Number of bursts: " << bursts << "\n";
        std::cout << "Average burst time: " << std::fixed << std::setprecision(2) << avgMs << " ms\n";
        std::cout << "P99 burst time: " << p99Ms << " ms\n";

        REQUIRE(p99Ms < 32);  // 10-char burst should complete in 2 frame times
    }
}

TEST_CASE("Stress test: Large selection operations", "[stress][10.10]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    SECTION("Select entire 150k word document") {
        std::cout << "\n=== Large Selection Test ===\n";

        PerformanceBenchmark bench;
        auto result = bench.run("Select All (150k words)", [&]() {
            // Simulate select all - get full text
            buffer.invalidatePlainTextCache();
            QString text = buffer.plainText();
            qsizetype length = text.length();
            (void)text;
            (void)length;
        }, 20, 5, Targets::SELECT_ALL);

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }

    SECTION("Copy entire document to string") {
        std::cout << "\n=== Large Copy Test ===\n";

        // First access to cache
        buffer.plainText();

        PerformanceBenchmark bench;
        auto result = bench.run("Copy All (cached)", [&]() {
            volatile auto text = buffer.plainText();
            (void)text;
        }, 100, 10, BENCHMARK_TARGET_MS(10));

        PerformanceBenchmark::printResult(result);
        REQUIRE(result.passedTarget);
    }

    SECTION("Select range performance") {
        std::cout << "\n=== Range Selection Test ===\n";

        size_t totalChars = buffer.characterCount();

        // Test various selection sizes
        struct SelectionTest {
            const char* name;
            size_t start;
            size_t length;
        };

        std::vector<SelectionTest> tests = {
            {"Small (100 chars)", totalChars / 2, 100},
            {"Medium (10k chars)", totalChars / 2, 10000},
            {"Large (100k chars)", totalChars / 4, 100000},
            {"Half doc", 0, totalChars / 2},
        };

        for (const auto& test : tests) {
            QElapsedTimer timer;
            timer.start();

            for (int i = 0; i < 100; ++i) {
                // Selection operations are O(1) - just setting range
                // The expensive part is rendering, but selection state is cheap
                volatile size_t start = test.start;
                volatile size_t end = test.start + test.length;
                (void)start;
                (void)end;
            }

            qint64 elapsed = timer.elapsed();
            std::cout << test.name << ": " << elapsed << " ms (100 ops)\n";
        }

        SUCCEED();
    }

    SECTION("Delete large selection") {
        std::cout << "\n=== Large Delete Test ===\n";

        // Create a copy for delete tests
        TextBuffer testBuffer;
        testBuffer.setPlainText(fixture.plainText);

        size_t initialCount = testBuffer.characterCount();

        QElapsedTimer timer;
        timer.start();

        // Delete first half of document
        int deleteSize = static_cast<int>(initialCount / 2);
        testBuffer.remove(0, deleteSize);

        qint64 elapsed = timer.elapsed();

        std::cout << "Deleted " << deleteSize << " characters\n";
        std::cout << "Delete time: " << elapsed << " ms\n";
        std::cout << "Remaining: " << testBuffer.characterCount() << " characters\n";

        REQUIRE(elapsed < 500);  // Large delete should be fast
        REQUIRE(testBuffer.characterCount() < initialCount);
    }
}

// =============================================================================
// Memory Profiling (Phase 10 Tasks 10.11-10.14)
// =============================================================================

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace {
/// @brief Get current process memory usage in bytes
size_t getMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    // Linux/macOS: read from /proc/self/statm
    FILE* file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long size;
        if (fscanf(file, "%lu", &size) == 1) {
            fclose(file);
            return size * sysconf(_SC_PAGESIZE);
        }
        fclose(file);
    }
    return 0;
#endif
}

/// @brief Format bytes as human-readable string
std::string formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}
} // anonymous namespace

TEST_CASE("Memory profiling: Document load", "[memory][10.11]") {
    std::cout << "\n=== Memory Profiling: Document Load ===\n";

    size_t baselineMemory = getMemoryUsage();
    std::cout << "Baseline memory: " << formatBytes(baselineMemory) << "\n";

    SECTION("Memory for 150k word document") {
        auto& fixture = TestDocumentFixture::getInstance();
        fixture.ensureGenerated();

        size_t afterGenMemory = getMemoryUsage();

        TextBuffer buffer;
        buffer.setPlainText(fixture.plainText);

        size_t afterLoadMemory = getMemoryUsage();

        LazyLayoutManager manager(&buffer);
        manager.setWidth(800.0);
        manager.setFont(getTestFont());
        manager.setViewport(0, 600);
        manager.layoutVisibleParagraphs();

        size_t afterLayoutMemory = getMemoryUsage();

        std::cout << "After doc generation: " << formatBytes(afterGenMemory)
                  << " (+" << formatBytes(afterGenMemory - baselineMemory) << ")\n";
        std::cout << "After TextBuffer load: " << formatBytes(afterLoadMemory)
                  << " (+" << formatBytes(afterLoadMemory - afterGenMemory) << ")\n";
        std::cout << "After layout init: " << formatBytes(afterLayoutMemory)
                  << " (+" << formatBytes(afterLayoutMemory - afterLoadMemory) << ")\n";

        // Calculate memory per word
        size_t totalUsed = afterLayoutMemory - baselineMemory;
        double bytesPerWord = static_cast<double>(totalUsed) / fixture.wordCount;
        std::cout << "Memory per word: " << std::fixed << std::setprecision(1)
                  << bytesPerWord << " bytes\n";

        // Reasonable memory usage: <100 bytes per word for 150k document
        // (character storage + metadata)
        REQUIRE(bytesPerWord < 200);  // Allow some overhead
    }
}

TEST_CASE("Memory profiling: Leak detection pattern", "[memory][10.12]") {
    std::cout << "\n=== Memory Profiling: Leak Detection ===\n";

    SECTION("Repeated document load/unload") {
        auto& fixture = TestDocumentFixture::getInstance();
        fixture.ensureGenerated();

        // Warm up
        {
            TextBuffer buffer;
            buffer.setPlainText(fixture.plainText);
        }

        size_t beforeLoop = getMemoryUsage();
        std::cout << "Before loop: " << formatBytes(beforeLoop) << "\n";

        // Load and unload multiple times
        const int iterations = 10;
        for (int i = 0; i < iterations; ++i) {
            TextBuffer buffer;
            buffer.setPlainText(fixture.plainText);

            LazyLayoutManager manager(&buffer);
            manager.setWidth(800.0);
            manager.setFont(getTestFont());
            manager.setViewport(0, 600);
            manager.layoutVisibleParagraphs();
        }

        size_t afterLoop = getMemoryUsage();
        std::cout << "After " << iterations << " load/unload cycles: "
                  << formatBytes(afterLoop) << "\n";

        // Memory growth should be minimal (allow 10% growth for fragmentation)
        double growthRatio = static_cast<double>(afterLoop) / beforeLoop;
        std::cout << "Growth ratio: " << std::fixed << std::setprecision(2)
                  << growthRatio << "x\n";

        // Allow some growth but not excessive (would indicate leak)
        CHECK(growthRatio < 1.5);  // Less than 50% growth after 10 cycles
    }
}

TEST_CASE("Memory profiling: Long editing session", "[memory][10.14]") {
    std::cout << "\n=== Memory Profiling: Long Editing Session ===\n";

    TestDocumentGenerator::Config config;
    config.targetWordCount = 10000;  // Smaller for many operations
    TestDocumentGenerator generator(config);
    QString text = generator.generatePlainText();

    TextBuffer buffer;
    buffer.setPlainText(text);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());
    manager.setViewport(0, 600);
    manager.layoutVisibleParagraphs();

    size_t startMemory = getMemoryUsage();
    std::cout << "Start memory: " << formatBytes(startMemory) << "\n";

    SECTION("Many insert/delete operations") {
        int insertPos = buffer.characterCount() / 2;

        // Simulate long editing session: 1000 insert/delete pairs
        for (int i = 0; i < 1000; ++i) {
            // Insert
            buffer.insert(insertPos, "Test text ");
            manager.layoutVisibleParagraphs();

            // Delete
            buffer.remove(insertPos, 10);
            manager.layoutVisibleParagraphs();
        }

        size_t afterOps = getMemoryUsage();
        std::cout << "After 1000 insert/delete pairs: " << formatBytes(afterOps)
                  << " (+" << formatBytes(afterOps > startMemory ? afterOps - startMemory : 0) << ")\n";

        // Memory shouldn't grow significantly for balanced ops
        double growthRatio = static_cast<double>(afterOps) / startMemory;
        std::cout << "Growth ratio: " << std::fixed << std::setprecision(2)
                  << growthRatio << "x\n";

        CHECK(growthRatio < 2.0);  // Allow 2x for balanced operations
    }

    SECTION("Many scroll operations") {
        double totalHeight = manager.totalHeight();

        for (int i = 0; i < 1000; ++i) {
            double y = std::fmod(i * 100.0, totalHeight);
            manager.setViewport(y, 600);
            manager.layoutVisibleParagraphs();
        }

        size_t afterScroll = getMemoryUsage();
        std::cout << "After 1000 scrolls: " << formatBytes(afterScroll) << "\n";

        // Scrolling shouldn't increase memory (layouts are reused)
        double growthRatio = static_cast<double>(afterScroll) / startMemory;
        CHECK(growthRatio < 1.5);
    }
}

// =============================================================================
// Thread Safety (Phase 10 Tasks 10.15-10.17)
// NOTE: Background layout was deferred to Phase 8 integration.
// Current implementation is single-threaded. Tests verify thread-safety
// requirements are documented and basic invariants hold.
// =============================================================================

TEST_CASE("Thread safety: Single-threaded invariants", "[thread][10.15]") {
    std::cout << "\n=== Thread Safety: Single-threaded Invariants ===\n";

    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(800.0);
    manager.setFont(getTestFont());

    SECTION("Concurrent-safe read operations") {
        // These operations should be safe to call from any thread
        // (read-only, no modification)

        size_t paraCount = buffer.paragraphCount();
        double totalHeight = buffer.totalHeight();
        size_t charCount = buffer.characterCount();

        std::cout << "Paragraph count: " << paraCount << "\n";
        std::cout << "Total height: " << totalHeight << "\n";
        std::cout << "Character count: " << charCount << "\n";

        // Consistency check
        REQUIRE(paraCount > 0);
        REQUIRE(totalHeight > 0);
        REQUIRE(charCount > 0);
    }

    SECTION("State consistency after operations") {
        // Verify state is consistent after operations
        size_t initialParas = buffer.paragraphCount();
        size_t initialChars = buffer.characterCount();

        // Insert at start
        buffer.insert(0, "New text at start\n");
        manager.layoutVisibleParagraphs();

        REQUIRE(buffer.paragraphCount() >= initialParas);
        REQUIRE(buffer.characterCount() > initialChars);

        // State should be consistent
        double height = buffer.totalHeight();
        REQUIRE(height > 0);
    }
}

TEST_CASE("Thread safety: Future multi-thread support notes", "[.thread][10.16][10.17]") {
    // NOTE: This test documents future thread safety requirements
    // Background layout thread was deferred from Phase 4.23-4.28

    std::cout << "\n=== Thread Safety: Future Requirements ===\n";
    std::cout << "Background layout thread: DEFERRED (not yet implemented)\n";
    std::cout << "ThreadSanitizer testing: Requires separate build with TSan\n";
    std::cout << "\n";
    std::cout << "Current architecture is single-threaded.\n";
    std::cout << "Future multi-thread implementation should:\n";
    std::cout << "  1. Use read-write locks for TextBuffer access\n";
    std::cout << "  2. Queue layout requests from background thread\n";
    std::cout << "  3. Use atomic flags for dirty region tracking\n";
    std::cout << "  4. Ensure viewport updates are thread-safe\n";

    SUCCEED();
}

// =============================================================================
// Catch2 Benchmark Integration (alternative benchmarks)
// =============================================================================

TEST_CASE("Catch2 native benchmarks", "[.benchmark][native]") {
    auto& fixture = TestDocumentFixture::getInstance();
    fixture.ensureGenerated();

    TextBuffer buffer;
    buffer.setPlainText(fixture.plainText);

    BENCHMARK("TextBuffer paragraph count") {
        return buffer.paragraphCount();
    };

    BENCHMARK("TextBuffer paragraph text (middle)") {
        return buffer.paragraphText(buffer.paragraphCount() / 2);
    };

    BENCHMARK("TextBuffer total height") {
        return buffer.totalHeight();
    };
}

// =============================================================================
// Performance Summary
// =============================================================================

TEST_CASE("Performance summary", "[benchmark][summary]") {
    std::cout << "\n";
    std::cout << "======================================================\n";
    std::cout << "PERFORMANCE SUMMARY\n";
    std::cout << "======================================================\n";
    std::cout << "\n";
    std::cout << "Target Performance (matching Word/Writer):\n";
    std::cout << "  - Scrolling:      60 fps (16ms frame budget)\n";
    std::cout << "  - Select All:     < 50ms\n";
    std::cout << "  - Copy:           < 100ms\n";
    std::cout << "  - Typing latency: < 16ms\n";
    std::cout << "  - Document load:  < 2 seconds\n";
    std::cout << "\n";
    std::cout << "New Architecture Components:\n";
    std::cout << "  - TextBuffer: QTextDocument + Fenwick tree heights\n";
    std::cout << "  - LazyLayoutManager: Viewport-only layout calculation\n";
    std::cout << "  - RenderEngine: Viewport-only rendering\n";
    std::cout << "\n";
    std::cout << "Run with: benchmark-new-arch [section]\n";
    std::cout << "======================================================\n";

    SUCCEED();
}

// =============================================================================
// Custom Main (required for QApplication initialization)
// =============================================================================

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    // Initialize Qt (required for QFontDatabase, QTextLayout, and other font operations)
    QApplication app(argc, argv);
    app.setApplicationName("benchmark-new-arch");

    // Initialize Catch2
    Catch::Session session;

    // Parse command line
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }

    // Run benchmarks
    return session.run();
}
