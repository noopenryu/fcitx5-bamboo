# Native engine checks

The native test suite includes parity fixtures and repeated-key regressions for
Telex, Telex 2, and VNI, with automatic restoration enabled and disabled. The
repeated-key cases check composition, backspace, commits, restoration, reset, and
typing Vietnamese afterward. The Fcitx integration test also exercises repeated
presses with interleaved releases.

Build and run the optional performance benchmark from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBAMBOO_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bamboo/native/benchmark-bamboo-repeat
```

The benchmark prints CSV timings for 32–1,024 consecutive `k` or `a` presses in
each input method, with automatic restoration enabled and disabled. Initialization
is excluded from the timings. Compare Release builds on the same machine without
other CPU-intensive jobs. Timings are deliberately not CI assertions.

Before the held-key fix, default Telex took about 5.6 seconds for 256 `k` presses
and 4.1 seconds for 256 `a` presses. On the same machine, the optimized engine took
about 50 ms and 19 ms, respectively. Doubling longer runs should take roughly four
times as long overall, rather than the previous roughly eight times. Complete
preedit text is retained, so rendering and publishing it still takes work
proportional to its length.
