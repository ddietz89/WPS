# Default Metgrid row interpolation threads

The default accelerated build parallelizes independent output rows for initialized latitude/longitude source grids, mass/U/V/corner staggering, and the non-average-gcell interpolation path. `MASKED_BOTH` and other projections retain serial execution. Arithmetic and the order of operations within each point are unchanged. Each row owns separate field values and bitmap words.

The qualified configuration is GNU 13.3 on Linux, four MPI ranks and four OpenMP threads per rank, with the separately installed parallel-compression NetCDF owner and compatible zlib-ng. MPI startup requests and requires `MPI_THREAD_FUNNELED`. Domain selection, MPI, NetCDF and HDF5 stay on the initial thread, outside the OpenMP regions. The provider rejects worker entry before making any MPI call.

## Build and launch

Use a separate configured checkout:

```sh
WPS_BUILD_JOBS=8 tools/build-metgrid-precompressed /absolute/owner-install

# Load the qualified MPI/IB environment first.
WPS_PRECOMPRESSED_NETCDF_PREFIX=/absolute/owner-install \
WPS_ZLIB_NG_LIBRARY=/absolute/zlib-ng/lib/libz.so \
  tools/metgrid -- mpirun -np 4 --rankfile rankfile \
  --bind-to core ./metgrid.exe
```

Allocate sixteen physical cores. Each rankfile entry must bind its rank to four distinct physical cores; on the qualified hosts, an example is `rank 0=hpc02 slot=0:32-35`, followed by ranges 36–39, 40–43 and 44–47 for the other ranks. Keep UCX shared memory/IB transport. Compression uses the existing MPI ranks and adds no compression thread team.

`tools/metgrid` defaults to four interpolation threads per rank for the accelerated build, overriding an inherited MPI-environment `OMP_NUM_THREADS=1`. Set `WPS_METGRID_OPENMP_THREADS=1` to run it with one interpolation thread. Core placement, close binding and disabled dynamic team sizing are the defaults. `tools/metgrid-openmp` remains a compatibility alias that explicitly enables this mode.

The build adds `-fopenmp -frecursive` and rebuilds the complete concurrently reachable module closure when changing modes. Provider-enabled MPI/NetCDF builds select it automatically. Set `WPS_METGRID_OPENMP=0` when building to restore original flags and `MPI_Init`; set it when launching to preserve the caller's OpenMP environment. An unset build option now enables threading when the provider is selected. Geogrid's shared source retains its original preprocessed implementation. Production `/pool/wrf/WPS` is a read-only reference.

The launcher reads the build's `.metgrid_backend_mode` stamp to select its owner prefix and thread mode. For a separately selected executable, explicitly supply its matching `WPS_PRECOMPRESSED_NETCDF_PREFIX` and `WPS_METGRID_OPENMP` settings. To restore the ordinary executable, rebuild with `WPS_METGRID_PRECOMPRESSED=0 WPS_METGRID_OPENMP=0` and launch from a clean original library environment. Neither runtime switch removes code from an already accelerated executable.

## Evidence

Independent actual-module tests compared complete field bitpatterns and both validity/new-point bitmap arrays across six cases and 1/2/4 threads, including missing-value search, masks and serial fallback. SINGLE/FUNNELED main-thread lifecycle tests passed at 1/2/4 MPI ranks; worker-entry rejection and error cleanup passed.

Three alternating stage pairs compared against the previously promoted parallel-compression build using the same sixteen-core allocation, four ranks, owner library and zlib-ng:

| Pair | Control seconds | Threaded seconds | Wall reduction |
|---|---:|---:|---:|
| 0 | 59.615 | 50.709 | 14.94% |
| 1 | 53.782 | 47.358 | 11.94% |
| 2 | 50.826 | 47.931 | 5.70% |

All six complete outputs were byte-identical to the independently accepted science/layout reference, with matching provider WORK/DONE proof and observed rank/thread placement. Median wall reduction was 11.94%; median aggregate CPU increased 22.01%. The control can use fewer active threads within the same allocation. Shared load and cache variation remain possible; these results cover the retained 1500×1140 GFS fixture and are not a general speedup guarantee.

The packaged executable and all eight concurrently reachable objects matched the tested candidate bit for bit. Mode switching restored both executables bit for bit. A separate time06 packaged launcher run passed complete byte identity and exact science/layout/provider checks, verifying the inherited one-thread setting was overridden to four. These checks are recorded with the experiment receipts under `/pool/wrf-experiment/results/interpolation-round/`. Larger-window results in the consolidated report apply to their earlier qualified builds; this option has its own bounded acceptance.
