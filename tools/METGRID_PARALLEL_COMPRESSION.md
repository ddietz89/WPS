# Default parallel Metgrid compression

This fork can use four existing MPI ranks to compress independent output chunks while rank 0 remains the only NetCDF/HDF5 file owner. It preserves the original WPS gather, WRF transpose, float values, metadata, chunk layout, shuffle and DEFLATE level 2. MPI/NetCDF builds select the qualified owner by default; `WPS_METGRID_PRECOMPRESSED=0` restores the ordinary build. The fast path is limited to 2 or 4 MPI ranks; other rank counts and unsupported writes use synchronous output.

## Build and run

Use a separate configured WPS checkout and a separate patched NetCDF installation. The owner build and provider contract are documented in `netcdf-precompressed/README.md`. Supported deployment qualification is Linux, GNU 13.3, NetCDF-C 4.9.2, retained NetCDF-Fortran, serial HDF5 1.14.3 and the compatible zlib-ng 2.3.3 library. AOCC/Intel qualification of earlier WPS improvements does not qualify this new backend.

```sh
WPS_BUILD_JOBS=8 tools/build-metgrid-precompressed /absolute/owner-install

# Source the site's qualified MPI/IB environment before launching.
WPS_PRECOMPRESSED_NETCDF_PREFIX=/absolute/owner-install \
WPS_ZLIB_NG_LIBRARY=/absolute/zlib-ng/lib/libz.so \
tools/metgrid -- mpirun -np 4 --rankfile rankfile --bind-to core ./metgrid.exe
```

The default also enables row interpolation threads: allocate sixteen physical cores per four-rank process group, binding four physical cores to each rank. For compression-only execution, build and launch with `WPS_METGRID_OPENMP=0` and allocate four physical cores per group. On the qualified hosts, use the retained UCX/IB environment rather than selecting TCP transports. Same-node payloads use shared memory; this feature has no measured inter-node IB speedup claim. Multiple independent valid-time groups can occupy disjoint core sets with job-local memory limits. Do not nest extra compression OpenMP teams.

The build helper validates the provider symbols, uses a private configuration copy to stop on compiler failures, creates the expected executable links, and leaves `configure.wps` unchanged. A prefix/mode stamp rebuilds the affected objects when switching between accelerated and ordinary builds. `METGRID_PRECOMPRESSED_NETCDF_PREFIX` enables direct use through ordinary WPS recursive make; the helper provides stricter error handling. Prefixes must be absolute. The default owner prefix is `/pool/wrf-experiment/builds/backend-integration/netcdf-owner-package-r1/install`; the helper accepts a different prefix, and the launcher reads the recorded build prefix. `WPS_PRECOMPRESSED_NETCDF_PREFIX` overrides runtime selection; no system loader files change. Missing dependencies fail clearly rather than silently disabling acceleration.

## Ownership and failures

Every rank enters a matched per-field session. Rank 0 registers the provider only around one original WRF external write; other ranks receive WORK/DONE commands. Provider raw data is borrowed and immutable. Workers perform no NetCDF/HDF5 calls. The ordinary backend uses `MPI_THREAD_SINGLE` and the process main thread. The default [row interpolation path](METGRID_OPENMP.md) uses `MPI_THREAD_FUNNELED`, with MPI and file ownership still confined to the initial thread.

Only full eligible little-endian float32 records, supported fill and exactly shuffle/DEFLATE2 qualify. Chunk size is bounded at 64 MiB. An initial decline can use the ordinary write. Once any direct write is attempted, an error is terminal for that product: propagate it, end the scope, close for cleanup and discard the incomplete output. Never retry or accept a partial file. Root unregisters and sends DONE before the original fatal reporting. MPI transport failure aborts the job.

For deployment verification, set `WPS_CHUNK_PROOF_DIR` to a new writable directory and `WPS_CHUNK_PROOF_RUN_ID` to a unique identifier; forward both with `mpirun -x`. Each close writes a unique per-rank/PID JSON ledger. Counts are cumulative across files. Confirm positive expected provider work plus successful model completion and exact outputs. A synchronous fallback must not be counted as accelerated execution. Leave these variables unset for normal use.

## Qualification and rollback

The prior matched short-stage pairs reduced wall time by 18.98–29.74%, median 29.70%, on the same four physical cores and zlib-ng. Median aggregate CPU fell 26.38%. All eight outputs were byte-identical to independently accepted scientific/layout references. The portable rebuild and actual production launcher also passed all25six-hour times and repeated five-time three-hour execution with complete byte identity. Fresh WRF4.8 initialization outputs passed the original reference checks. Mode switching and deliberate compiler/HDF5 failures passed. Exact build identities and scope are recorded in `OPTIMIZATION_FINDINGS.md` and `netcdf-precompressed/qualification.json`.

Keep the ordinary executable and original environment available. Rollback selects that executable with `WPS_METGRID_PRECOMPRESSED=0 WPS_METGRID_OPENMP=0` and uses the original library environment; do not reuse a shell that already exports the provider's library directory. `WPS_METGRID_EXECUTABLE` selects the executable when invoking `tools/metgrid` without command arguments. `WPS_USE_STOCK_ZLIB=1` skips adding zlib-ng while preserving caller environment settings. Rebuild an ordinary checkout with `WPS_METGRID_PRECOMPRESSED=0 WPS_METGRID_OPENMP=0` if necessary. Runtime opt-outs do not remove provider code from an already accelerated executable. The protected production reference `/pool/wrf/WPS` is never rebuilt or edited by this workflow.
