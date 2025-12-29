/// @file performance_benchmark.cpp
/// @brief Implementation of performance benchmark framework
///
/// OpenSpec #00043 Phase 10 - Performance Benchmark Infrastructure

#include "performance_benchmark.h"

#include <iostream>
#include <iomanip>
#include <numeric>

namespace kalahari::benchmarks {

// =============================================================================
// BenchmarkResult
// =============================================================================

QString BenchmarkResult::summary() const
{
    QString result = QString("%1: avg=%2, median=%3")
        .arg(name)
        .arg(PerformanceBenchmark::formatTime(avgNs))
        .arg(PerformanceBenchmark::formatTime(medianNs));

    if (targetNs > 0) {
        result += passedTarget ? " [PASS]" : " [FAIL]";
    }

    return result;
}

QString BenchmarkResult::detailedReport() const
{
    QString result;
    result += QString("Benchmark: %1\n").arg(name);
    result += QString("  Iterations: %1\n").arg(iterations);
    result += QString("  Min:    %1\n").arg(PerformanceBenchmark::formatTime(minNs));
    result += QString("  Max:    %1\n").arg(PerformanceBenchmark::formatTime(maxNs));
    result += QString("  Avg:    %1\n").arg(PerformanceBenchmark::formatTime(avgNs));
    result += QString("  Median: %1\n").arg(PerformanceBenchmark::formatTime(medianNs));
    result += QString("  P95:    %1\n").arg(PerformanceBenchmark::formatTime(p95Ns));
    result += QString("  P99:    %1\n").arg(PerformanceBenchmark::formatTime(p99Ns));

    if (targetNs > 0) {
        result += QString("  Target: %1\n").arg(PerformanceBenchmark::formatTime(targetNs));
        result += QString("  Status: %1\n").arg(passedTarget ? "PASS" : "FAIL");
    }

    return result;
}

// =============================================================================
// PerformanceBenchmark
// =============================================================================

BenchmarkResult PerformanceBenchmark::run(
    const QString& name,
    std::function<void()> operation,
    int iterations,
    int warmupIterations,
    qint64 targetNs)
{
    BenchmarkResult result;
    result.name = name;
    result.iterations = iterations;
    result.targetNs = targetNs;

    // Warmup phase
    for (int i = 0; i < warmupIterations; ++i) {
        operation();
    }

    // Measurement phase
    std::vector<qint64> times;
    times.reserve(iterations);

    QElapsedTimer timer;
    for (int i = 0; i < iterations; ++i) {
        timer.start();
        operation();
        times.push_back(timer.nsecsElapsed());
    }

    calculateStatistics(times, result);

    // Check target
    if (targetNs > 0) {
        result.passedTarget = (result.p95Ns <= targetNs);
    }

    return result;
}

BenchmarkResult PerformanceBenchmark::runWithSetup(
    const QString& name,
    std::function<void()> setup,
    std::function<void()> operation,
    std::function<void()> teardown,
    int iterations,
    qint64 targetNs)
{
    BenchmarkResult result;
    result.name = name;
    result.iterations = iterations;
    result.targetNs = targetNs;

    std::vector<qint64> times;
    times.reserve(iterations);

    QElapsedTimer timer;
    for (int i = 0; i < iterations; ++i) {
        setup();

        timer.start();
        operation();
        qint64 elapsed = timer.nsecsElapsed();

        teardown();

        times.push_back(elapsed);
    }

    calculateStatistics(times, result);

    // Check target
    if (targetNs > 0) {
        result.passedTarget = (result.p95Ns <= targetNs);
    }

    return result;
}

std::pair<BenchmarkResult, BenchmarkResult> PerformanceBenchmark::runComparison(
    const QString& name,
    std::function<void()> baseline,
    std::function<void()> optimized,
    int iterations)
{
    auto baselineResult = run(name + " (baseline)", baseline, iterations, 10, 0);
    auto optimizedResult = run(name + " (optimized)", optimized, iterations, 10, 0);

    return {baselineResult, optimizedResult};
}

void PerformanceBenchmark::printResults(const std::vector<BenchmarkResult>& results)
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "BENCHMARK RESULTS\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::cout << std::left
              << std::setw(35) << "Benchmark"
              << std::setw(12) << "Avg"
              << std::setw(12) << "Median"
              << std::setw(12) << "P95"
              << std::setw(10) << "Status"
              << "\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& result : results) {
        std::cout << std::left
                  << std::setw(35) << result.name.toStdString().substr(0, 34)
                  << std::setw(12) << formatTime(result.avgNs).toStdString()
                  << std::setw(12) << formatTime(result.medianNs).toStdString()
                  << std::setw(12) << formatTime(result.p95Ns).toStdString();

        if (result.targetNs > 0) {
            std::cout << (result.passedTarget ? "PASS" : "FAIL");
        } else {
            std::cout << "-";
        }
        std::cout << "\n";
    }

    std::cout << std::string(80, '=') << "\n";
}

void PerformanceBenchmark::printResult(const BenchmarkResult& result)
{
    std::cout << result.detailedReport().toStdString();
}

void PerformanceBenchmark::printComparison(
    const QString& name,
    const BenchmarkResult& baseline,
    const BenchmarkResult& optimized)
{
    std::cout << "\n" << std::string(60, '-') << "\n";
    std::cout << "Comparison: " << name.toStdString() << "\n";
    std::cout << std::string(60, '-') << "\n";

    std::cout << std::left
              << std::setw(15) << "Metric"
              << std::setw(15) << "Baseline"
              << std::setw(15) << "Optimized"
              << std::setw(15) << "Speedup"
              << "\n";

    auto printRow = [](const QString& metric, qint64 baseVal, qint64 optVal) {
        double speedup = static_cast<double>(baseVal) / optVal;
        std::cout << std::left
                  << std::setw(15) << metric.toStdString()
                  << std::setw(15) << formatTime(baseVal).toStdString()
                  << std::setw(15) << formatTime(optVal).toStdString()
                  << std::fixed << std::setprecision(2) << speedup << "x"
                  << "\n";
    };

    printRow("Avg", baseline.avgNs, optimized.avgNs);
    printRow("Median", baseline.medianNs, optimized.medianNs);
    printRow("P95", baseline.p95Ns, optimized.p95Ns);
    printRow("P99", baseline.p99Ns, optimized.p99Ns);

    std::cout << std::string(60, '-') << "\n";
}

QString PerformanceBenchmark::formatTime(qint64 ns)
{
    if (ns < 1000) {
        return QString("%1 ns").arg(ns);
    } else if (ns < 1000000) {
        return QString("%1 us").arg(ns / 1000.0, 0, 'f', 1);
    } else if (ns < 1000000000) {
        return QString("%1 ms").arg(ns / 1000000.0, 0, 'f', 2);
    } else {
        return QString("%1 s").arg(ns / 1000000000.0, 0, 'f', 3);
    }
}

QString PerformanceBenchmark::formatSpeed(qint64 ns, qint64 targetNs)
{
    QString timeStr = formatTime(ns);

    if (targetNs <= 0) {
        return timeStr;
    }

    double ratio = static_cast<double>(ns) / targetNs;

    if (ratio <= 0.5) {
        return timeStr + " [EXCELLENT]";
    } else if (ratio <= 1.0) {
        return timeStr + " [PASS]";
    } else if (ratio <= 2.0) {
        return timeStr + " [OK]";
    } else if (ratio <= 4.0) {
        return timeStr + " [SLOW]";
    } else {
        return timeStr + " [FAIL]";
    }
}

qint64 PerformanceBenchmark::calculatePercentile(std::vector<qint64>& times, double percentile)
{
    if (times.empty()) return 0;

    size_t index = static_cast<size_t>(percentile * (times.size() - 1));
    return times[index];
}

void PerformanceBenchmark::calculateStatistics(std::vector<qint64>& times, BenchmarkResult& result)
{
    if (times.empty()) {
        return;
    }

    // Sort for percentile calculations
    std::sort(times.begin(), times.end());

    // Min and max
    result.minNs = times.front();
    result.maxNs = times.back();

    // Average
    qint64 sum = std::accumulate(times.begin(), times.end(), 0LL);
    result.avgNs = sum / static_cast<qint64>(times.size());

    // Median (50th percentile)
    result.medianNs = calculatePercentile(times, 0.50);

    // 95th percentile
    result.p95Ns = calculatePercentile(times, 0.95);

    // 99th percentile
    result.p99Ns = calculatePercentile(times, 0.99);
}

} // namespace kalahari::benchmarks
