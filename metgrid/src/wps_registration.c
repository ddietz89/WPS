/* Root-only per-external-call provider scope. Not linked until owner API review. */
#include <netcdf.h>
#include "netcdf_precompressed.h"
typedef struct wps_chunk_session wps_chunk_session;
int wps_chunk_provider(const nc_precompressed_float_layout*,const void*,size_t,nc_precompressed_float_batch*,void*);
int wps_chunk_register(void *session){return nc_precompressed_provider_begin(wps_chunk_provider,session);}
int wps_chunk_unregister(void){return nc_precompressed_provider_end();}
