# Isolated WRF I/O identity write

This optional dependency patch avoids allocation and transpose for committed contiguous real32 fields whose exact uppercase XY/XYZ memory, patch and domain bounds agree. Positive extents and the complete default-integer index range are checked with widened arithmetic. Other layouts/types/conversions retain the original path. Metadata, compression filters, MPI/NetCDF ownership and the caller-owned buffer lifetime remain unchanged.

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
