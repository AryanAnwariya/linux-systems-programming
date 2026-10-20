# POSIX Threads

## report-card

Reads a marks matrix (subjects × students) from `input.txt`, then:

1. **Aggregates in parallel.** One thread per student computes average, minimum and maximum.
2. **Sorts in parallel.** The user picks a sort key (average, min, max or a subject) and *k* threads. Students are split into *k* nearly equal segments, each thread insertion-sorts its own segment, and the main thread does a k-way merge.
3. Offers a menu to look up one student or print the sorted list.

```bash
cd report-card && ../../build/report_card
```

Concepts: `pthread_create`/`pthread_join`, splitting data so threads never write to the same memory (no locks needed), passing per-thread parameters, k-way merge.

## sudoku-analyzer

Checks a 9×9 Sudoku grid (`.` or `0` for blanks) and explains what's wrong with it.

1. **27 validator threads**, one per row, column and 3×3 box. Each returns a heap-allocated result through `pthread_exit` with bitmasks of missing and duplicated digits.
2. Prints `VALID` or `INVALID` with a validity score (the share of valid units).
3. **9 analysis threads**, one per box, build a **conflict heatmap**: each cell's score is how many of its three units have a duplicate of its value (blanks score 3). Score-3 cells are listed as hotspots.
4. A **missing-digit report** suggests a fix when a unit has exactly one blank and one missing digit.

```bash
../build/sudoku_analyzer sudoku-analyzer/grids/grid_blanks.txt
```

```
INVALID
Validity Score: 77.78%
...
  Row      1 : missing {1}  blanks=1
      -> repair: place 1 at (1,3)
```

Concepts: returning results from threads via `pthread_exit`/`pthread_join`, bitmask sets, two-phase parallelism where phase 2 reads phase 1's output only after a join.
