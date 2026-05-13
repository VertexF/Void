# Container / Allocator Benchmark Baselines

Configuration:
- Build: `out\build\x64-Release`
- Command: `.\out\build\x64-Release\VoidContainerBenchmarks.exe`
- Toolchain line reported by benchmark: `MSVC /O2 /EHsc /DNDEBUG`
- Data shape: `int`, median of 7 runs, `1,000,000` large elements, `200,000 x 32` small arrays

Expected median ranges from the current baseline:

| Allocator | Small arrays legacy | Small arrays `Array<int,32>` | Small arrays `ChunkedArray<int,32>` |
| --- | ---: | ---: | ---: |
| System | 17.0-18.0 ms | 3.3-3.6 ms | 13.0-14.0 ms |
| Malloc | 24.0-25.0 ms | 3.3-3.6 ms | 16.5-17.5 ms |
| Aligned64 | 20.0-21.5 ms | 3.3-3.7 ms | 14.5-15.5 ms |
| Binned | 26.0-28.0 ms | 3.3-3.7 ms | 17.5-18.8 ms |
| TLSF-128MB | 20.5-22.0 ms | 3.3-3.7 ms | 14.8-15.8 ms |
| Buddy-128MB | 40.0-44.0 ms | 4.2-4.8 ms | 25.0-27.5 ms |
| Linear-128MB | 11.0-12.0 ms | 3.0-3.8 ms | 10.5-15.0 ms |
| TLSCaching | 8.0-10.5 ms | 3.3-7.5 ms | 8.5-17.5 ms |
| Tracked | 80.0-125.0 ms | 2.9-3.1 ms | 47.0-57.0 ms |
| Debug | 116.0-131.0 ms | 4.2-4.5 ms | 60.0-83.0 ms |

Large-array expected ranges:
- Reserved push + sum: most allocators should stay in the 0.9-1.5 ms median band for `Array PushBack`.
- Reserved fill + sum: `Array AddUninitialized` should stay in the 0.7-1.5 ms median band.
- Clear + refill: `Array AppendWriter` should stay in the 3.1-3.7 ms median band for general-purpose allocators.

Notes:
- Secured allocator intentionally skips allocator-model-mismatched small-array and chunked scenarios.
- Debug and Tracked small-array legacy paths are expected to remain much slower because every heap allocation is intentionally diagnostic-heavy.
