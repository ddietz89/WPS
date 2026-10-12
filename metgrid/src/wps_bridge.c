#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* Codex local implementation after bounded hosted dispatch rejection. */
#include <mpi.h>
#include <zlib.h>
#include <netcdf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include "netcdf_precompressed.h"
#if defined(WPS_ROOT_DOMAIN_REUSE)
#ifdef __linux__
#include <sys/syscall.h>
#endif
/* Optional output-domain bank gate: no MPI/OpenMP call, including owner OMP=0. */
int wps_chunk_is_main_thread(void) {
#ifdef __linux__
 return syscall(SYS_gettid) == (long)getpid();
#else
 return 0;
#endif
}
#endif
#ifdef WPS_OPENMP_FUNNELED
#include <sys/syscall.h>
static int wps_omp_main_thread(void) {
#ifdef __linux__
 return syscall(SYS_gettid) == (long)getpid();
#else
 return 0;
#endif
}
#endif
#define MAGIC 0x57505343u
#define WORK 1
#define DONE 2
#define CHUNK_LIMIT (64u*1024u*1024u)
#define WORKSPACE_CAP (256u*1024u*1024u)
typedef struct wps_chunk_session {
 MPI_Comm comm; int rank,size,root,done,busy; unsigned magic; size_t work_batches;
} wps_chunk_session;
typedef struct {unsigned char *blob;} result_storage;
/* Process-local ROOT packed-input scratch. Main-thread-only, no locks. */
static unsigned char *workspace_ptr;
static size_t workspace_capacity;
static int workspace_leased;
/* Proof counters only; accessed by the serialized main thread. */
static struct {int known,rank,size;uint64_t sessions,provider_calls,work_batches,chunks,raw_bytes,compressed_bytes,declines,provider_errors,done,done_errors,no_work,closes;} proof;

static void mpi_check(wps_chunk_session *s,int e){if(e!=MPI_SUCCESS)MPI_Abort(s->comm,e);}
static int agree(wps_chunk_session *s,int bad){int out;mpi_check(s,MPI_Allreduce(&bad,&out,1,MPI_INT,MPI_MAX,s->comm));return out;}
static void release_result(void *p){result_storage *s=p;if(s){free(s->blob);free(s);}}
static int multiply(size_t a,size_t b,size_t *out){if(b&&a>SIZE_MAX/b)return 0;*out=a*b;return 1;}
static unsigned char *rx_cache_ptr;
static size_t rx_cache_capacity;
static unsigned char *shuf_cache_ptr;
static size_t shuf_cache_capacity;
static unsigned char *comp_cache_ptr;
static size_t comp_cache_capacity;
static int rank_scratch_leased;

#define RANK_SCRATCH_CAP (256u*1024u*1024u)

/* Take a fresh owned allocation for one scratch role; caller frees on every path. */
static unsigned char *rank_scratch_fresh(size_t need){
 unsigned char *p=malloc(need?need:1);
 return p;
}

/* Borrow process-local cached scratch if capacity suffices, else own a fresh
   malloc. *owned tells the caller which release path to use. Request above
   RANK_SCRATCH_CAP always uses owned fresh malloc; cache untouched. */
static unsigned char *rank_scratch_get(unsigned char **cache,size_t *cap,size_t need,int *owned,int *bad){
 unsigned char *p=NULL;
 if(!need)need=1;
 if(need>RANK_SCRATCH_CAP){p=rank_scratch_fresh(need);*owned=1;*bad=!p;return p;}
 if(*cap<need){
  unsigned char *fresh=rank_scratch_fresh(need);
  if(!fresh){*owned=0;*bad=1;return NULL;}
  free(*cache);*cache=fresh;*cap=need;
 }
 *owned=0;*bad=0;return *cache;
}

/* Release a role's buffer: owned fresh frees immediately; borrowed cache stays. */
static void rank_scratch_put(unsigned char *owned_ptr,unsigned char **cache,size_t *cap,int owned){
 if(owned){free(owned_ptr);(void)cache;(void)cap;}
}

int wps_chunk_workspace_clear(void){
#ifdef WPS_OPENMP_FUNNELED
 /* Reject worker entry before touching cache; FUNNELED main thread only. */
 if(!wps_omp_main_thread())return NC_EINVAL;
#endif
 if(workspace_leased)return NC_EINVAL;
 if(rank_scratch_leased)return NC_EINVAL;
 free(workspace_ptr);workspace_ptr=NULL;workspace_capacity=0;
 free(rx_cache_ptr);rx_cache_ptr=NULL;rx_cache_capacity=0;
 free(shuf_cache_ptr);shuf_cache_ptr=NULL;shuf_cache_capacity=0;
 free(comp_cache_ptr);comp_cache_ptr=NULL;comp_cache_capacity=0;
 return NC_NOERR;
}

int wps_chunk_session_create(MPI_Fint fcomm,int root,const char *field,int eligible,wps_chunk_session **out){
#ifdef WPS_OPENMP_FUNNELED
 /* Reject worker entry before any MPI call; FUNNELED allows only main thread. */
 if(!wps_omp_main_thread())return NC_EINVAL;
#endif
 wps_chunk_session temp;memset(&temp,0,sizeof temp);temp.comm=MPI_Comm_f2c(fcomm);
 
 mpi_check(&temp,MPI_Comm_rank(temp.comm,&temp.rank));mpi_check(&temp,MPI_Comm_size(temp.comm,&temp.size));
 int level,main_thread;mpi_check(&temp,MPI_Query_thread(&level));mpi_check(&temp,MPI_Is_thread_main(&main_thread));
 int bad=!out||root!=0||!field||(field&&strlen(field)>127)||
#ifdef WPS_OPENMP_FUNNELED
 (level!=MPI_THREAD_SINGLE&&level!=MPI_THREAD_FUNNELED)||
#else
 level!=MPI_THREAD_SINGLE||
#endif
 !main_thread;if(agree(&temp,bad))return NC_EINVAL;*out=NULL;
 char name[128]={0};if(temp.rank==root)memcpy(name,field,strlen(field)+1);
 mpi_check(&temp,MPI_Bcast(name,128,MPI_CHAR,root,temp.comm));
 int low,high;eligible=!!eligible;
 mpi_check(&temp,MPI_Allreduce(&eligible,&low,1,MPI_INT,MPI_MIN,temp.comm));
 mpi_check(&temp,MPI_Allreduce(&eligible,&high,1,MPI_INT,MPI_MAX,temp.comm));
 if(agree(&temp,strcmp(name,field)!=0||low!=high))return NC_EINVAL;
 if(!low||(temp.size!=2&&temp.size!=4))return NC_NOERR;
 wps_chunk_session *s=calloc(1,sizeof *s);if(agree(&temp,s==NULL)){free(s);return NC_ENOMEM;}
 *s=temp;s->root=root;s->magic=MAGIC;*out=s;proof.known=1;proof.rank=temp.rank;proof.size=temp.size;proof.sessions++;return NC_NOERR;
}
int wps_chunk_schema_agree(MPI_Fint fcomm,const int *values,int count){
 wps_chunk_session s;memset(&s,0,sizeof s);s.comm=MPI_Comm_f2c(fcomm);
 if(agree(&s,!values||count<1||count>24))return NC_EINVAL;
 int counts[2]={count,-count},bounds[2];mpi_check(&s,MPI_Allreduce(counts,bounds,2,MPI_INT,MPI_MAX,s.comm));
 if(bounds[0]!=-bounds[1])return NC_EINVAL;
 int lo[24],hi[24];mpi_check(&s,MPI_Allreduce(values,lo,count,MPI_INT,MPI_MIN,s.comm));
 mpi_check(&s,MPI_Allreduce(values,hi,count,MPI_INT,MPI_MAX,s.comm));
 return memcmp(lo,hi,(size_t)count*sizeof(int))?NC_EINVAL:NC_NOERR;
}
static int layout_valid(const nc_precompressed_float_layout *l,size_t bytes,size_t *chunk_bytes){
 size_t raw=1,n=1;uint32_t one=1;
 if(!l||l->abi_version!=1||l->ndims<1||l->ndims>4||!l->batch_count||l->batch_count>4||
    l->batch_first>l->total_chunks||l->batch_count>l->total_chunks-l->batch_first||
    *(unsigned char*)&one!=1||sizeof(float)!=4||FLT_RADIX!=2||FLT_MANT_DIG!=24||FLT_MAX_EXP!=128)return 0;
 for(int d=0;d<l->ndims;d++){
  if(!l->request_count[d]||!l->chunk_dims[d]||l->request_start[d]>SIZE_MAX-l->request_count[d]||
     !multiply(raw,l->request_count[d],&raw)||!multiply(n,l->chunk_dims[d],&n))return 0;
  for(size_t c=0;c<l->batch_count;c++)if(l->offsets[c][d]%l->chunk_dims[d]||
     l->offsets[c][d]>SIZE_MAX-l->chunk_dims[d]||
     l->offsets[c][d]>=l->request_start[d]+l->request_count[d]||
     l->offsets[c][d]+l->chunk_dims[d]<=l->request_start[d])return 0;
 }
 if(!multiply(raw,4,&raw)||raw!=bytes||!multiply(n,4,&n)||n>CHUNK_LIMIT||n>INT_MAX/4)return 0;
 *chunk_bytes=n;return 1;
}
static void pack_chunk(const nc_precompressed_float_layout *l,size_t c,const unsigned char *raw,unsigned char *out,size_t nbytes){
 memcpy(out,l->fill_le,4);
 for(size_t filled=4;filled<nbytes;){size_t n=filled<nbytes-filled?filled:nbytes-filled;memcpy(out+filled,out,n);filled+=n;}
 int last=l->ndims-1;size_t width=l->chunk_dims[last],rows=nbytes/4/width;
 size_t lo=l->offsets[c][last]>l->request_start[last]?l->offsets[c][last]:l->request_start[last];
 size_t hi=l->offsets[c][last]+width,limit=l->request_start[last]+l->request_count[last];if(hi>limit)hi=limit;
 if(hi<=lo)return;
 for(size_t row=0;row<rows;row++){
  size_t rem=row,index=0,pos[4]={0};int inside=1;
  for(int d=last-1;d>=0;d--){pos[d]=l->offsets[c][d]+rem%l->chunk_dims[d];rem/=l->chunk_dims[d];
   if(pos[d]<l->request_start[d]||pos[d]>=l->request_start[d]+l->request_count[d])inside=0;}
  if(!inside)continue;
  for(int d=0;d<last;d++)index=index*l->request_count[d]+pos[d]-l->request_start[d];
  index=index*l->request_count[last]+lo-l->request_start[last];
  memcpy(out+4*(row*width+lo-l->offsets[c][last]),raw+4*index,4*(hi-lo));
 }
}
/* Every participant calls batch_work once per WORK, including failed batches. */
static int batch_work(wps_chunk_session *s,nc_precompressed_float_layout *l,unsigned char *packed,size_t chunk_bytes,nc_precompressed_float_batch *result){
 int rc=NC_EINVAL,raw_counts[4]={0},raw_disps[4]={0},owners[4],local_count=0;
 size_t local_ids[4],bound=compressBound((uLong)chunk_bytes),local_capacity=0;
 unsigned char *recv=NULL,*shuffle=NULL,*compressed=NULL;result_storage *storage=NULL;
 unsigned long long lengths[4]={0},all_lengths[16]={0};
 int send_counts[4]={0},send_disps[4]={0};size_t local_used=0,total_used=0;
 int recv_owned=0,shuffle_owned=0,compressed_owned=0,lease_acquired=0,alloc_bad=0;
 for(size_t c=0;c<l->batch_count;c++){int rank=(int)((l->batch_first+c)%(size_t)s->size);owners[c]=rank;raw_counts[rank]+=(int)chunk_bytes;if(rank==s->rank)local_ids[local_count++]=c;}
 for(int r=1;r<s->size;r++)raw_disps[r]=raw_disps[r-1]+raw_counts[r-1];
 int bad=chunk_bytes>ULONG_MAX||bound<chunk_bytes||!multiply(bound,(size_t)local_count,&local_capacity)||local_capacity>INT_MAX||rank_scratch_leased;
 if(!bad){rank_scratch_leased=1;lease_acquired=1;}
 if(!bad){
  recv=rank_scratch_get(&rx_cache_ptr,&rx_cache_capacity,raw_counts[s->rank]?raw_counts[s->rank]:1,&recv_owned,&alloc_bad);bad=alloc_bad;
 }
 if(!bad){
  shuffle=rank_scratch_get(&shuf_cache_ptr,&shuf_cache_capacity,chunk_bytes,&shuffle_owned,&alloc_bad);bad=alloc_bad;
 }
 if(!bad){
  compressed=rank_scratch_get(&comp_cache_ptr,&comp_cache_capacity,local_capacity?local_capacity:1,&compressed_owned,&alloc_bad);bad=alloc_bad;
 }
 if(agree(s,bad)){rc=NC_ENOMEM;goto cleanup;}
 mpi_check(s,MPI_Scatterv(packed,raw_counts,raw_disps,MPI_BYTE,recv,raw_counts[s->rank],MPI_BYTE,s->root,s->comm));
 bad=0;
 for(int j=0;j<local_count;j++){
  const unsigned char *src=recv+(size_t)j*chunk_bytes;size_t items=chunk_bytes/4;
  for(size_t i=0;i<items;i++)for(size_t b=0;b<4;b++)shuffle[b*items+i]=src[4*i+b];
  uLongf n=(uLongf)bound;int z=compress2(compressed+local_used,&n,shuffle,(uLong)chunk_bytes,2);
  if(z!=Z_OK||n>bound){bad=1;break;}lengths[local_ids[j]]=n;local_used+=(size_t)n;
 }
 if(agree(s,bad)){rc=NC_EIO;goto cleanup;}
 mpi_check(s,MPI_Gather(lengths,4,MPI_UNSIGNED_LONG_LONG,all_lengths,4,MPI_UNSIGNED_LONG_LONG,s->root,s->comm));
 if(s->rank==s->root){
  bad=0;
  for(size_t c=0;c<l->batch_count;c++)for(int r=0;r<s->size;r++){
   unsigned long long n=all_lengths[4*r+c];
   if(r==owners[c]){if(!n||n>bound||n>(unsigned long long)(INT_MAX-send_counts[r]))bad=1;else send_counts[r]+=(int)n;}
   else if(n)bad=1;
  }
  for(int r=0;r<s->size;r++){if(total_used>(size_t)INT_MAX-(size_t)send_counts[r])bad=1;else{send_disps[r]=(int)total_used;total_used+=(size_t)send_counts[r];}}
  if(!bad){storage=calloc(1,sizeof *storage);if(storage)storage->blob=malloc(total_used?total_used:1);bad=!storage||!storage->blob;}
 }else bad=0;
 if(agree(s,bad)){rc=NC_ENOMEM;goto cleanup;}
 mpi_check(s,MPI_Gatherv(compressed,(int)local_used,MPI_BYTE,s->rank==s->root?storage->blob:NULL,send_counts,send_disps,MPI_BYTE,s->root,s->comm));
 if(s->rank==s->root){
  size_t offsets[4]={0};for(int r=0;r<s->size;r++)offsets[r]=(size_t)send_disps[r];
  for(size_t c=0;c<l->batch_count;c++){int r=owners[c];size_t n=(size_t)all_lengths[4*r+c];result->data[c]=storage->blob+offsets[r];result->nbytes[c]=n;offsets[r]+=n;}
  result->count=l->batch_count;result->release=release_result;result->release_user=storage;storage=NULL;
 }
 rc=NC_NOERR;
cleanup:
 if(lease_acquired)rank_scratch_leased=0;
 rank_scratch_put(recv,&rx_cache_ptr,&rx_cache_capacity,recv_owned);
 rank_scratch_put(shuffle,&shuf_cache_ptr,&shuf_cache_capacity,shuffle_owned);
 rank_scratch_put(compressed,&comp_cache_ptr,&comp_cache_capacity,compressed_owned);
 release_result(storage);return rc;
}
int wps_chunk_provider(const nc_precompressed_float_layout *layout,const void *raw,size_t bytes,nc_precompressed_float_batch *result,void *user){
 wps_chunk_session *s=user;size_t chunk_bytes=0;unsigned char *packed=NULL;
 if(!s||s->magic!=MAGIC||s->rank!=s->root||s->done||s->busy||!result)return NC_EINVAL;
 memset(result,0,sizeof *result);proof.provider_calls++;
 if(!raw||!layout_valid(layout,bytes,&chunk_bytes)){proof.declines++;return 0;}
 size_t total;if(!multiply(chunk_bytes,layout->batch_count,&total)||total>WORKSPACE_CAP){proof.provider_errors++;return NC_ENOMEM;}
 if(workspace_leased)return NC_EINVAL;
 if(workspace_capacity<total){
  unsigned char *fresh=malloc(total);
  if(!fresh){proof.provider_errors++;return NC_ENOMEM;}
  free(workspace_ptr);workspace_ptr=fresh;workspace_capacity=total;
 }
 packed=workspace_ptr;workspace_leased=1;
 int ranks[4]={0},base[4]={0};for(size_t c=0;c<layout->batch_count;c++)ranks[(layout->batch_first+c)%(size_t)s->size]++;
 for(int r=1;r<s->size;r++)base[r]=base[r-1]+ranks[r-1];int filled[4]={0};
 for(size_t c=0;c<layout->batch_count;c++){int r=(int)((layout->batch_first+c)%(size_t)s->size);pack_chunk(layout,c,raw,packed+(size_t)(base[r]+filled[r]++)*chunk_bytes,chunk_bytes);}
 s->busy=1;s->work_batches++;proof.work_batches++;proof.chunks+=layout->batch_count;proof.raw_bytes+=total;int cmd=WORK;nc_precompressed_float_layout copy=*layout;
 mpi_check(s,MPI_Bcast(&cmd,1,MPI_INT,s->root,s->comm));
 mpi_check(s,MPI_Bcast(&copy,(int)sizeof copy,MPI_BYTE,s->root,s->comm));
 unsigned long long n=chunk_bytes;mpi_check(s,MPI_Bcast(&n,1,MPI_UNSIGNED_LONG_LONG,s->root,s->comm));
 if(agree(s,0))MPI_Abort(s->comm,NC_EINVAL);
 int rc=batch_work(s,&copy,packed,chunk_bytes,result);workspace_leased=0;s->busy=0;if(rc!=NC_NOERR)proof.provider_errors++;else for(size_t i=0;i<result->count;i++)proof.compressed_bytes+=result->nbytes[i];return rc==NC_NOERR?1:rc;
}
int wps_chunk_workers(wps_chunk_session *s){
 if(!s||s->magic!=MAGIC||s->rank==s->root||s->done||s->busy)return NC_EINVAL;
 for(;;){int cmd;mpi_check(s,MPI_Bcast(&cmd,1,MPI_INT,s->root,s->comm));
  if(cmd==DONE){int status;mpi_check(s,MPI_Bcast(&status,1,MPI_INT,s->root,s->comm));s->done=1;proof.done++;proof.done_errors+=(status!=0);return status;}
  if(cmd!=WORK){MPI_Abort(s->comm,NC_EINVAL);return NC_EINVAL;}
  nc_precompressed_float_layout l;unsigned long long n;
  mpi_check(s,MPI_Bcast(&l,(int)sizeof l,MPI_BYTE,s->root,s->comm));mpi_check(s,MPI_Bcast(&n,1,MPI_UNSIGNED_LONG_LONG,s->root,s->comm));
  size_t checked=0,raw=1;int bad=n>SIZE_MAX||l.ndims<1||l.ndims>4;
  if(!bad)for(int d=0;d<l.ndims;d++)if(!multiply(raw,l.request_count[d],&raw))bad=1;
  if(!multiply(raw,4,&raw)||!layout_valid(&l,raw,&checked)||checked!=(size_t)n)bad=1;
  if(agree(s,bad))MPI_Abort(s->comm,NC_EINVAL);
  s->busy=1;s->work_batches++;proof.work_batches++;proof.chunks+=l.batch_count;(void)batch_work(s,&l,NULL,(size_t)n,NULL);s->busy=0;
 }
}
int wps_chunk_done(wps_chunk_session *s,int status){
 if(!s||s->magic!=MAGIC||s->rank!=s->root||s->done||s->busy)return NC_EINVAL;
 int cmd=DONE;mpi_check(s,MPI_Bcast(&cmd,1,MPI_INT,s->root,s->comm));mpi_check(s,MPI_Bcast(&status,1,MPI_INT,s->root,s->comm));s->done=1;proof.done++;proof.done_errors+=(status!=0);return status;
}
void wps_chunk_session_destroy(wps_chunk_session *s){if(s){if(!s->work_batches)proof.no_work++;s->magic=0;free(s);}}

int wps_chunk_proof_write(void){
 const char *dir=getenv("WPS_CHUNK_PROOF_DIR"),*run=getenv("WPS_CHUNK_PROOF_RUN_ID");
 if(!dir||!*dir)return NC_NOERR;
 if(!run||!*run||strlen(run)>128)return NC_EINVAL;
 if(!proof.known){int initialized=0;MPI_Initialized(&initialized);if(!initialized)return NC_EINVAL;MPI_Comm_rank(MPI_COMM_WORLD,&proof.rank);MPI_Comm_size(MPI_COMM_WORLD,&proof.size);proof.known=1;}
 for(const unsigned char *p=(const unsigned char*)run;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'))return NC_EINVAL;
 char path[4096];proof.closes++;int n=snprintf(path,sizeof path,"%s/provider-rank%d-pid%ld-close%"PRIu64".json",dir,proof.rank,(long)getpid(),proof.closes);
 if(n<0||(size_t)n>=sizeof path)return NC_EINVAL;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return NC_EIO;
 FILE *f=fdopen(fd,"w");if(!f){close(fd);return NC_EIO;}
 int err=fprintf(f,"{\"status\":\"completed\",\"run_id\":\"%s\",\"rank\":%d,\"size\":%d,\"pid\":%ld,\"cumulative\":true,\"sessions\":%"PRIu64",\"provider_calls\":%"PRIu64",\"work_batches\":%"PRIu64",\"chunks\":%"PRIu64",\"raw_padded_bytes\":%"PRIu64",\"compressed_bytes\":%"PRIu64",\"provider_declines\":%"PRIu64",\"provider_errors\":%"PRIu64",\"terminal_done\":%"PRIu64",\"terminal_errors\":%"PRIu64",\"sessions_no_work\":%"PRIu64"}\n",run,proof.rank,proof.size,(long)getpid(),proof.sessions,proof.provider_calls,proof.work_batches,proof.chunks,proof.raw_bytes,proof.compressed_bytes,proof.declines,proof.provider_errors,proof.done,proof.done_errors,proof.no_work)<0;
 if(fclose(f)!=0)err=1;return err?NC_EIO:NC_NOERR;
}

#ifdef WPS_ORACLE_TRACE
int wps_chunk_oracle_trace(wps_chunk_session *s,uint64_t trace[3]){
 if(!s||s->magic!=MAGIC||!trace)return NC_EINVAL;
 trace[0]=s->work_batches;trace[1]=s->done;trace[2]=s->busy;return NC_NOERR;
}
#endif
