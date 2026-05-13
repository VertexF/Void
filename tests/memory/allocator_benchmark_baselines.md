# Allocator Benchmark Baselines

Configuration:
- Build: `out\build\x64-Release`
- Command: `.\out\build\x64-Release\VoidAllocatorBenchmarks.exe --runs=3 --gate`
- Date: 2026-05-13
- Workloads: fixed 64-byte alloc/free, mixed realloc churn, arena reset allocation, threaded alloc/free

Current expected median ranges on this machine:

| Allocator | Fixed 64B | Mixed realloc churn | Arena reset | Threaded 64B |
| --- | ---: | ---: | ---: | ---: |
| Malloc | 2.7-3.2 ms | 19.0-22.0 ms | skipped | 4.3-5.5 ms |
| Aligned64 | 4.0-6.0 ms | 18.0-22.0 ms | skipped | skipped |
| Binned | 2.2-3.0 ms | 19.0-23.0 ms | skipped | 21.0-28.0 ms |
| TLSF-64MB | 1.2-1.4 ms | 15.0-17.0 ms | skipped | 21.0-28.0 ms |
| Buddy-64MB | 2.3-3.1 ms | 360.0-390.0 ms | skipped | 55.0-260.0 ms |
| Pool-256x65536 | 1.5-2.0 ms | 6.0-7.5 ms | skipped | 19.0-28.0 ms |
| Linear-64MB | skipped | skipped | 0.4-0.8 ms | skipped |
| Frame-64MB | skipped | skipped | 0.4-0.8 ms | skipped |
| Stack-64MB | skipped | skipped | 1.4-2.0 ms | skipped |
| Monotonic | skipped | skipped | 5.8-7.2 ms | skipped |
| ThreadSafe | 2.8-3.5 ms | 19.0-22.0 ms | skipped | 4.0-6.0 ms |
| ThreadSafeLinear-64MB | skipped | skipped | 3.4-4.2 ms | skipped |
| ThreadLocalLinear-64MB | skipped | skipped | 1.4-1.9 ms | skipped |
| TLSCaching | 4.0-4.6 ms | 21.0-24.0 ms | skipped | 3.5-6.5 ms |
| Tracked | 6.0-9.0 ms | 25.0-28.0 ms | skipped | 38.0-80.0 ms |
| Debug | 3.2-4.0 ms | 12.0-15.0 ms | skipped | 17.0-40.0 ms |
| Secured | 1.2-2.6 ms | 4.2-8.0 ms | skipped | skipped |

Gate thresholds are intentionally wider than current medians to catch regressions without failing on normal workstation noise. Buddy mixed realloc churn and Buddy threaded alloc/free are current performance risks; their gates track measured behavior so regressions remain visible during allocator tuning.
