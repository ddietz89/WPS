#define _GNU_SOURCE
#include <hdf5.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
static unsigned calls;
herr_t H5Dwrite_chunk(hid_t d, hid_t x, uint32_t mask, const hsize_t *off, size_t n, const void *data){
 herr_t (*real)(hid_t,hid_t,uint32_t,const hsize_t*,size_t,const void*)=dlsym(RTLD_NEXT,"H5Dwrite_chunk");
 const char *value=getenv("NC_TEST_FAIL_DIRECT_AFTER");
 unsigned after=value?(unsigned)strtoul(value,NULL,10):0;
 if(calls++==after){fprintf(stderr,"INJECT actualHDFerror directcall%u\n",calls);herr_t result=real(d,x,mask,off,0,data);fprintf(stderr,"HDF_ERROR_COUNT %ld\n",(long)H5Eget_num(H5E_DEFAULT));return result;}
 return real(d,x,mask,off,n,data);
}
