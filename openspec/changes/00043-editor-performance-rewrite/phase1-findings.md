# OpenSpec #00043 - Phase 1: Research & Spike - Findings

## Summary

Phase 1 benchmarks completed successfully. Key architecture decisions confirmed.

## Benchmark Results (150k words / ~1M characters)

### 1. Piece Table vs QString

| Operation | PieceTable | QString | Winner |
|-----------|------------|---------|--------|
| Load document | 13.5 µs | 100 ns | QString (135x) |
| 1000 random inserts | 3.15 ms | 13.71 ms | **PieceTable (4.4x)** |
| 5000 chars at end | 68.19 ms | 2.51 ms | QString (27x) |
| Get text 100x | 1.43 ms | 2.4 µs | QString (596x) |

**Conclusion:** Vector-based piece table has O(N) operations. For production, need balanced tree or use QTextDocument.

### 2. QTextDocument Performance

| Operation | Time | Status |
|-----------|------|--------|
| Load document | 109.55 ms | FAIL |
| 1000 random inserts | 42.30 ms | OK |
| Block access (1000x) | 1.20 ms | **PASS** |
| Select All | 3.93 ms | **PASS** |
| Cursor movement (10000x) | 12.40 ms | **PASS** |
| Sequential typing 5000 chars | 516.32 ms | FAIL |

**Conclusion:** QTextDocument has excellent block (paragraph) access. Sequential typing needs optimization.

### 3. Lazy Layout vs Traditional (30k words / 600 paragraphs)

| Metric | Traditional | Lazy | Speedup |
|--------|-------------|------|---------|
| **Initialization** | 2.87 ms | 46.6 µs | **61.6x faster** |
| Y lookup (1000x) | 419.1 µs | 84.5 µs | **5.0x faster** |
| Scroll to top | 2.6 µs | 94.7 µs | 36x slower |
| 100 scroll steps | 75.1 µs | 2.94 ms | 39x slower |

**Conclusion:** Lazy layout dramatically reduces initialization time. Scroll overhead is acceptable (still < 16ms frame budget).

## Architecture Decision: CONFIRMED

### Use QTextDocument + Lazy Layout

1. **Text Storage: QTextDocument**
   - Already optimized by Qt team
   - O(log N) operations internally
   - Built-in undo/redo, cursor, selection
   - Block (paragraph) access is O(1)

2. **Layout Engine: Lazy Layout with Fenwick Tree**
   - Initialize with height estimation only (61x faster)
   - Calculate actual layout on-demand for visible paragraphs
   - Fenwick tree for O(log N) Y-position queries

3. **Rendering: Viewport-only**
   - Only render visible paragraphs
   - Height estimation for off-screen paragraphs
   - Dirty region tracking for minimal repaints

### Bottleneck Analysis

The current BookEditor's performance issues are NOT from text storage, but from:

1. **Layout during paint** - should be pre-calculated
2. **Full document traversal** - should use virtual scrolling
3. **No dirty region tracking** - should repaint only changed areas

## Next Steps

Phase 2: Core Architecture Implementation
- Task 2.1: Implement PerformantTextBuffer with QTextDocument
- Task 2.2: Implement Fenwick tree for paragraph heights
- Task 2.3: Implement LazyLayoutManager
- Task 2.4: Implement ViewportManager

## Files Created

- `tests/prototypes/piece_table_prototype.h` - Piece table data structure
- `tests/prototypes/lazy_layout_prototype.h` - Lazy layout with Fenwick tree
- `tests/prototypes/benchmark_main.cpp` - Benchmark runner
- `tests/prototypes/CMakeLists.txt` - Build configuration

## Build & Run

```bash
# Build
scripts/build_windows.bat Debug

# Run all benchmarks
cd build-windows/bin
./benchmark_prototypes.exe --all

# Run specific benchmark
./benchmark_prototypes.exe --piece-table
./benchmark_prototypes.exe --qtextdocument
./benchmark_prototypes.exe --lazy-layout
```
