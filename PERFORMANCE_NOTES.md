# WPS preprocessing optimizations

This branch adds six source changes to the WPS 4.7.0 baseline and a job-local metgrid dependency selection:

- Reuse packed bytes and unused bits in a per-call GBYTES reservoir for zero-skip, 1–31-bit extraction. Keep the original byte conversion and generic path for unsupported cases, with overflow guards.
- Write contiguous ungrib slabs as whole array sections, avoiding scalar runtime transfers while preserving unformatted records. Keep the original path for strided slabs. This uses Fortran 2008 CONTIGUOUS and IS_CONTIGUOUS, checked with the configured GNU, AOCC and Intel compilers.
- During metgrid mask discovery, read field headers but materialize only mask fields and their conservative table aliases. Audit file record envelopes before selection; uncertain or malformed files retain full reads. Normal processing reads are unchanged.
- Decode eligible independent GRIB2 complex-packing groups with OpenMP, preserving the retained bit reservoir, original arithmetic and serial fallback for missing values or unsupported geometry. Ungrib defaults to four threads when OMP_NUM_THREADS is absent or empty; an explicit setting overrides it.

- Decode eligible geogrid 1-/2-byte tiles with a separate SIMD conversion/scaling helper; retain original rounding, signed thresholds, alias guards and fallback.

- Use the original sixteen-point interpolation arithmetic directly for eligible unmasked interior stencils, avoiding per-element clipping and optional-mask dispatch. Retain the original path for edges, active masks, missing stencils and near-integer points.

## Promoted metgrid interpolation fast path

Metgrid uses a symlink to geogrid/src/interp_module.F; the fast path is included in this shared module and applies to both stages when rebuilt. Prior speedup evidence covers Metgrid. Fresh next-round exact acceptance now covers both rebuilt stages in the combined configuration.

The promoted interp_module.F is byte-identical to the prior tested candidate (SHA256 97d7e2edcdaabc64cad379173591fe9eac9e7c5bc1a343e8913db602bd1dd765). Its actual-source oracle passed 900,000 comparisons in each of GNU, AOCC and Intel. The cases cover signed coordinates, threshold and boundary cases, missing/zero/negative-zero/tiny/NaN values, optional masks and fallback sequences. Prior three-time stage products passed exact checks against their control, which also passed against the scientific reference.

Four representative kernel configurations improved 1.17–1.25x. One short stage pair measured 193.61 to 190.27 seconds (1.73% less wall); that single pair does not establish a repeatable stage percentage. The original oned calls and their expression order are retained, with no fast-math, contraction or threading changes. Promotion originally reused the tested source without new runs. The subsequent isolated build at `9e0ae2a` passes seven-product short qualification and 28 products from four matched timing pipelines; see OPTIMIZATION_FINDINGS.md for scope and longer-window status.

The consolidated findings and proposed next steps are in OPTIMIZATION_FINDINGS.md.

## Promoted geogrid SIMD reader

The reader retains strict O2 compilation; only the separate helper uses strict O3, Zen2 targeting and SIMD-only directives. It creates no OpenMP teams or runtime dependency. GNU, clang/AOCC and Intel C flags follow the previous experiment. Set GEOGRID_SIMD_ARCH_FLAGS= for a generic CPU target, or supply another target; other compiler families can provide their own optimization flags.

The prior source passed15,264 exact kernel/reader cases across GNU/AOCC/Intel and all12 stage outputs. Kernel gains were2.11–6.62x, but five stage pairs were mixed (three slower, two faster; median paired change+0.09%). The user directed promotion; an established whole-stage speedup is not claimed.

## Promoted metgrid zlib-ng selection

Use tools/metgrid to run the compiled metgrid executable with zlib-ng by default, or tools/metgrid -- mpirun <MPI options> ./metgrid.exe for MPI. The launcher preloads the library into that command and its children. It leaves other WPS invocations alone. The default shared-project path selects the existing tested compatibility library; WPS_ZLIB_NG_LIBRARY overrides its location. WPS_USE_STOCK_ZLIB=1 skips adding this preload and preserves caller-provided environment settings. Direct metgrid.exe invocation retains its normal linked-library selection.

The dependency is official zlib-ng2.3.3, commit12731092979c6d07f42da27da673a9f6c7b13586, built with --zlib-compat. Existing library SHA256:5d69c44314f30e4a6f6c76ee26a454538f7e1b019afe5782e67a6b94ba794b64. No library binary is vendored or system zlib replaced. Build a separate compatibility prefix with the official configure/make flow when needed. HDF5 level2, shuffle and chunk shapes stay unchanged.

Five prior16-rank stage pairs reduced wall14.7–22.1% (median17.7%), median CPU19.8%, with0.23% larger files; all12 outputs were exact. Two fixed16-core pipeline pairs showed2.6–11.1% additional wall reduction in the concurrent layout; all42 products passed. Those pipelines excluded the newly promoted geogrid reader and OpenMP default.

These additions were initially promoted without new runs at the user's request. The fresh next-round build exposed recursive-make flag overrides; `9e0ae2a` uses override appends to retain OpenMP flags in the main object and link. Runtime four-thread placement and complete combined short products now pass. Longer-window and downstream statuses are recorded in OPTIMIZATION_FINDINGS.md.

## Ungrib OpenMP build and runtime

GNU and AOCC/flang builds select `-fopenmp`; Intel ifort/ifx builds select `-qopenmp`. The flags apply to ungrib's GRIB2 library, main program, RRPR RH adjustment and executable links. Other WPS stages keep their existing build settings. Set `UNGRIB_OPENMP_FLAGS=` when invoking make for a serial build; other compilers can supply their own OpenMP flag through that variable.

Use `OMP_NUM_THREADS=1` for serial decoding or another explicit value to override the four-thread default. Launchers that already export OMP_NUM_THREADS retain that choice. Allocate four physical cores per default ungrib process: three valid-time workers plus four-rank geogrid fit within sixteen cores using disjoint core sets.

The independent-group implementation previously passed exact caller checks across GNU, AOCC and Intel and showed approximately1.9x kernel speedup at four threads. Its earlier warm ungrib stage pair improved wall time3.58% for36.4% moreCPU. The current four-thread default and build integration were included at the user's request without a new build or test run; the earlier integrated results below cover the original three retained changes, not this addition.

## Validation

The original three retained changes match the tested integrated GNU build. Standalone actual-source correctness checks covered GNU 13.3, AOCC 5.2 and Intel 2025.3. These compare each compiler with its original implementation; they do not establish cross-compiler equality.

The integrated GNU three-time, 1500 x 1140 Lambert case passed all seven exact scientific product comparisons against a fresh control; the control also passed against the established reference (14 comparisons). A subsequent concurrent pipeline using these same sources passed all seven product comparisons against that reference. Checks include field values and scientific metadata; numerical tolerances were not relaxed.

One paired sequential short run measured 277.23 seconds before and 224.20 seconds after these changes (19.1% less wall time), with 16 MPI ranks and serial ungrib. Cache and shared-load variation limit this estimate. Individual kernel timings are not additive stage-speedup claims.

A separate cluster-specific orchestration experiment completed in 89.75 seconds using up to 48 model cores. That workflow is not part of this source patch and does not establish equal-core speedup.

The previously qualified combined build passed full 25-time WPS acceptance and short WRF initialization. Forecast scientific qualification remains outstanding; the new backend has separate qualification recorded in OPTIMIZATION_FINDINGS.md. These are experimental improvements; the production tree and original scientific reference were not modified.

## Default parallel compression backend

The default MPI compression backend and its reproducible NetCDF owner extension are described in [tools/METGRID_PARALLEL_COMPRESSION.md](tools/METGRID_PARALLEL_COMPRESSION.md). Explicit ordinary builds retain synchronous output. The backend uses 2 or 4 MPI ranks; other rank counts fall back. Matched four-core short-stage measurements reduced wall time 18.98–29.74%, median29.70%, with exact outputs. Deployment scope, multi-time/downstream acceptance, library identities and rollback are recorded in OPTIMIZATION_FINDINGS.md.

## Default Metgrid row interpolation threads

The [row interpolation option](tools/METGRID_OPENMP.md) uses four threads per MPI rank while keeping MPI and file I/O on the main thread. Three alternating pairs against the promoted compression backend reduced stage wall time 5.70–14.94%, median 11.94%, with all six outputs exact. Median CPU increased 22.01%. Both arms received the same sixteen-core allocation; the original used fewer active interpolation threads. Provider-enabled builds and the matching launcher now select this path by default; explicit opt-outs restore the ordinary build. The separately investigated four-lane stencil SIMD helper remains unpromoted because its final warm stage pair showed no gain.

Metgrid MPI/NetCDF builds now default to the qualified parallel-compression owner and row interpolation threads. `tools/build-metgrid-precompressed` needs no prefix argument on the qualified cluster, and `tools/metgrid` reads the selected build prefix/mode and defaults to four threads per rank. Allocate sixteen physical cores for four MPI ranks. Use `WPS_METGRID_PRECOMPRESSED=0 WPS_METGRID_OPENMP=0` for an ordinary rollback build and launcher environment, or `WPS_METGRID_OPENMP_THREADS=1` for one interpolation thread. See `tools/METGRID_OPENMP.md` for binding and qualification scope.

## Ungrib RRPR bulk reads

RRPR now reads its freshly allocated contiguous two-dimensional pointer through a full array section, avoiding per-element Fortran runtime transfers. Record framing, endian conversion and storage ownership remain unchanged. Two reversed GNU three-time stage pairs reduced wall by 23.97–40.42%, median32.19%, with all12 intermediate products byte-exact; median CPU fell26.97%. Actual-source bit/ownership kernels pass in GNU/AOCC/Intel stacks, and original malformed-record behavior is preserved. See OPTIMIZATION_FINDINGS.md for scope/evidence. Rollback selects the retained prior executable or restores the previous RRPR source and rebuilds.

## Ungrib RH row threads

RH adjustment partitions independent rows using the existing Ungrib OpenMP runtime and four-thread default. Scalar formulas, threshold branches and serial storage ownership remain unchanged. Two reversed GNU three-time stage pairs improved wall5.79% median for0.86% moreCPU; all12 intermediate files were byte-exact. The actual-source storage oracle passed25,065,936 checks at1/2/4threads. Scope is GNU scientific values under configured nontrapping fixed rounding, not master-thread IEEE status/trap ordering; AOCC/Intel qualification for this new threaded path is pending. These gains are separate from prior RRPR and pipeline percentages. Use OMP_NUM_THREADS=1 for one-thread execution, or restore the previous RRPR source/build rule and rebuild; the retained bulk-read executable is a qualified rollback. See OPTIMIZATION_FINDINGS.md for hashes and evidence.

## Qualified WRF I/O identity dependency

[tools/wrf-io-identity/README.md](tools/wrf-io-identity/README.md) describes the isolated WRF4.8/GNU13.3 build helper and reviewed patch. Complete eligible real32 XY/XYZ writes borrow the original contiguous input instead of allocating/transposing a copy; all other layouts/types/conversions retain the original path. Two reversed actual Metgrid pairs reduced wall17.35%/8.23% and CPU12.01%/8.03%, all outputs exact. A separate packaged artifact also passed exact stage/runtime/provider checks. Select the new capsule via WRF_DIR in a separate checkout and relink; rollback selects the original WRF_DIR and relinks. Original archives/source/module files remain unchanged. This is qualified working-version integration, separate from production and prior pipeline timing claims.

## Ungrib independent scaling threads

The no-missing GRIB2 scaling loop uses existing OpenMP flags for at least65,536 points. Each element retains the original scalar expression and disjoint output; small/constant/missing and spatial reconstruction paths stay unchanged. Exact loop checks passed833,534,550 values across1/2/4threads and81 complete caller cases. Five caller pairs improved12.66% median; two reversed GNU three-time stages improved16.67% median wall/7.13% CPU, all12 products exact. Shared-cache/load variation limits the stage percentage; gains are not added to other kernel/stage/pipeline results. Qualification is GNU fixed-rounding nontrapping values, with AOCC/Intel experimental checks pending. OMP_NUM_THREADS=1 selects one-thread work; restore the previous comunpack.f and rebuild G2/Ungrib, or retain qualified RH-only456d67 as rollback.

## Metgrid bounded packing scratch

Qualified compression builds retain one root packed-input scratch buffer across field writes, at most256MiB, and explicitly free it at output close. Original padding/copy code and MPI protocol remain unchanged; returned compressed results retain independent ownership. Allocation growth/error/lease/cleanup and actual MPI protocol tests pass. Original-packing replay improved70.49% median; two reversed stage pairs improved15.89%/3.51%, with the warmer1.44-second saving supporting narrow promotion. All four files remain canonical-byte exact. Extra root residency is bounded; strict flags, layout and numerical behavior are unchanged. Rollback uses prior identity-write Metgrid78d0fd or restores previous bridge/output-module source and rebuilds. The shared Geogrid source addition is guarded out for Geogrid. See OPTIMIZATION_FINDINGS.md for qualified GNU scope and hashes.
