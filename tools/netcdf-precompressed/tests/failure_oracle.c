#define main original_oracle_main
#include "../oracle.c"
#undef main
int main(int argc,char **argv){
 if(argc!=3||chdir(argv[1]))return 2;candidate=1;mode=1;
 beginfn=dlsym(RTLD_DEFAULT,"nc_precompressed_provider_begin");endfn=dlsym(RTLD_DEFAULT,"nc_precompressed_provider_end");
 void*h=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL|RTLD_DEEPBIND);stockdecode=dlsym(h,"uncompress");
 if(!beginfn||!endfn||!stockdecode)return 2;
 size_t c[4],s[4]={0};create("discard-injected-error.nc",3,NC_NETCDF4,0,0,NC_FLOAT,c);float a[63];bits(a,63,8);
 OK(beginfn(provider,NULL));EXPECT(nc_put_vara_float(ncid,varid,s,c,a),NC_EHDFERR);OK(endfn());if(calls!=1||releases!=1)fail("one callback/release no fallback",NC_EINVAL);
 /* Close is resource cleanup, never acceptance of the failed product. */
 OK(nc_close(ncid));if(unlink("discard-injected-error.nc"))return 2;
 calls=releases=0;create("fresh-scope.nc",3,NC_NETCDF4,0,0,NC_FLOAT,c);scoped_put(ncid,varid,s,c,a,1);bitread(ncid,varid,s,c,a,63);OK(nc_close(ncid));if(calls!=releases)fail("fresh release",NC_EINVAL);
 printf("PASS injectedHDFerror; cleanupdiscard; newhandle/scope calls%d releases%d\n",calls,releases);return 0;
}
