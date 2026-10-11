/* Experimental owner extension ABI v1. Root/main thread only; not promoted. */
#ifndef NETCDF_PRECOMPRESSED_H
#define NETCDF_PRECOMPRESSED_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NC_PRECOMPRESSED_ABI 1
#define NC_PRECOMPRESSED_MAX_DIMS 4
#define NC_PRECOMPRESSED_BATCH_MAX 4
typedef struct {
 unsigned abi_version;
 int ncid, varid, ndims;
 size_t request_start[4], request_count[4], field_dims[4], chunk_dims[4];
 unsigned char fill_le[4];
 size_t total_chunks, batch_first, batch_count, offsets[4][4];
} nc_precompressed_float_layout;
typedef struct {
 size_t count;
 const void *data[4];
 size_t nbytes[4];
 void (*release)(void *release_user);
 void *release_user;
} nc_precompressed_float_batch;
/* Return1 supplied,0 declined,negative NC error. Do not call NC/HDF inside.
 * raw_data is actual C-order NC_FLOAT buffer; raw_nbytes includes only valid
 * requested cells. Pack LEfloat32 fullchunks with fill_le at tails, then
 * shuffle(elementsize4) and zlib-format DEFLATE2. Result streams ordered by
 * layout.offsets. Root owner releases each result once, including errors.
 * release is pure memory cleanup: no MPI/NC/HDF calls. */
typedef int (*nc_precompressed_float_provider)(
 const nc_precompressed_float_layout *layout, const void *raw_data,
 size_t raw_nbytes, nc_precompressed_float_batch *result, void *user);
/* Single-active process scope around one root ext_ncd_write_field call.
 * begin rejects NULL/nesting; end clears on every outer caller exit path.
 * Root/main thread only, no ownership transfer of user; no per-NCID registry.
 * Original numerical nc_put_vara_float API remains caller entry point. */
int nc_precompressed_provider_begin(nc_precompressed_float_provider provider, void *user);
int nc_precompressed_provider_end(void);
#ifdef __cplusplus
}
#endif
#endif
