# NetCDF precompressed owner extension

This package provides the reviewed owner extension for **NetCDF-C 4.9.2**. It preserves `libnetcdf.so.19` and the ordinary numerical APIs. It is qualified for opt-in production use with the documented four-rank GNU/Linux WPS stack; deployment requires explicit dependency selection. See qualification.json and ../METGRID_PARALLEL_COMPRESSION.md for the completed integrated gates and rollback. It does not change a production WPS tree or any system library.

Supported qualification scope: Linux process main thread, little-endian IEEE float32, serial HDF5 1.14.3, standard shuffle(element size4) followed by zlib-format DEFLATE level2. The existing NetCDF/HDF5 libraries are not thread-safe. One process-local provider scope surrounds one root WRF field-write call. NetCDF remains the only live file owner; providers may use MPI and private compression state, and may not call NetCDF or HDF5. End the scope on every outer return, including errors. Nested scopes and handle-based NetCDF callback/release reentry are rejected; stateless/global NetCDF functions and direct HDF5 calls are forbidden by contract rather than comprehensively intercepted.

## Build

Use the qualified original NetCDF CMake cache to retain compiler flags and features. Source, dependencies, work and install paths must be disjoint. Both destination directories must be new. The script verifies exact unmodified source files, applies the patch with zero fuzz, copies the ABI header, builds/installs into the explicit prefix, verifies SONAME/provider symbols, and writes a build receipt. It does not download dependencies or change retained files.

```sh
python3 tools/netcdf-precompressed/build.py \
  --source /path/to/unmodified/netcdf-c-4.9.2 \
  --reference-cache /path/to/qualified-original-build/CMakeCache.txt \
  --dependency-prefix /path/to/qualified-serial-hdf5-and-zlib \
  --build-root /path/to/new-owner-work \
  --prefix /path/to/new-owner-install \
  --cmake /path/to/cmake --jobs 8
```

The optional `--compiler` selects an explicit C compiler; omit it to retain the reference compiler. Such a compiler change requires renewed deployment qualification. Use job-local `LD_LIBRARY_PATH` to select the new NetCDF library ahead of the retained dependencies; do not edit system loader configuration. Select the separately qualified zlib-ng library for both timing arms when testing MPI compression. Existing NetCDF-Fortran/WRF objects use the unchanged NetCDF SONAME; verify actual loaded paths and hashes rather than assuming search order.

## Provider contract

`netcdf_precompressed.h` is ABI1. The raw buffer and layout are **borrowed, immutable** and valid only during the callback. The provider must never free, modify, retain or alias ownership of this buffer; returned streams and release state own only separately allocated memory. Callback return1 supplies exactly the requested ordered batch (at most4chunks),0 declines, and a negative value returns a NetCDF error. Owner release is invoked exactly once for every callback that supplies a release function, including decline and error. Release is pure memory cleanup, with no MPI/NetCDF/HDF5 calls.

The callback receives actual C-order data after WRF conversion/transposition and NetCDF's normal preflight. Only complete single-Time records with full fixed spatial extents qualify; odd edge chunks are padded with the supplied external little-endian fill bytes. Streams must contain shuffle then zlib-format DEFLATE2 data; HDF5 filter mask0 means both filters were applied. Chunk bytes and compressed stream length are bounded below4GiB. This is a trusted-provider API: it cannot prove arbitrary supplied compressed bytes are correct without decoding them. Provider qualification must dual-decompress and compare complete padded chunk bytes.

Ineligible geometry, type, filter, fill or backend retains the original synchronous numerical write. The first provider decline also falls back, after release. Once any direct chunk write is attempted, failures are **terminal for that output product**: the caller must propagate the error, end the provider scope, close the handle for resource cleanup, and discard the incomplete file. Closing may succeed; that is cleanup, not acceptance of a failed write. There is no transaction or rollback guarantee, consistent with ordinary NetCDF/HDF5 write errors. Do not retry ordinary writes over a partially committed product or hide failures. A job must verify expected provider work/bytes and successful output checks to prevent silent fallback from being mistaken for acceleration.

## Oracle and evidence

`oracle.c` generates synthetic 3D/4D fields, custom/default fill, odd tails and Time gaps. It checks append/rewrite, read/dirty raw caches, subsequent partial writes, reopen, absent/declined providers, unsupported fallbacks, scope/reentry/release and early/late errors. It contains no scientific fixtures or private source. Compile it against the original NetCDF/dependency headers and this package. For example, after setting `PACKAGE`, `CONTROL`, `OWNER` and `ZNG` to their explicit package/original-prefix/new-prefix/zlib-ng-library paths:

```sh
cc -O2 -fno-fast-math -ffp-contract=off -I"$CONTROL/include" -I"$PACKAGE" \
  "$PACKAGE/oracle.c" -L"$CONTROL/lib" -lnetcdf -lz -ldl -pthread -o oracle
mkdir new-control-output new-candidate-output
LD_LIBRARY_PATH="$CONTROL/lib" LD_PRELOAD="$ZNG" \
  ./oracle 0 "$PWD/new-control-output" "$CONTROL/lib/libz.so"
LD_LIBRARY_PATH="$OWNER/lib:$CONTROL/lib" LD_PRELOAD="$ZNG" \
  ./oracle 1 "$PWD/new-candidate-output" "$CONTROL/lib/libz.so"
```

Both output directories must be new, and use the same dependency/compression selections in both arms. The program arguments are:

```sh
./oracle 0 /path/to/new-control-output /path/to/stock/libz.so
./oracle 1 /path/to/new-candidate-output /path/to/stock/libz.so
```

Preload the qualified zlib-ng in both arms. Verify logged NetCDF/zlib paths and compare successful files against the original checker. Four candidate-only `failed-mode*.nc` files are intentionally failed outputs and must not enter a success inventory. Fourteen finite successful file pairs passed the unchanged scientific checker and exact filter/chunk/endian checks. The extra deliberate NaN file passed complete valid-cell bit equality; the original scientific checker rejects NaN/Inf by policy, so it is a separate bit-pattern test rather than a relaxed acceptance rule.

The qualification patch and header identities are pinned by `build.py`; the installed qualification library SHA256 was `465302d5d5cf806a8719aa1c11b5b87f9ca57bac72e19178763a8e73cb24eb67`. Rebuilding may change bytes due to paths/toolchain, so compare source/flags/features and run the appropriate deployment gates. Detailed source provenance and review disposition are in `review.json`. The upstream NetCDF source and license remain required inputs; this directory contains a patch, not a redistributed dependency tree.

`tests/fail_chunk.c` and `tests/failure_oracle.c` provide the focused backend-failure test. The interposer forwards a zero data size to the actual HDF5 direct write (rather than fabricating a NetCDF return code), at `NC_TEST_FAIL_DIRECT_AFTER=0` or `1`. It confirms a nonempty HDF5 error stack. The test must preload that interposer only in its dedicated test process; never select it for WPS. Run the failure oracle with a fresh output directory and stock-zlib path arguments. Both cases passed terminal error propagation, matching release counts, close/discard cleanup and a new successful scoped write.

The portable script itself was exercised into a fresh prefix with unchanged source/flags/features. Its new library (`8adc33059a98399cd8adea41de071598aaa2b09ff53eb3ae860cad0c6696b538`) passed the same14 finite metadata/layout/value pairs, separate NaN bit proof and first/second real HDF5 failure tests. No source algorithm changed between the original isolated build and the packaged rebuild. Canonical receipt identities are recorded in `review.json`.
