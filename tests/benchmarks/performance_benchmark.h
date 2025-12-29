/// @file performance_benchmark.h
/// @brief Performance benchmark framework for editor tests
///
/// OpenSpec #00043 Phase 10 - Performance Benchmark Infrastructure
/// Provides a framework for running and measuring performance benchmarks
/// with statistical analysis (min, max, avg, median, percentiles).

#pragma once

#include <QString>
#include <QElapsedTimer>
#include <functional>
#include <vector>
#include <algorithm>

namespace kalahari::benchmarks {

/// @brief Result of a performance benchmark
struct BenchmarkResult {
    QString name;            ///< Benchmark name
    qint64 minNs = 0;        ///< Minimum time in nanoseconds
    qint64 maxNs = 0;        ///< Maximum time in nanoseconds
    qint64 avgNs = 0;        ///< Average time in nanoseconds
    qint64 medianNs = 0;     ///< Median time in nanoseconds
    qint64 p95Ns = 0;        ///< 95th percentile in nanoseconds
    qint64 p99Ns = 0;        ///< 99th percentile in nanoseconds
    int iterations = 0;      ///< Number of iterations run
    qint64 targetNs = 0;     ///< Target time in ns (0 = no target)
    bool passedTarget = true; ///< Whether the benchmark passed its target

    /// @brief Get average time in milliseconds
    double avgMs() const { return avgNs / 1000000.0; }

    /// @brief Get median time in milliseconds
    double medianMs() const { return medianNs / 1000000.0; }

    /// @brief Get minimum time in milliseconds
    double minMs() const { return minNs / 1000000.0; }

    /// @brief Get maximum time in milliseconds
    double maxMs() const { return maxNs / 1000000.0; }

    /// @brief Get 95th percentile in milliseconds
    double p95Ms() const { return p95Ns / 1000000.0; }

    /// @brief Get 99th percentile in milliseconds
    double p99Ms() const { return p99Ns / 1000000.0; }

    /// @brief Get target time in milliseconds
    double targetMs() const { return targetNs / 1000000.0; }

    /// @brief Generate a summary string for the result
    /// @return Human-readable summary
    QString summary() const;

    /// @brief Generate a detailed report string
    /// @return Detailed human-readable report
    QString detailedReport() const;
};

/// @brief Framework for running performance benchmarks
///
/// Usage:
/// @code
/// PerformanceBenchmark benchmark;
///
/// auto result = benchmark.run("My Operation", []() {
///     // Operation to benchmark
/// }, 100, 10, BENCHMARK_TARGET_MS(16));
///
/// if (!result.passedTarget) {
///     std::cerr << "FAILED: " << result.summary().toStdString() << std::endl;
/// }
/// @endcode
class PerformanceBenchmark {
public:
    /// @brief Run a benchmark with warmup
    /// @param name Benchmark name for reporting
    /// @param operation Function to benchmark
    /// @param iterations Number of iterations to run
    /// @param warmupIterations Number of warmup iterations (not measured)
    /// @param targetNs Target time in nanoseconds (0 = no target)
    /// @return Benchmark result with statistics
    BenchmarkResult run(
        const QString& name,
        std::function<void()> operation,
        int iterations = 100,
        int warmupIterations = 10,
        qint64 targetNs = 0
    );

    /// @brief Run a benchmark with setup/teardown for each iteration
    /// @param name Benchmark name for reporting
    /// @param setup Function called before each operation (not measured)
    /// @param operation Function to benchmark
    /// @param teardown Function called after each operation (not measured)
    /// @param iterations Number of iterations to run
    /// @param targetNs Target time in nanoseconds (0 = no target)
    /// @return Benchmark result with statistics
    BenchmarkResult runWithSetup(
        const QString& name,
        std::function<void()> setup,
        std::function<void()> operation,
        std::function<void()> teardown,
        int iterations = 100,
        qint64 targetNs = 0
    );

    /// @brief Run a benchmark comparing two implementations
    /// @param name Benchmark name for reporting
    /// @param baseline Baseline implementation
    /// @param optimized Optimized implementation
    /// @param iterations Number of iterations to run
    /// @return Pair of results (baseline, optimized)
    std::pair<BenchmarkResult, BenchmarkResult> runComparison(
        const QString& name,
        std::function<void()> baseline,
        std::function<void()> optimized,
        int iterations = 100
    );

    /// @brief Print results to console
    /// @param results Vector of benchmark results
    static void printResults(const std::vector<BenchmarkResult>& results);

    /// @brief Print a single result to console
    /// @param result Benchmark result
    static void printResult(const BenchmarkResult& result);

    /// @brief Print comparison results to console
    /// @param name Comparison name
    /// @param baseline Baseline result
    /// @param optimized Optimized result
    static void printComparison(
        const QString& name,
        const BenchmarkResult& baseline,
        const BenchmarkResult& optimized
    );

    /// @brief Format nanoseconds to human-readable string
    /// @param ns Time in nanoseconds
    /// @return Human-readable time string
    static QString formatTime(qint64 ns);

    /// @brief Format time with speed indicator
    /// @param ns Time in nanoseconds
    /// @param targetNs Target time in nanoseconds
    /// @return Time string with PASS/OK/SLOW/FAIL indicator
    static QString formatSpeed(qint64 ns, qint64 targetNs);

private:
    /// @brief Calculate percentile from sorted vector
    /// @param times Sorted vector of times
    /// @param percentile Percentile (0.0 - 1.0)
    /// @return Time at the given percentile
    static qint64 calculatePercentile(std::vector<qint64>& times, double percentile);

    /// @brief Calculate statistics from time measurements
    /// @param times Vector of measured times
    /// @param result Result to populate
    static void calculateStatistics(std::vector<qint64>& times, BenchmarkResult& result);
};

/// @brief Helper macro to convert milliseconds to nanoseconds for targets
#define BENCHMARK_TARGET_MS(ms) (static_cast<qint64>(ms) * 1000000LL)

/// @brief Helper macro to convert target FPS to nanoseconds per frame
#define BENCHMARK_TARGET_FPS(fps) (1000000000LL / static_cast<qint64>(fps))

/// @brief Target times for common performance requirements
namespace Targets {
    /// @brief 60 FPS frame time (~16.67ms)
    constexpr qint64 FRAME_60FPS = BENCHMARK_TARGET_FPS(60);

    /// @brief 30 FPS frame time (~33.33ms)
    constexpr qint64 FRAME_30FPS = BENCHMARK_TARGET_FPS(30);

    /// @brief Interactive response time (50ms)
    constexpr qint64 INTERACTIVE = BENCHMARK_TARGET_MS(50);

    /// @brief Acceptable response time (100ms)
    constexpr qint64 ACCEPTABLE = BENCHMARK_TARGET_MS(100);

    /// @brief Maximum acceptable latency for typing (16ms)
    constexpr qint64 TYPING_LATENCY = BENCHMARK_TARGET_MS(16);

    /// @brief Maximum acceptable time for Select All (50ms)
    constexpr qint64 SELECT_ALL = BENCHMARK_TARGET_MS(50);

    /// @brief Maximum acceptable time for Copy (100ms)
    constexpr qint64 COPY = BENCHMARK_TARGET_MS(100);

    /// @brief Maximum acceptable document load time (2000ms)
    constexpr qint64 DOCUMENT_LOAD = BENCHMARK_TARGET_MS(2000);
}

} // namespace kalahari::benchmarks
