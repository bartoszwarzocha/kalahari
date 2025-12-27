/**
 * @file test_piece_table_benchmark.cpp
 * @brief Benchmark tests for Piece Table vs Traditional QString
 *
 * OpenSpec #00043 - Phase 1: Research & Spike
 * Task 1.1: Create minimal piece table prototype
 * Task 1.2: Benchmark piece table vs QString for 150k words
 */

#include "piece_table_prototype.h"

#include <gtest/gtest.h>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <iostream>
#include <iomanip>

using namespace prototype;

namespace {

/**
 * @brief Generate lorem ipsum-like text with specified word count
 */
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
    result.reserve(wordCount * 8); // Average word length + space

    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) {
            result.append(' ');
        }
        result.append(words[QRandomGenerator::global()->bounded(words.size())]);

        // Add paragraph breaks every ~50 words
        if (i > 0 && i % 50 == 0) {
            result.append("\n\n");
        }
    }

    return result;
}

/**
 * @brief Format nanoseconds to human readable
 */
QString formatTime(qint64 nanoseconds) {
    if (nanoseconds < 1000) {
        return QString("%1 ns").arg(nanoseconds);
    } else if (nanoseconds < 1000000) {
        return QString("%1 µs").arg(nanoseconds / 1000.0, 0, 'f', 2);
    } else if (nanoseconds < 1000000000) {
        return QString("%1 ms").arg(nanoseconds / 1000000.0, 0, 'f', 2);
    } else {
        return QString("%1 s").arg(nanoseconds / 1000000000.0, 0, 'f', 3);
    }
}

void printBenchmarkHeader(const QString& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title.toStdString() << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printResult(const QString& operation, qint64 pieceTableNs, qint64 traditionalNs) {
    double ratio = static_cast<double>(traditionalNs) / pieceTableNs;
    QString winner = (pieceTableNs < traditionalNs) ? "PieceTable" : "Traditional";
    QString ratioStr = (ratio > 1.0)
        ? QString("%1x faster").arg(ratio, 0, 'f', 2)
        : QString("%1x slower").arg(1.0/ratio, 0, 'f', 2);

    std::cout << std::left << std::setw(25) << operation.toStdString()
              << "  PT: " << std::setw(12) << formatTime(pieceTableNs).toStdString()
              << "  Trad: " << std::setw(12) << formatTime(traditionalNs).toStdString()
              << "  " << ratioStr.toStdString() << " (" << winner.toStdString() << ")\n";
}

} // anonymous namespace

class PieceTableBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator for reproducibility
        QRandomGenerator::global()->seed(42);
    }
};

/**
 * @brief Test basic piece table operations
 */
TEST_F(PieceTableBenchmarkTest, BasicOperations) {
    PieceTable pt("Hello World");

    EXPECT_EQ(pt.text(), "Hello World");
    EXPECT_EQ(pt.length(), 11u);

    pt.insert(6, "Beautiful ");
    EXPECT_EQ(pt.text(), "Hello Beautiful World");

    pt.remove(6, 10);
    EXPECT_EQ(pt.text(), "Hello World");
}

/**
 * @brief Benchmark: Load 150k words document
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_Load_150k) {
    printBenchmarkHeader("BENCHMARK: Load 150,000 words document");

    QString text = generateText(150000);
    std::cout << "Generated text size: " << text.length() << " characters\n";

    QElapsedTimer timer;

    // PieceTable load
    timer.start();
    PieceTable pt(text);
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Traditional load
    timer.start();
    TraditionalDocument td(text);
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("Load document", pieceTableNs, traditionalNs);

    EXPECT_EQ(pt.length(), td.length());
    std::cout << "PieceTable pieces: " << pt.pieceCount() << "\n";
}

/**
 * @brief Benchmark: Random inserts in 150k document
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_RandomInserts_150k) {
    printBenchmarkHeader("BENCHMARK: 1000 random inserts in 150k document");

    QString text = generateText(150000);
    PieceTable pt(text);
    TraditionalDocument td(text);

    const int numInserts = 1000;
    std::vector<std::pair<size_t, QString>> insertions;

    // Pre-generate insertions
    for (int i = 0; i < numInserts; ++i) {
        size_t pos = QRandomGenerator::global()->bounded(static_cast<quint32>(text.length()));
        QString insertText = QString("INSERT_%1").arg(i);
        insertions.emplace_back(pos, insertText);
    }

    QElapsedTimer timer;

    // PieceTable inserts
    timer.start();
    for (const auto& [pos, insertText] : insertions) {
        pt.insert(pos, insertText);
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Reset and test traditional
    td = TraditionalDocument(text);

    // Traditional inserts
    timer.start();
    for (const auto& [pos, insertText] : insertions) {
        td.insert(pos, insertText);
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("1000 random inserts", pieceTableNs, traditionalNs);
    std::cout << "PieceTable pieces after: " << pt.pieceCount() << "\n";
    std::cout << "PieceTable add buffer: " << pt.addBufferSize() << " chars\n";
}

/**
 * @brief Benchmark: Sequential typing simulation
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_SequentialTyping_150k) {
    printBenchmarkHeader("BENCHMARK: 10000 chars sequential typing at end");

    QString text = generateText(150000);
    PieceTable pt(text);
    TraditionalDocument td(text);

    const int numChars = 10000;

    QElapsedTimer timer;

    // PieceTable typing
    timer.start();
    for (int i = 0; i < numChars; ++i) {
        pt.insert(pt.length(), QString(QChar('a' + (i % 26))));
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Traditional typing
    timer.start();
    for (int i = 0; i < numChars; ++i) {
        td.insert(td.length(), QString(QChar('a' + (i % 26))));
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("10000 chars at end", pieceTableNs, traditionalNs);
    std::cout << "PieceTable pieces after: " << pt.pieceCount() << "\n";
}

/**
 * @brief Benchmark: Typing in middle of document
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_TypingInMiddle_150k) {
    printBenchmarkHeader("BENCHMARK: 1000 chars typing in middle");

    QString text = generateText(150000);
    PieceTable pt(text);
    TraditionalDocument td(text);

    const int numChars = 1000;
    size_t middlePos = text.length() / 2;

    QElapsedTimer timer;

    // PieceTable typing in middle
    timer.start();
    for (int i = 0; i < numChars; ++i) {
        pt.insert(middlePos + i, QString(QChar('a' + (i % 26))));
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Traditional typing in middle
    timer.start();
    for (int i = 0; i < numChars; ++i) {
        td.insert(middlePos + i, QString(QChar('a' + (i % 26))));
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("1000 chars in middle", pieceTableNs, traditionalNs);
}

/**
 * @brief Benchmark: Get full text (for clipboard/display)
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_GetText_150k) {
    printBenchmarkHeader("BENCHMARK: Get full text 100 times");

    QString text = generateText(150000);

    // First, do some edits to make piece table fragmented
    PieceTable pt(text);
    for (int i = 0; i < 100; ++i) {
        size_t pos = QRandomGenerator::global()->bounded(static_cast<quint32>(text.length()));
        pt.insert(pos, QString("EDIT_%1").arg(i));
    }

    TraditionalDocument td(text);
    for (int i = 0; i < 100; ++i) {
        size_t pos = QRandomGenerator::global()->bounded(static_cast<quint32>(text.length()));
        td.insert(pos, QString("EDIT_%1").arg(i));
    }

    std::cout << "After 100 edits, PieceTable has " << pt.pieceCount() << " pieces\n";

    const int numGets = 100;
    QElapsedTimer timer;

    // PieceTable get text (first call builds cache)
    timer.start();
    for (int i = 0; i < numGets; ++i) {
        volatile auto txt = pt.text(); // volatile to prevent optimization
        Q_UNUSED(txt);
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Traditional get text
    timer.start();
    for (int i = 0; i < numGets; ++i) {
        volatile auto txt = td.text();
        Q_UNUSED(txt);
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("Get text 100x", pieceTableNs, traditionalNs);
}

/**
 * @brief Benchmark: Delete operations
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_Delete_150k) {
    printBenchmarkHeader("BENCHMARK: 500 random deletions (10 chars each)");

    QString text = generateText(150000);
    PieceTable pt(text);
    TraditionalDocument td(text);

    const int numDeletes = 500;
    const int deleteLength = 10;
    std::vector<size_t> deletePositions;

    // Pre-generate positions (avoiding going past end)
    size_t safeLength = text.length() - deleteLength * numDeletes;
    for (int i = 0; i < numDeletes; ++i) {
        deletePositions.push_back(
            QRandomGenerator::global()->bounded(static_cast<quint32>(safeLength)));
    }

    QElapsedTimer timer;

    // PieceTable deletes
    timer.start();
    for (size_t pos : deletePositions) {
        pt.remove(pos, deleteLength);
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Reset traditional
    td = TraditionalDocument(text);

    // Traditional deletes
    timer.start();
    for (size_t pos : deletePositions) {
        td.remove(pos, deleteLength);
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("500 deletions", pieceTableNs, traditionalNs);
}

/**
 * @brief Benchmark: Mixed operations (realistic editing)
 */
TEST_F(PieceTableBenchmarkTest, Benchmark_MixedOperations_150k) {
    printBenchmarkHeader("BENCHMARK: Mixed operations (insert/delete/read)");

    QString text = generateText(150000);
    PieceTable pt(text);
    TraditionalDocument td(text);

    const int numOps = 1000;

    // Generate operation sequence
    struct Op {
        enum Type { Insert, Delete, Read } type;
        size_t pos;
        QString text;
    };
    std::vector<Op> operations;

    for (int i = 0; i < numOps; ++i) {
        int opType = QRandomGenerator::global()->bounded(100);
        Op op;

        if (opType < 50) {
            // 50% inserts
            op.type = Op::Insert;
            op.pos = QRandomGenerator::global()->bounded(static_cast<quint32>(text.length()));
            op.text = QString("INS%1").arg(i);
        } else if (opType < 80) {
            // 30% deletes
            op.type = Op::Delete;
            op.pos = QRandomGenerator::global()->bounded(static_cast<quint32>(text.length() - 10));
            op.text = ""; // unused
        } else {
            // 20% reads
            op.type = Op::Read;
            op.pos = 0;
            op.text = "";
        }
        operations.push_back(op);
    }

    QElapsedTimer timer;

    // PieceTable mixed ops
    timer.start();
    for (const auto& op : operations) {
        switch (op.type) {
            case Op::Insert:
                pt.insert(op.pos % pt.length(), op.text);
                break;
            case Op::Delete:
                if (pt.length() > 10) pt.remove(op.pos % (pt.length() - 5), 5);
                break;
            case Op::Read:
                { volatile auto t = pt.text(); Q_UNUSED(t); }
                break;
        }
    }
    qint64 pieceTableNs = timer.nsecsElapsed();

    // Reset traditional
    td = TraditionalDocument(text);

    // Traditional mixed ops
    timer.start();
    for (const auto& op : operations) {
        switch (op.type) {
            case Op::Insert:
                td.insert(op.pos % td.length(), op.text);
                break;
            case Op::Delete:
                if (td.length() > 10) td.remove(op.pos % (td.length() - 5), 5);
                break;
            case Op::Read:
                { volatile auto t = td.text(); Q_UNUSED(t); }
                break;
        }
    }
    qint64 traditionalNs = timer.nsecsElapsed();

    printResult("1000 mixed ops", pieceTableNs, traditionalNs);
    std::cout << "Final PieceTable pieces: " << pt.pieceCount() << "\n";
}

/**
 * @brief Summary and conclusions
 */
TEST_F(PieceTableBenchmarkTest, Summary) {
    printBenchmarkHeader("SUMMARY & CONCLUSIONS");

    std::cout << R"(
Piece Table Advantages:
  - O(1) insert to add buffer (no string copying)
  - Original text never modified (good for undo)
  - Memory efficient for many small edits

Piece Table Disadvantages:
  - O(N) piece vector operations (this prototype)
  - Text reconstruction requires traversing all pieces
  - More complex implementation

Recommendations for Production:
  1. Use balanced tree (red-black) for pieces -> O(log N) ops
  2. Cache full text with dirty flag invalidation
  3. Cache paragraph boundaries for O(1) paragraph access
  4. Consider rope data structure for very large documents

Alternative: QTextDocument
  - Already optimized by Qt team
  - Built-in undo/redo, layout, cursor handling
  - May be sufficient for our needs

NEXT: Benchmark QTextDocument with same workload
)";
}
