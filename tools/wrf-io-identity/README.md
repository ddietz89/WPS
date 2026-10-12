# Isolated WRF I/O identity paths

This optional dependency patch avoids allocation and transpose for committed contiguous real32 fields whose exact uppercase XY/XYZ memory, patch and domain bounds agree. Positive extents and the complete default-integer index range are checked with widened arithmetic. Other layouts/types/conversions retain the original path. Metadata, compression filters, MPI/NetCDF ownership and the caller-owned buffer lifetime remain unchanged.

For reads, a guarded INTEGER-word bulk copy replaces the generic transpose only after successful FieldIO, for complete uppercase XY/XYZ REAL32 layouts. The original temporary allocation, read and error path remain, preserving the destination on failed reads. Unsupported orders/types/partial or negative layouts and size-risk cases retain the original transpose. No new buffer, thread or numerical flag is introduced.

The supported source is the pinned WRF4.8 GNU13.3.0 build identified by the builder. This package builds only one object in a NEW isolated WRF dependency capsule, copies the archive and replaces only wrf_io.o. Original source, libraries, module files and /pool/wrf/WPS remain unchanged. Other archive member bytes are checked before and after. It requires Python3, GNU13.3.0, cpp, m4, patch, ar and ranlib. The existing NetCDFFortran prefix must match the configured WRF ABI.

Run with absolute paths:

```sh
python3 build_wrf_io.py --wrf-root /absolute/retained/WRF \
  --output /absolute/new/WRF-identity \
  --patch /absolute/identity.patch \
  --fc /absolute/gnu13.3/gfortran \
  --netcdf-fortran-prefix /absolute/retained/netcdf-fortran
```

Select `WRF_DIR = /absolute/new/WRF-identity` in a separately configured WPS checkout and force Metgrid to relink. The original WPS objects and all other WRF objects remain unchanged. Continue using the qualified job-local provider-NetCDF/zlib-ng launcher. No global environment or system libraries change. Rollback selects the original WRF_DIR and forces another relink; no dependency file must be overwritten.

A successful build.json identifies source/patch/compiler/commands/object/archive and rechecks original inputs. Failed builds leave a fresh output directory for inspection without a successful manifest; choose a new output path after correcting the failure. The builder never removes an existing directory. A changed object/archive digest requires actual artifact qualification before use; diagnostic __FILE__ strings can differ across preprocess paths. Only the pinned source/compiler/flags are supported by the included evidence.

Qualification includes the original actual-Transpose word oracle,14-file original/candidate WRFIO exact/layout checks with fallback/error/source-preservation coverage, representative TT/LANDMASK actual writes, and two reversed whole-Metgrid stage pairs with exact canonical products and complete provider/thread/runtime proof. Whole-stage wall reductions were17.35% and8.23%, with CPU reductions12.01% and8.03%. These measured values include normal load variation. Production deployment remains separate from working-version promotion.

Read qualification passed4,000 original-Transpose layouts/1,917,393 arbitrary words,14 actual file layouts/two dates and status/error/source-preservation checks, including a partial temporary mutation followed by a read error while the destination remained unchanged. Five allocation-inclusive HGT_M/LANDUSEF read-caller pairs improved23.52–25.49%, median23.89%. Two reversed archive-only Metgrid stage pairs measured42.243→40.434 and41.017→39.415seconds:4.281%/3.905% lower wall with2.272%/1.580% lessCPU; all four complete outputs and provider/runtime/resource proofs passed. This is a bounded stage result, not a universal or cumulative percentage. The packaged object and archive exactly match the tested artifacts.

The full read guard was authored locally after two unusable DeepSeek implementations; the returned integer-copy helper was retained and independently qualified. Rollback of reads alone selects the retained write-identity capsule archive `a6b81a8b9e7b76aed5f50f71f009010d7efbe11a7f88e3cffde74e03592e9fa3` and relinks Metgrid, or restores the earlier write-only patch/builder. The combined read/write archive is `772be4d46c2ce9d29b56a1848e6754cb8100f059fe88c8feb206472a57b0d1ea`. With the separately promoted rank-scratch source, the composed working Metgrid executable is `ccfd9f896faafba6cda35a98d628f5923756ce91aa17a02cd3444c9da9cfb576`. GNU/pinned dependency scope applies; AOCC/Intel remain eligible but unqualified for these new paths, and earlier25/49-time or forecast results retain their original build scope.
