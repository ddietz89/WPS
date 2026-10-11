# WPS optimization: findings and proposed next steps

Updated 11 October 2026. This is the consolidated report; it supersedes older candidate promotion decisions, while preserving the scope of their original measurements.

## Current outcome

The private WPS fork now includes six source improvements, a four-thread Ungrib default, and a launcher selecting compatible zlib-ng for Metgrid. The strongest measured improvements are concurrent processing of independent valid times and lossless output compression. Several source changes accelerate isolated kernels without a large or repeatable whole-stage gain.

The new Metgrid interior interpolation fast path is included by user direction. Its source exactly matches the previously tested candidate. No new builds, model runs or scientific tests were launched for this promotion. The combined configuration containing all recent promotions has not yet been validated; historical timings below apply to the configurations actually measured.

Production `/pool/wrf/WPS` remains a read-only reference. No production source or original scientific checker has been changed.

## Baselines and measurement scope

- WPS: the user's private fork, WPS 4.7.0; starting commit `5feccecd63384381b6942371c7a837f66e4ccb84`.
- WRF: the user's chosen WRF 4.8.0 fork and retained experimental I/O libraries.
- Retained three-change WPS control: `bb0d14c58f4bbb067cf53143fad90df6284c0353`. Geogrid SIMD, Ungrib OpenMP/default and zlib-ng launcher were added in `7aac02d`; interpolation is added with this report.
- Main short fixture: native 0.25-degree GFS, 15 March 2026 00 UTC cycle, valid times 00/06/12 UTC; one 1500 × 1140 Lambert domain at 2.5 km spacing. A complete pipeline produces seven products: one geography, three intermediate and three Metgrid files.
- Latest experiments ran only on hpc01: dual EPYC 7742, 128 physical cores, eight NUMA nodes and approximately 125 GiB RAM. Earlier hpc02–04 results are retained historical evidence.
- GNU 13.3, AOCC 5.2 and Intel 2025.3 participate in relevant original-versus-candidate kernel checks. These checks do not establish cross-compiler bitwise equality or approve numerical tolerances.
- Strict floating-point behavior is retained: no fast math, reassociation or contraction relaxation. Scientific checks compare complete field inventory, values, types, dimensions and metadata; dependency trials also check HDF5 chunk/filter settings.
- Timings use explicit physical-core placement and bounded cgroups. Shared cache/storage/load variation remains possible. MPI core binding exposes both SMT siblings of a physical core; that is not an independently measured SMT speedup.
- MPI uses the reviewed UCX shared-memory/IB configuration. Single-node messages use shared memory; distributed transport should use native IB. XPMEM remains an available, separately authorized transport option, rather than an explanation for these source gains.

## Included improvements

| Change | What it does | Measured evidence | Practical interpretation |
|---|---|---|---|
| Ungrib GBYTES reservoir | Reuses packed bytes and leftover bits; guarded original fallback | Approximately 24% lower complete decode-kernel time in the earlier experiment | Useful retained integer optimization; not a 24% whole-Ungrib claim |
| Ungrib bulk slab writes | Writes contiguous array sections instead of scalar runtime transfers; strided fallback | Approximately 76% lower slab-write kernel time | Reduces Fortran I/O overhead while retaining record format |
| Metgrid selective mask discovery | Reads headers and materializes mask fields/aliases; uncertain records retain full reads | Approximately 9.95% lower stage wall in one independent pair, exact products | Promising source-level I/O reduction; single-pair magnitude is provisional |
| Geogrid SIMD reader | Separate strict conversion/scaling helper for eligible 1-/2-byte tiles; legacy reader stays O2 | 15,264 exact kernel/reader cases across compiler families; 2.11–6.62× kernel gain; all 12 stage products exact | Included, but no established stage-speedup claim |
| Ungrib independent-group OpenMP | Parallelizes eligible complex-packing groups with private scratch/disjoint output; serial fallback for unsupported or dependent cases | Exact caller checks across compiler families; fresh four-thread kernel screen approximately 1.92×; earlier warm stage pair 3.58% faster with 36.4% more CPU | Included with four threads by default; explicit OMP_NUM_THREADS overrides it |
| Metgrid interior sixteen-point interpolation | Gathers an eligible unmasked interior stencil without repeated clipping/mask dispatch; retains original oned arithmetic and fallbacks | 900,000 exact comparisons per compiler family; 1.17–1.25× kernel gain; stage 193.61 → 190.27 s, exact outputs | Included now; 1.73% stage reduction is one pair, not a reliable general percentage |
| Metgrid zlib-ng selection | Job-local compatible zlib-ng 2.3.3, retaining shuffle, chunks and compression level 2 | Five stage pairs: 14.7–22.1% less wall, median 17.7%; median CPU 19.8% lower; all 12 outputs exact | Strongest repeatedly measured dependency gain; files approximately 0.23% larger |

WPS shares this module: `metgrid/src/interp_module.F` is a symlink to `geogrid/src/interp_module.F`. The change preserves that structure and therefore applies to both stages when rebuilt. Existing exact kernel checks exercise the shared implementation, but the prior stage-speedup evidence covers Metgrid. Geogrid stage acceptance with this interpolation change is part of the pending combined check.

The interpolation source SHA256 is `97d7e2edcdaabc64cad379173591fe9eac9e7c5bc1a343e8913db602bd1dd765`. Its oracle covers signed coordinates, integer thresholds, edges/corners, missing values, zeros, negative zeros, tiny values, NaNs, optional masks and fallback sequences. The original five oned calls preserve per-point arithmetic order. This is a scalar fast path, not a newly vectorized interpolation algorithm.

The Geogrid helper's large kernel gain does not imply a comparable stage gain: its warm benchmark isolates conversion/scaling and excludes tile I/O, projections, interpolation, categorical work, communication and output. Five stage pairs were mixed: three slower, two faster; median paired change was 0.09% slower. The actual critical-path fraction saved has not been established.

Runtime selection: Ungrib defaults to four threads only when OMP_NUM_THREADS is absent or empty. Use OMP_NUM_THREADS=1 for serial controls. `tools/metgrid` selects the tested zlib-ng compatibility library; WPS_ZLIB_NG_LIBRARY overrides its location and WPS_USE_STOCK_ZLIB=1 skips adding the preload. Direct metgrid.exe invocation retains its normal linked dependency selection. No system zlib was replaced.

## End-to-end and scaling results

The original three retained source changes passed seven fresh pipeline product comparisons against their control, and that control passed against the scientific reference: 14 exact comparisons. One sequential pair improved 277.23 → 224.20 seconds, or 19.1%. This is a short, single-pair result with cache/load limitations; individual kernel gains cannot be added to it.

The later execution trial used the same retained three-change executables throughout. It compared six fresh pipelines within the same 16-physical-core budget, with two order-reversed pairs. The new Geogrid SIMD, Ungrib OpenMP/default and interpolation fast path were absent from those executables.

| Layout | Pair 0 wall / CPU seconds | Pair 1 wall / CPU seconds |
|---|---:|---:|
| Sequential stages, 16-rank geography → serial Ungrib → 16-rank Metgrid, stock zlib | 282.458 / 3235.995 | 256.708 / 3175.436 |
| Concurrent prerequisites, then three independent four-rank Metgrid workers, stock zlib | 121.118 / 1182.825 | 117.951 / 1169.170 |
| Same concurrent layout, zlib-ng for Metgrid | 107.718 / 1020.645 | 114.833 / 1022.830 |

Concurrent execution reduced wall 54–57% and CPU about 63%. Within that concurrent layout, zlib-ng reduced wall a further 2.6–11.1% and CPU 12.5–13.7%. All 42 products passed pinned exact scientific checks. Complete-file hash reuse was permitted only after full equality had been established. These two pairs demonstrate a useful short-pipeline result, with variable magnitude; they do not forecast a full production cycle.

Total seven-product size increased approximately 0.115% with zlib-ng. The cumulative cgroup high-water mark of approximately 36.8 GiB includes cache retained across all six pipelines, so it is not a per-layout RSS comparison. Single-stage zlib-ng maximum-process RSS was essentially unchanged.

Controlled spatial MPI stage medians, with serial worker threads:

| Ranks | Geogrid wall seconds | Metgrid wall seconds, one valid time |
|---:|---:|---:|
| 1 | 40.500 | 115.810 |
| 4 | 22.921 | 80.238 |
| 16 | 20.053 | 67.884 |

All 18 stage-screen products passed exact checks. Wider MPI teams reduced individual latency but scaled inefficiently: at 16 ranks, speedup was only 2.02× for Geogrid and 1.71× for Metgrid. Independent valid-time workers therefore offer better measured throughput at the tested budget. A historical 48-core cluster run finished in 89.75 seconds; it is not comparable to the fixed-16-core result.

## Bottlenecks identified

One instrumented 16-rank Metgrid run recorded rank 0 spending 32.83 seconds in 88 float output writes and 5.84 seconds in 51 float reads, within 60.67 seconds rank runtime. Output is NetCDF4/HDF5 with shuffle and zlib level 2. This identifies output as a leading critical-path cost, but does not attribute every write second to compression.

Other ranks spent long periods in MPI sends/broadcasts while root performed serial work. These are partly waiting/imbalance costs, not evidence that IB is slow. Aggregate instruction counters include polling and cannot establish useful interpolation throughput.

Every rank independently materializes full meteorological slabs: approximately 818 MB logical payload per valid time, or roughly 13.1 GB requested by 16 ranks before extra copies. Filesystem caches can greatly reduce physical traffic. Sharing materialization is promising, but its benefit is unmeasured.

A 15-second compute-phase sample attributed about 39% of sampled cycles to sixteen-point interpolation and 14% to oned. Those are windowed samples, not whole-stage time fractions. Exact point batching is a concrete next arithmetic target after output work.

## Candidates held or rejected

| Candidate | Result | Disposition |
|---|---|---|
| Positive 32-bit bitarray shifts/masks | 27.36 million exact/storage checks; 1.30× query kernel, stage 181.16 → 181.28 s | Held; no useful stage change established |
| Portable byte-aligned Ungrib width-6/7/8 block decoder | Exact across compiler families, including guarded tails; every caller pair 4.64–5.17% slower | Rejected |
| BMI2 PEXT extraction on Zen 2 | Exact instruction probe, actual PEXT confirmed; 2.51–2.92× slower than shifts/masks | Rejected |
| Blanket decoder CPU/compiler tuning | O2 Zen 2 approximately 1.34% slower; O3 Zen 2 approximately 5.36% slower | Retain strict O2 for that code |
| Metgrid row OpenMP, four MPI ranks × four threads | 177.39 versus 170.01 s for 16 MPI ranks × one thread, same core budget | Rejected configuration |
| Cheap LATLON coordinate cache | Approximately 18.7% slower kernel | Rejected |
| Geogrid reader OpenMP | Weak stage gain at unequal core budgets; small-tile regressions | Held; separate SIMD helper is included instead |
| First halo-face collective fallback | Universal caller participation/order not established | Held; avoid unsafe collectives |

AOCC and Intel remain eligible experimental compiler stacks. Their per-compiler source checks pass for the relevant candidates. Proposed field-specific tolerances have not been approved, and no cross-compiler numerical difference is accepted by silently loosening exact checks.

## Proposed next steps

### 1. One combined integration check when testing resumes

Build the promoted branch in a separate project build, then run one fresh short pipeline against the existing pinned reference. This addresses interactions/build wiring among the recent promotions; do not rerun broad compiler, rank or thread matrices. If exact science passes, use a small number of paired pipeline measurements to check for a material regression. No such run was launched as part of this promotion.

For a 16-core concurrent prerequisite phase, allocate four Geogrid ranks plus three four-thread Ungrib processes on disjoint physical cores. After prerequisites finish, run three four-rank Metgrid time workers. Explicitly bind/count each Ungrib team; simply inheriting old one-core worker placement would oversubscribe it. Preserve native IB/UCX and bounded memory/I/O concurrency.

### 2. Launch three bounded source investigations in parallel

Use existing Codex component agents for ownership and review, with bounded DeepSeek implementation offloading under the standing source-payload authorization. Root owns host allocations, acceptance and integration. Isolate actual hotspots into standalone kernels/protocol harnesses before whole-stage experiments.

| Lane | First target | Smallest useful experiment | Stop condition |
|---|---|---|---|
| Metgrid, highest priority | Reusable root-owned gather/output scratch, then bounded overlap of gathering/preparation with one output owner | Actual 2-/4-rank gather oracle; measure allocation/packing/gather; prove immutable queue buffers, I/O ownership and supported thread level before asynchronous output | Negligible measured contribution, invalid interface/order/thread assumptions, or memory/copy overhead erases benefit |
| Geogrid | Interchange only the two independent integer initialization nests to make dimension 1 contiguous | Actual-source oracle for bounds/guards/untouched plane and odd/negative/empty ranges; cheap alternating timings | Flat kernel or negligible contribution; do not reorder categorical REAL accumulation |
| Ungrib | Profile complete-caller time and decoded-value/byte-weighted width/alignment eligibility | Small instrumented replay of representative complex-packing fields, then at most one eligible integer AVX2 decoder prototype | No substantial eligible time share, overread/exactness failure, or complete-caller regression |

Avoid repeating the rejected portable block or PEXT experiments unchanged. Workers use bounded contracts, denied tool access and a 240-second implementation limit; allow at most one targeted repair before root review. Current project compute remains restricted to hpc01; hpc00 is the gateway and old hpc02–04 runs remain evidence only.

### 3. Follow Metgrid evidence with input sharing and exact point batches

Once output ownership work is bounded, investigate materializing each slab once per node. Prove rank participation/header order, preserve malformed/truncated input errors and metadata, and compare distribution/copy/NUMA costs with independent cached reads. Do not introduce a collective on conditional rank paths or broadcast pointer-bearing derived types.

Then isolate one dense interpolation branch and batch independent destination points. Keep each point's original expression order, missing/mask thresholds and fallback; write packed validity words under explicit ownership. Require actual-source exact cases and a useful complete-caller result before stage testing. SIMD directives alone are not evidence of useful AVX2.

### 4. Extend acceptance once the combined short configuration is sound

Run the 25-time integration window, then the 49-time f000–f144 production window and downstream WRF initialization/forecast validation. Use modest MPI teams and a bounded valid-time queue; measure filesystem/output contention and memory before widening concurrency. Longer-window and downstream acceptance remain outstanding. Do not treat the short-cycle gains as a completed production qualification.

## Evidence locations

All paths below are on hpc01 unless identified as fork files. They identify retained evidence; the report does not require following them to understand the findings.

- Interpolation: `/pool/wrf-experiment/results/m6/metgrid-kernel-oracle.json`, `metgrid-aocc-kernels.json`, `metgrid-oneapi-kernels.json`, `metgrid-kernel-benchmark.json`, `metgrid-short-stage.json`, `metgrid-short-comparison.json`.
- Exact promoted interpolation source: `/pool/wrf-experiment/builds/m5/metgrid-interior/gnu/WPS/metgrid/src/interp_module.F`.
- Original integrated source trial: `/pool/wrf-experiment/results/m6/integrated-short-performance.json`, `integrated-short-comparison.json`.
- Latest CPU trial root: `/pool/wrf-experiment/results/cpu-investigation/`.
- Fixed-budget execution: `integrated-pair-fixed16-r1.json` and `integrated-pair-fixed16-r1-exact.json` under that root; timing receipt retains its original executed-pending-checks status, while the separate exact receipt records PASS.
- MPI screen: `stage-screen.json`, `stage-screen-exact.json`; Geogrid SIMD: `geogrid-reader-stage-exact.json` and associated reader timing receipts.
- Compression: `metgrid-zlib-ng-kernel.json`, `metgrid-zlib-ng-stage.json`, `metgrid-zlib-ng-stage-exact.json`.
- Profiling: `metgrid-pmpi.json`, `metgrid-io-pmpi.json`; Ungrib placement screen: `ungrib-openmp/screen.json`; rejected BMI probe: `bmi-probe-r1.json`.
- Fork runtime/build documentation: `PERFORMANCE_NOTES.md`; Metgrid dependency launcher: `tools/metgrid`.

The repository contains the promoted source and this report. Existing protected remote executables have not been rebuilt or replaced by this documentation/promotion step.
