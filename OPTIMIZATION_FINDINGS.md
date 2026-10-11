# WPS optimization: findings and proposed next steps

Updated 11 October 2026. This is the consolidated report; it supersedes older candidate promotion decisions, while preserving the scope of their original measurements.

## Current outcome

The private WPS fork now includes six source improvements, a four-thread Ungrib default, and a launcher selecting compatible zlib-ng for Metgrid. The strongest measured improvements are concurrent processing of independent valid times and lossless output compression. Several source changes accelerate isolated kernels without a large or repeatable whole-stage gain.

The combined configuration now passes fresh short integration, including both rebuilt users of the shared interpolation source. All seven qualification products and all 28 products from four matched timing runs pass the pinned exact science and output-layout checks. The fresh 25-time pipeline also passes all 51 products. Three-hour cadence and native 49-time coverage remain in progress; a two-minute full-grid forecast resource probe completed. Historical timings below retain their original configuration scope.

Production `/pool/wrf/WPS` remains a read-only reference. No production source or original scientific checker has been changed.

## Baselines and measurement scope

- WPS: the user's private fork, WPS 4.7.0; starting commit `5feccecd63384381b6942371c7a837f66e4ccb84`.
- WRF: the user's chosen WRF 4.8.0 fork and retained experimental I/O libraries.
- Retained three-change WPS control: `bb0d14c58f4bbb067cf53143fad90df6284c0353`. Geogrid SIMD, Ungrib OpenMP/default and zlib-ng launcher were added in `7aac02d`; interpolation was added in `086c518`; `9e0ae2a` fixes recursive-make flag overrides so the rebuilt Ungrib main program and link retain OpenMP support.
- Main short fixture: native 0.25-degree GFS, 15 March 2026 00 UTC cycle, valid times 00/06/12 UTC; one 1500 × 1140 Lambert domain at 2.5 km spacing. A complete pipeline produces seven products: one geography, three intermediate and three Metgrid files.
- The next round uses hpc01–04 under the owner's later authorization. Each node has dual EPYC 7742 CPUs, 128 physical cores, eight NUMA nodes and approximately 125 GiB RAM. hpc00 remains the gateway; concurrent node work and shared NFS load are declared.
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

WPS shares this module: `metgrid/src/interp_module.F` is a symlink to `geogrid/src/interp_module.F`. The change preserves that structure and therefore applies to both stages when rebuilt. Existing exact kernel checks exercise the shared implementation, but the prior stage-speedup evidence covers Metgrid. Fresh combined acceptance now covers both Geogrid and Metgrid with this shared change; longer-window acceptance is separate.

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

## Next-round combined qualification and performance

The isolated build matches all 1,447 tracked files/symlinks at `9e0ae2a`. Both interpolation consumer objects were rebuilt. The initial build exposed ordinary Makefile appends being overridden by recursive-make command-line flags; the repair uses override appends for the Ungrib main object and link. The failed build is preserved, and protected executables remain unchanged.

Four fresh short pipelines ran on hpc01 with the same 16-physical-core budget, native inputs, output settings and job-local zlib-ng. The retained three-change control used explicitly serial Ungrib; the combined candidate used the verified four-thread default with disjoint four-core bindings. Geography overlapped three Ungrib workers; three four-rank Metgrid workers followed, leaving four cores unused during that phase.

| Order | Configuration | Pipeline wall seconds | Cgroup CPU seconds |
|---|---|---:|---:|
| 1 | Retained control | 151.174 | 1100.003 |
| 2 | Combined | 149.934 | 1062.253 |
| 3 | Combined | 149.835 | 1097.125 |
| 4 | Retained control | 158.264 | 1092.779 |

Paired wall reductions are 0.82% and 5.33%. Both favor the combined build, but the control spread and concurrent shared-storage work make the magnitude provisional. This check establishes no material regression; it does not support adding individual optimization percentages. All seven-product pipelines were 5,407,188,985 bytes. Download and full scientific-comparison time are excluded from these pipeline clocks. The existing runner includes its between-stage header/envelope integrity scan; this is shared by both arms and is not a model-only stage clock. All 28 products passed full pinned science and NetCDF chunk/filter checks; byte reuse was allowed only after an independently completed exact comparison.

The fresh full-grid, 25-time f000–f144/every-six-hours combined pipeline completed in 972.338 seconds, using 8,404.745 cgroup CPU seconds and producing 43,663,035,586 bytes across 51 WPS products. Its outer service duration was 1,123.495 seconds including input/preflight checks; these endpoints differ. The runner reported a 63,844,339,712-byte cgroup peak including cache and preflight pages; this is not process RSS. Native 49-time runs will defer duplicate intermediate parsing to the independent scientific gate, so their model-clock endpoint must be distinguished from this earlier audited pipeline clock. All 51 products now pass full pinned scientific and NetCDF layout checks. This is coverage evidence, not a matched production-speedup result, and it is not the native 49-time window.

Fresh baseline and combined WRF 4.8 `real.exe` runs independently passed the original short reference checks. Their initialization/boundary outputs are also byte-identical. Model walls were 254.75 and 280.61 seconds; no downstream speedup is claimed. The first two-minute forecast memory probe stopped at a missing forecast-only table alias before time stepping, without OOM. A fresh probe restored the required alias to the already frozen WRF table, preserving physics and resources. It completed in 531.31 seconds, reached approximately 00:02:05 with normal adaptive-step overshoot, and peaked at 70.284 GiB cgroup memory without OOM. It produced the initial full-grid history file with 275 variables. This is execution/resource coverage only: no forecast scientific reference was frozen, and six-hour qualification remains pending.

Native f003/f009 were downloaded and fully decoded into a separate five-time, three-hour fixture without changing the original fixture. Two retained-control pipelines completed in 269.211 and 232.659 seconds; their independent reference comparison is running. Remaining odd-three-hour inputs are being acquired for a separate native 49-time case. Input acquisition time is excluded from model timings.

## Selected-path source decisions

The qualified combined four-rank Metgrid path with zlib-ng was profiled on hpc02. The ordinary/NC+PMPI pair was 57.906/64.365 seconds: an 11.16% single-pair instrumentation difference, not an optimization result. Both products passed exact checks. A separate coarse source ledger also passed exact output checks; its ordinary/ledger walls of 62.206/58.423 seconds reflect load variation and do not demonstrate negative instrumentation overhead.

The ledger measured rank-0 backend writes at 26.756 seconds, complete gathers at 4.230, interpolation at 10.684, main field reads at 5.845, output packing at 0.261, allocation at 0.000704 and free at 0.377. Inclusive phases and rank waits are not additive. This makes allocation caching a poor next target: even eliminating allocation/free cannot save much of the stage. A narrower candidate is avoiding payload gathers during NetCDF precommit training, where the actual retained WRF backend does not read the data argument. Actual data writes and other backends retain their original path; this candidate is under caller/oracle review, not included.

A standalone input-sharing probe used the actual qualified reader objects and all 197 fields (818,133,120 logical slab bytes per rank). Explicit typed headers and contiguous slabs produced eight byte-identical complete streams across four ranks and the two read layouts. Independent/shared read-and-distribute times were 5.546/5.524 seconds; the reversed pair without correctness snapshot writes was 5.661/5.661 seconds. Sharing was effectively flat at this budget, so application integration is stopped for this round. This does not qualify malformed-input behavior or claim lower physical NFS traffic.

Geogrid's bounded integer initialization interchange passed 336 actual-source cases and improved large-array caller time about 2.04–2.14×. The approved DeepSeek implementation independently matched the locally validated transformation. Stage contribution remains unmeasured; the change is not promoted. No categorical REAL accumulation was reordered.

## Bottlenecks identified

One instrumented 16-rank Metgrid run recorded rank 0 spending 32.83 seconds in 88 float output writes and 5.84 seconds in 51 float reads, within 60.67 seconds rank runtime. Output is NetCDF4/HDF5 with shuffle and zlib level 2. This identifies output as a leading critical-path cost, but does not attribute every write second to compression.

Other ranks spent long periods in MPI sends/broadcasts while root performed serial work. These are partly waiting/imbalance costs, not evidence that IB is slow. Aggregate instruction counters include polling and cannot establish useful interpolation throughput.

Every rank independently materializes full meteorological slabs: approximately 818 MB logical payload per valid time, or roughly 13.1 GB requested by 16 ranks before extra copies. Filesystem caches can greatly reduce physical traffic. This motivated the next-round actual-reader probe above; its four-rank cached result was flat, so sharing is held.

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

### 1. Finish coverage and downstream validation

Fresh short combined integration and the four-run exact timing gate are complete. Finish the 25-time check, freeze the three-hour cadence reference and validate the combined cadence pipeline. Attempt the native 49-time case after its complete inputs and reference are available. Keep the full target grid and separate model, download, check and downstream clocks.

The two-minute forecast resource probe is complete. Subsequent steps took roughly 22–24 seconds as adaptive dt increased from 10.5 to 15.51 seconds; a six-hour forecast is deferred to keep this round WPS-first. Missing forecast inputs or resource limits remain explicit coverage gaps; successful real initialization does not replace forecast execution.

### 2. Launch three bounded source investigations in parallel

Use existing Codex component agents for ownership and review, with bounded DeepSeek implementation offloading under the standing source-payload authorization. Root owns host allocations, acceptance and integration. Isolate actual hotspots into standalone kernels/protocol harnesses before whole-stage experiments.

| Lane | First target | Smallest useful experiment | Stop condition |
|---|---|---|---|
| Metgrid, highest priority | Reusable root-owned gather/output scratch, then bounded overlap of gathering/preparation with one output owner | Actual 2-/4-rank gather oracle; measure allocation/packing/gather; prove immutable queue buffers, I/O ownership and supported thread level before asynchronous output | Negligible measured contribution, invalid interface/order/thread assumptions, or memory/copy overhead erases benefit |
| Geogrid | Interchange only the two independent integer initialization nests to make dimension 1 contiguous | Actual-source oracle for bounds/guards/untouched plane and odd/negative/empty ranges; cheap alternating timings | Flat kernel or negligible contribution; do not reorder categorical REAL accumulation |
| Ungrib | Profile complete-caller time and decoded-value/byte-weighted width/alignment eligibility | Small instrumented replay of representative complex-packing fields, then at most one eligible integer AVX2 decoder prototype | No substantial eligible time share, overread/exactness failure, or complete-caller regression |

Avoid repeating the rejected portable block or PEXT experiments unchanged. Workers use bounded contracts, denied tool access and a 240-second implementation limit; allow at most one targeted repair before root review. Current testing is authorized on hpc01–04; hpc00 remains the gateway. Root centrally schedules node cores, memory and shared-storage concurrency.

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
