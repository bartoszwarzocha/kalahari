/// @file editor_benchmark.h
/// @brief Performance benchmarks for BookEditor (OpenSpec #00043)
///
/// Measures cursor navigation, text insertion, and deletion performance.
/// Results are logged and can be compared against reference values (e.g., Word).

#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <vector>
#include <functional>

namespace kalahari {
namespace editor {

class BookEditor;

/// @brief Result of a single benchmark run
struct BenchmarkResult {
    QString name;              ///< Benchmark name
    int iterations;            ///< Number of iterations
    double totalMs;            ///< Total time in milliseconds
    double avgMs;              ///< Average time per operation in ms
    double opsPerSecond;       ///< Operations per second
    double minMs;              ///< Minimum time for single op
    double maxMs;              ///< Maximum time for single op

    /// @brief Format result as string
    QString toString() const;
};

/// @brief Performance benchmarks for BookEditor
///
/// Usage:
/// @code
/// EditorBenchmark bench(editor);
/// bench.runAll();  // Runs all benchmarks
/// // Or run individual:
/// bench.benchCursorNavigation();
/// bench.benchTextInsertion();
/// @endcode
class EditorBenchmark : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param editor BookEditor to benchmark (must be valid)
    /// @param parent Parent QObject
    explicit EditorBenchmark(BookEditor* editor, QObject* parent = nullptr);

    /// @brief Destructor
    ~EditorBenchmark() override = default;

    // =========================================================================
    // Benchmark Configuration
    // =========================================================================

    /// @brief Set number of iterations for benchmarks
    /// @param iterations Number of iterations (default: 1000)
    void setIterations(int iterations) { m_iterations = iterations; }

    /// @brief Get current iteration count
    int iterations() const { return m_iterations; }

    /// @brief Set warmup iterations (not measured)
    /// @param warmup Number of warmup iterations (default: 100)
    void setWarmupIterations(int warmup) { m_warmupIterations = warmup; }

    // =========================================================================
    // Individual Benchmarks
    // =========================================================================

    /// @brief Benchmark cursor left/right navigation
    /// @return Benchmark result
    BenchmarkResult benchCursorLeftRight();

    /// @brief Benchmark cursor up/down navigation
    /// @return Benchmark result
    BenchmarkResult benchCursorUpDown();

    /// @brief Benchmark cursor word navigation (Ctrl+Left/Right)
    /// @return Benchmark result
    BenchmarkResult benchCursorWordNavigation();

    /// @brief Benchmark single character insertion
    /// @return Benchmark result
    BenchmarkResult benchCharacterInsertion();

    /// @brief Benchmark backspace deletion
    /// @return Benchmark result
    BenchmarkResult benchBackspaceDeletion();

    /// @brief Benchmark delete key deletion
    /// @return Benchmark result
    BenchmarkResult benchDeleteKeyDeletion();

    /// @brief Benchmark rapid typing simulation (typing at 60 WPM)
    /// @return Benchmark result
    BenchmarkResult benchRapidTyping();

    /// @brief Benchmark scroll performance
    /// @return Benchmark result
    BenchmarkResult benchScrolling();

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /// @brief Run all benchmarks
    /// @return Vector of all benchmark results
    std::vector<BenchmarkResult> runAll();

    /// @brief Run cursor-related benchmarks only
    /// @return Vector of cursor benchmark results
    std::vector<BenchmarkResult> runCursorBenchmarks();

    /// @brief Run editing-related benchmarks only
    /// @return Vector of editing benchmark results
    std::vector<BenchmarkResult> runEditingBenchmarks();

    /// @brief Get last benchmark results
    /// @return Vector of last run results
    const std::vector<BenchmarkResult>& lastResults() const { return m_lastResults; }

    // =========================================================================
    // Reference Values (Word 2019 typical performance)
    // =========================================================================

    /// @brief Get reference value for comparison
    /// @param benchmarkName Name of benchmark
    /// @return Reference ops/second (0 if unknown)
    static double referenceOpsPerSecond(const QString& benchmarkName);

signals:
    /// @brief Emitted when benchmark starts
    /// @param name Benchmark name
    void benchmarkStarted(const QString& name);

    /// @brief Emitted when benchmark completes
    /// @param result Benchmark result
    void benchmarkCompleted(const BenchmarkResult& result);

    /// @brief Emitted for progress updates
    /// @param current Current iteration
    /// @param total Total iterations
    void progressUpdated(int current, int total);

private:
    /// @brief Generic benchmark runner
    /// @param name Benchmark name
    /// @param setup Setup function (called once before benchmark)
    /// @param operation Operation to benchmark (called m_iterations times)
    /// @param teardown Teardown function (called once after benchmark)
    /// @return Benchmark result
    BenchmarkResult runBenchmark(
        const QString& name,
        std::function<void()> setup,
        std::function<void()> operation,
        std::function<void()> teardown = nullptr);

    /// @brief Prepare editor for benchmarking
    /// @param textContent Initial text content
    void prepareEditor(const QString& textContent);

    /// @brief Restore editor state after benchmark
    void restoreEditor();

    BookEditor* m_editor;                    ///< Editor to benchmark
    int m_iterations{1000};                  ///< Benchmark iterations
    int m_warmupIterations{100};             ///< Warmup iterations
    std::vector<BenchmarkResult> m_lastResults;  ///< Last benchmark results

    // State backup for restore
    QString m_savedContent;                  ///< Saved editor content
    int m_savedCursorPara{0};                ///< Saved cursor paragraph
    int m_savedCursorOffset{0};              ///< Saved cursor offset
};

} // namespace editor
} // namespace kalahari
