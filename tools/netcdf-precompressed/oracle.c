#define _GNU_SOURCE
#include <netcdf.h>
#include "netcdf_precompressed.h"
#include <zlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int candidate,mode,calls,releases,nd,ncid,varid,errors,default_fill,special_nan;
static int(*beginfn)(nc_precompressed_float_provider,void*);static int(*endfn)(void);
static int(*stockdecode)(Bytef*,uLongf*,const Bytef*,uLong);
static void fail(const char*s,int rc){fprintf(stderr,"FAIL %s %d %s\n",s,rc,nc_strerror(rc));exit(2);}
#define OK(call) do{int rc=(call);if(rc)fail(#call,rc);}while(0)
#define EXPECT(call,wanted) do{int rc=(call);if(rc!=(wanted))fail(#call,rc);}while(0)
typedef struct{unsigned char*b[4];int handle;}owned;
static void release(void*user){owned*p=user;if(mode==3&&candidate){EXPECT(nc_sync(p->handle),NC_EINVAL);EXPECT(endfn(),NC_EINVAL);}for(int i=0;i<4;i++)free(p->b[i]);free(p);releases++;}
static int provider(const nc_precompressed_float_layout*l,const void*raw,size_t bytes,nc_precompressed_float_batch*out,void*user){calls++;if(l->abi_version!=1||l->ndims<3||l->ndims>4||l->batch_count>4)fail("descriptor",NC_EINVAL);
size_t count=1;for(int d=0;d<l->ndims;d++)count*=l->request_count[d];if(count*4!=bytes)fail("rawbytes",NC_EINVAL);
owned*p=calloc(1,sizeof(*p));p->handle=l->ncid;out->release=release;out->release_user=p;out->count=l->batch_count;
if(mode==2)return 0;
if(mode==4)return NC_EIO;
if(mode==5&&l->batch_first)return 0;
if(mode==6){out->count=l->batch_count+1;return 1;}
if(mode==7&&l->batch_first)return NC_EIO;
if(mode==3){EXPECT(beginfn(provider,NULL),NC_EINVAL);EXPECT(endfn(),NC_EINVAL);EXPECT(nc_sync(l->ncid),NC_EINVAL);EXPECT(nc_close(l->ncid),NC_EINVAL);EXPECT(nc_put_vara_float(l->ncid,l->varid,l->request_start,l->request_count,raw),NC_EINVAL);int f;EXPECT(nc_create("provider-forbidden-create.nc",NC_NETCDF4|NC_CLOBBER,&f),NC_EINVAL);EXPECT(nc_open("does-not-matter.nc",NC_NOWRITE,&f),NC_EINVAL);}
size_t elems=1;for(int d=0;d<l->ndims;d++)elems*=l->chunk_dims[d];size_t n=elems*4;
unsigned char*chunk=malloc(n),*shuffled=malloc(n),*decoded=malloc(n);
for(size_t q=0;q<l->batch_count;q++){
 for(size_t k=0;k<elems;k++){size_t flat=k,source=0,relative[4];int valid=1;
  for(int d=l->ndims-1;d>=0;d--){relative[d]=flat%l->chunk_dims[d];flat/=l->chunk_dims[d];size_t absolute=l->offsets[q][d]+relative[d];if(absolute<l->request_start[d])valid=0;else{relative[d]=absolute-l->request_start[d];if(relative[d]>=l->request_count[d])valid=0;}}
  for(int d=0;d<l->ndims;d++)source=source*l->request_count[d]+relative[d];
  memcpy(chunk+4*k,valid?(const unsigned char*)raw+source*4:l->fill_le,4);
 }
 for(size_t k=0;k<elems;k++)for(int b=0;b<4;b++)shuffled[b*elems+k]=chunk[4*k+b];
 uLongf compressed=compressBound(n);p->b[q]=malloc(compressed);if(compress2(p->b[q],&compressed,shuffled,n,2)!=Z_OK)fail("compress",NC_EIO);out->data[q]=p->b[q];out->nbytes[q]=compressed;
 for(int decoder=0;decoder<2;decoder++){uLongf got=n;int rc=decoder?stockdecode(decoded,&got,p->b[q],compressed):uncompress(decoded,&got,p->b[q],compressed);if(rc!=Z_OK||got!=n||memcmp(decoded,shuffled,n))fail("dual exact",NC_EIO);}
}
free(chunk);free(shuffled);free(decoded);return 1;}
static void scoped_put(int id,int v,size_t*s,size_t*c,float*data,int selected){int before=calls;if(candidate&&selected)OK(beginfn(provider,NULL));OK(nc_put_vara_float(id,v,s,c,data));if(candidate&&selected)OK(endfn());if(candidate&&selected&&mode!=2&&calls==before)fail("provider not executed",NC_EINVAL);}
static void bits(float*a,size_t n,int seed){for(size_t i=0;i<n;i++){float x=(float)((int)i*17-seed*11)/13.f;uint32_t v;memcpy(&v,&x,4);if(i%11==0)v=0x80000000;if(i%17==0)v=special_nan?0x7fc01234:0x3fc01234;memcpy(a+i,&v,4);}}
static void bitread(int id,int v,size_t*s,size_t*c,float*a,size_t n){float*b=malloc(n*4);OK(nc_get_vara_float(id,v,s,c,b));if(memcmp(a,b,n*4))fail("bit read",NC_EINVAL);free(b);}
static void create(const char*path,int dims,int flags,int extra,int nofill,int type,size_t*count){int did[4];OK(nc_create(path,NC_CLOBBER|flags,&ncid));OK(nc_def_dim(ncid,"Time",NC_UNLIMITED,&did[0]));int d=1;if(dims==4)OK(nc_def_dim(ncid,"z",5,&did[d++]));OK(nc_def_dim(ncid,"y",7,&did[d++]));OK(nc_def_dim(ncid,"x",9,&did[d++]));OK(nc_def_var(ncid,"F",type,dims,did,&varid));size_t ch[4];ch[0]=1;d=1;if(dims==4)ch[d++]=2;ch[d++]=3;ch[d++]=4;OK(nc_def_var_chunking(ncid,varid,NC_CHUNKED,ch));OK(nc_def_var_deflate(ncid,varid,1,1,2));if(extra)OK(nc_def_var_fletcher32(ncid,varid,1));if(type==NC_FLOAT&&!default_fill){float fill=-9999.f;OK(nc_def_var_fill(ncid,varid,nofill,&fill));}OK(nc_put_att_text(ncid,NC_GLOBAL,"purpose",6,"oracle"));OK(nc_put_att_text(ncid,varid,"units",1,"K"));OK(nc_enddef(ncid));count[0]=1;d=1;if(dims==4)count[d++]=5;count[d++]=7;count[d++]=9;}
static void normal(const char*path,int dims,int selected){size_t count[4],s[4]={0};create(path,dims,NC_NETCDF4,0,0,NC_FLOAT,count);size_t n=1;for(int d=0;d<dims;d++)n*=count[d];float*a=malloc(n*4);bits(a,n,1);scoped_put(ncid,varid,s,count,a,selected);bitread(ncid,varid,s,count,a,n);
/* Append Time2 with gapTime1, then dirty partial and replace cached Time0. */
s[0]=2;bits(a,n,2);scoped_put(ncid,varid,s,count,a,selected);bitread(ncid,varid,s,count,a,n);s[0]=0;size_t one[4]={1,1,1,1};float single=99.125f;OK(nc_put_vara_float(ncid,varid,s,one,&single));float check;OK(nc_get_vara_float(ncid,varid,s,one,&check));bits(a,n,3);scoped_put(ncid,varid,s,count,a,selected);bitread(ncid,varid,s,count,a,n);
/* Direct then ordinary partial overwrite, sync/close/reopen proves cache order. */
OK(nc_put_vara_float(ncid,varid,s,one,&single));memcpy(a,&single,4);bitread(ncid,varid,s,count,a,n);OK(nc_sync(ncid));OK(nc_close(ncid));OK(nc_open(path,NC_WRITE,&ncid));OK(nc_inq_varid(ncid,"F",&varid));bitread(ncid,varid,s,count,a,n);
/* Scope ended: no stale callback on reopen or ordinary write. */
int before=calls;s[0]=3;bits(a,n,4);OK(nc_put_vara_float(ncid,varid,s,count,a));if(calls!=before)fail("stale scope",NC_EINVAL);bitread(ncid,varid,s,count,a,n);OK(nc_close(ncid));free(a);}
static void fallback(const char*path,int which){size_t c[4],s[4]={0};int type=which==0?NC_INT:NC_FLOAT;create(path,3,NC_NETCDF4,which==1,which==2,type,c);float a[63];for(int i=0;i<63;i++)a[i]=i+.25f;int before=calls;if(candidate)OK(beginfn(provider,NULL));if(which==3){c[1]=2;c[2]=3;}if(which==4)c[1]=0;OK(nc_put_vara_float(ncid,varid,s,c,a));if(candidate)OK(endfn());if(calls!=before)fail("unsupported callback",NC_EINVAL);OK(nc_close(ncid));}
static void fatal(const char*path,int m){size_t c[4],s[4]={0};create(path,3,NC_NETCDF4,0,0,NC_FLOAT,c);float a[63];bits(a,63,7);mode=m;calls=0;releases=0;OK(beginfn(provider,NULL));int rc=nc_put_vara_float(ncid,varid,s,c,a);int wanted=(m==4||m==7)?NC_EIO:NC_EINVAL;if(rc!=wanted)fail("expected terminal",rc);OK(endfn());if(calls!=releases)fail("terminal release",NC_EINVAL);OK(nc_close(ncid));printf("FATAL mode%d calls%d releases%d rc%d\n",m,calls,releases,rc);}
static void*thread_probe(void*p){EXPECT(beginfn(provider,NULL),NC_EINVAL);EXPECT(endfn(),NC_EINVAL);return NULL;}
int main(int argc,char**argv){if(argc!=4)return 2;candidate=atoi(argv[1]);if(chdir(argv[2]))return 2;beginfn=dlsym(RTLD_DEFAULT,"nc_precompressed_provider_begin");endfn=dlsym(RTLD_DEFAULT,"nc_precompressed_provider_end");if(candidate&&(!beginfn||!endfn))fail("owner symbols",NC_EINVAL);void*h=dlopen(argv[3],RTLD_NOW|RTLD_LOCAL|RTLD_DEEPBIND);if(!h)return 2;stockdecode=dlsym(h,"uncompress");Dl_info di;if(!dladdr(dlsym(RTLD_DEFAULT,"nc_put_vara_float"),&di))return 2;printf("NETCDF %s\n",di.dli_fname);if(!dladdr(dlsym(RTLD_DEFAULT,"compress2"),&di))return 2;printf("ZLIB %s\n",di.dli_fname);
for(mode=0;mode<=3;mode++){int selected=mode!=0;for(int dims=3;dims<=4;dims++){char file[80];snprintf(file,sizeof file,"normal-mode%d-dim%d.nc",mode,dims);normal(file,dims,selected);printf("NORMAL mode%d dims%d calls%d releases%d\n",mode,dims,calls,releases);if(calls!=releases)fail("release mismatch",NC_EINVAL);}}
mode=1;default_fill=1;normal("normal-defaultfill-dim4.nc",4,1);default_fill=0;special_nan=1;normal("normal-nan-dim4.nc",4,1);special_nan=0;for(int f=0;f<5;f++){char file[80];snprintf(file,sizeof file,"fallback-%d.nc",f);fallback(file,f);}
if(candidate){pthread_t t;OK(beginfn(provider,NULL));if(pthread_create(&t,NULL,thread_probe,NULL)||pthread_join(t,NULL))return 2;OK(endfn());for(int m=4;m<=7;m++){char file[80];snprintf(file,sizeof file,"failed-mode%d.nc",m);fatal(file,m);}EXPECT(beginfn(NULL,NULL),NC_EINVAL);OK(endfn());}
printf("PASS candidate%d\n",candidate);return 0;}
