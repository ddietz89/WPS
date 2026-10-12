# Separate production WPS setup

Installed package: `/pool/wrf/WPS_new`. New entry point: `/pool/wrf/runwps_setup_new.sh`.

```bash
# Run on hpc00 (automatically forwards to hpc01) or directly on hpc01.
/pool/wrf/runwps_setup_new.sh YYYYMMDDHH

# Run the unmodified GNU WPS 4.7 control on the same downloaded inputs.
/pool/wrf/runwps_setup_new.sh YYYYMMDDHH control47

# Choose a fresh root to repeat an arm; existing arm directories are refused.
/pool/wrf/runwps_setup_new.sh YYYYMMDDHH new /pool/wrf/WPS_new_runs/another-comparison
```

The script downloads the full GFS 0.25-degree products at hours 000,006,...,144 from the same public S3 bucket as `runwrf_scratch.sh`. It snapshots the existing production namelist template, GEOGRID.TBL, METGRID.TBL and Vtable, substitutes only the start/end dates, and runs Geogrid, Ungrib and Metgrid. The grid, geographic data, six-hour cadence and scientific settings are unchanged.

Downloads are checksummed once. The receipt records device/inode, size and nanosecond modification/change timestamps; setup reuses the checksum when that complete identity matches. Changed files and older receipts are fully rehashed. This avoids repeated full-input reads during normal fresh setup, without changing scientific acceptance.

Each arm has an independent namelist, input links, logs, provider receipts and output directory. Products are under `RUN_ROOT/{new,control47}/outputs`. No REAL, WRF forecast, postprocessing, existing-run cleanup, cache clearing or existing-file replacement occurs. The service mounts `/pool/wrf` read-only, with only `/pool/wrf/WPS_new_runs` writable. Consequently custom run roots must be below that writable directory. Existing production executables and configuration are hashed before/after the run.

Both arms use the same hpc01 physical cores 32–47, CPU quota 16, memory 96 GiB and swap 0. The retained unmodified WPS 4.7 control is `/pool/wrf-experiment/builds/m2/gnu/WPS`, source commit `5feccecd63384381b6942371c7a837f66e4ccb84`, compiled with GNU 13.3.0 and WRF 4.8.0 libraries. Control Geogrid uses one MPI rank, Ungrib one thread, and Metgrid 16 MPI ranks with one thread each. New Geogrid uses one rank, Ungrib four threads, and Metgrid four MPI ranks with four threads each. Both use the qualified UCX shared-memory/native InfiniBand selection (`rc,sm,self`), with no TCP MPI data transport. Unlimited IB memory lock is scoped to the service. In this full-range comparison both Metgrid runs logged the same XPMEM fallback warning; XPMEM was not active, and no XPMEM benefit is claimed. Control uses stock NetCDF and zlib; the optimized Metgrid launcher selects the qualified compression provider and zlib-ng.

The original production tree supplies existing configuration only and remains read-only. WPS 4.5 is excluded from this comparison. All selected control and optimized executables have pinned hashes. Optimized runtime dependencies are installed under WPS_new; standard host OS/IB dependencies remain required. Build-source configuration still records historical experiment locations; these are not needed for the installed optimized executable runtime. Rebuilding requires the documented source/toolchain setup.

The package includes the final promoted bitmap and vertical-mask shortcuts, narrow widths2–9 AVX2 decoder, strict scaling SIMD, Geogrid SIMD reader, threaded Ungrib/Metgrid defaults, parallel output compression, zlib-ng, root workspace/domain reuse, bulk input reader and other previously promoted changes. Wider AVX2, RH factor caching, storage bulk copy and held codec/interpolation candidates are excluded.

This comparison measures the complete optimized GNU/WPS 4.7 configuration against unmodified GNU/WPS 4.7, including the intended library and thread-count changes. The optimized run completed before the newly requested control; one run of each is indicative and has cache/order/shared-load variation. Stage, kernel and pipeline percentages are not added together. Download and exact scientific comparisons are outside stage clocks. The unchanged original scientific comparers check all 51 products, including metadata and NetCDF filter/chunk layout. Differences are retained as failures; tolerances are not relaxed.

Installation manifest: `/pool/wrf/WPS_new/INSTALLATION.json`. Benchmark inputs/results: `/pool/wrf/WPS_new_runs/2026101206`. Rollback is simply to continue using `/pool/wrf/WPS` and the unchanged `/pool/wrf/runwrf_scratch.sh`; no existing path was redirected.
