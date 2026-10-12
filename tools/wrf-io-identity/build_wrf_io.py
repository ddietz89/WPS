#!/usr/bin/env python3
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path

SRC_SHA='c3cb2d549f5547cf9bb86c080e5b5a332c2280da408a9b0be1988e9b6ca8c4ef'
PATCH_SHA='b8a624970560a345e528b1b96c114aa00735642b853563207d9389dbcf652e70'
PATCHED_SHA='494ca8c7eab58d00c4899f6d7b535731de41327bcc8305dd166d03b4a01edc31'
ARCHIVE_SHA='00121efbb91287bdc72e20651cd9dae77bbd01930006615f66fe56fbda61053f'
FLAGS=['-O2','-fno-fast-math','-ffp-contract=off','-ffree-form','-ffree-line-length-none','-fconvert=big-endian','-frecord-marker=4','-fallow-argument-mismatch','-fallow-invalid-boz']
FORBIDDEN={'/pool/wrf/WPS'}

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--wrf-root',required=True)
    ap.add_argument('--output',required=True)
    ap.add_argument('--patch',required=True)
    ap.add_argument('--fc',required=True)
    ap.add_argument('--netcdf-fortran-prefix',required=True)
    ap.add_argument('--expected-archive-sha',default=ARCHIVE_SHA)
    a=ap.parse_args()
    for value in (a.wrf_root,a.output,a.patch,a.fc,a.netcdf_fortran_prefix):
        if not Path(value).is_absolute(): sys.exit('all paths must be absolute')
    if Path(a.output).is_symlink(): sys.exit('output must not be a symlink')
    W=Path(a.wrf_root).resolve();O=Path(a.output).resolve();P=Path(a.patch).resolve();FC=Path(a.fc).resolve();NC=Path(a.netcdf_fortran_prefix).resolve()
    if not W.is_dir(): sys.exit('wrf-root missing')
    src=W/'external/io_netcdf/wrf_io.F90'
    arc=W/'external/io_netcdf/libwrfio_nf.a'
    ioapi=W/'external/ioapi_share'
    for req in (src,arc,ioapi,P,FC,NC/'include/netcdf.inc',W/'frame',W/'inc'):
        if not req.exists(): sys.exit('missing required: %s'%req)
    if O.exists(): sys.exit('output must not exist')
    if str(O)==str(W) or W in O.parents: sys.exit('output inside original WRF root')
    for f in FORBIDDEN:
        if O==Path(f).resolve() or Path(f).resolve() in O.parents: sys.exit('forbidden output path')
    if O.is_symlink(): sys.exit('output aliases')
    if sha(src)!=SRC_SHA: sys.exit('source SHA mismatch')
    if sha(P)!=PATCH_SHA: sys.exit('patch SHA mismatch')
    if not a.expected_archive_sha or a.expected_archive_sha==ARCHIVE_SHA:
        if sha(arc)!=ARCHIVE_SHA: sys.exit('archive SHA mismatch (configured/alt archives need caller SHA)')
    else:
        if sha(arc)!=a.expected_archive_sha: sys.exit('archive SHA mismatch vs caller pin')
    ver=subprocess.check_output([str(FC),'-dumpfullversion'],text=True).strip()
    banner=subprocess.check_output([str(FC),'--version'],text=True)
    if ver!='13.3.0' or 'GNU Fortran' not in banner: sys.exit('only qualified GNU13.3.0 supported')
    input_pins={str(src):sha(src),str(arc):sha(arc),str(P):sha(P),str(NC/'include/netcdf.inc'):sha(NC/'include/netcdf.inc')}
    for f in sorted(list(ioapi.glob('*.h'))+list(arc.parent.glob('*.code'))+list(arc.parent.glob('*.mod'))+list(arc.parent.glob('*.smod'))): input_pins[str(f)]=sha(f)
    arches_before=subprocess.run(['ar','t',str(arc)],capture_output=True,text=True,check=True).stdout.split()
    members_before={m:subprocess.run(['ar','p',str(arc),m],capture_output=True,check=True).stdout for m in arches_before}
    O.mkdir()
    (O/'frame').symlink_to(W/'frame')
    (O/'inc').symlink_to(W/'inc')
    ext_out=O/'external'; ext_out.mkdir()
    for d in sorted((W/'external').iterdir()):
        if d.name=='io_netcdf': continue
        (ext_out/d.name).symlink_to(d)
    ion=ext_out/'io_netcdf'; ion.mkdir()
    orig_arc=arc
    shutil.copy2(src,ion/'wrf_io.F90')
    for f in sorted(orig_arc.parent.iterdir()):
        # Compiler owns fresh module outputs; never expose original .mod files
        # as symlink write targets. All unchanged objects remain read-only.
        if f.name in ('wrf_io.F90','wrf_io.f','wrf_io.o','libwrfio_nf.a') or f.suffix in ('.mod','.smod'): continue
        if f.suffix=='.code': shutil.copy2(f,ion/f.name)
        else: (ion/f.name).symlink_to(f)
    r=subprocess.run(['patch','--batch','--forward','--fuzz=0','-p1','-i',str(P)],cwd=str(O),capture_output=True,text=True)
    if r.returncode!=0: sys.exit('patch failed: '+r.stdout+r.stderr)
    if sha(ion/'wrf_io.F90')!=PATCHED_SHA: sys.exit('post-patch source SHA mismatch')
    cpp=['/usr/bin/cpp','-P','-traditional-cpp','-DUSE_NETCDF4_FEATURES','-DWRFIO_NCD_LARGE_FILE_SUPPORT','-I'+str(ioapi),'wrf_io.F90']
    r=subprocess.run(cpp,cwd=str(ion),capture_output=True,text=True)
    if r.returncode!=0: sys.exit('cpp failed: '+r.stderr)
    r=subprocess.run(['m4','-Uinclude','-Uindex','-Ulen','-'],cwd=str(ion),input=r.stdout,capture_output=True,text=True)
    if r.returncode!=0: sys.exit('m4 failed: '+r.stderr)
    (ion/'wrf_io.f').write_text(r.stdout)
    cc=[str(FC)]+FLAGS+['-I'+str(NC/'include'),'-I'+str(ioapi),'-c','wrf_io.f','-o','wrf_io.o']
    r=subprocess.run(cc,cwd=str(ion),capture_output=True,text=True)
    (O/'build.log').write_text(r.stdout+r.stderr)
    sys.stdout.write(r.stdout+r.stderr)
    if r.returncode!=0: sys.exit('compile failed')
    shutil.copy2(orig_arc,ion/'libwrfio_nf.a')
    r=subprocess.run(['ar','r','libwrfio_nf.a','wrf_io.o'],cwd=str(ion),capture_output=True,text=True)
    if r.returncode!=0: sys.exit('ar failed')
    subprocess.run(['ranlib','libwrfio_nf.a'],cwd=str(ion),check=True)
    after=subprocess.run(['ar','t',str(ion/'libwrfio_nf.a')],capture_output=True,text=True,check=True).stdout.split()
    if after!=arches_before: sys.exit('archive member inventory changed')
    unchanged_ok=True
    for m in arches_before:
        if m=='wrf_io.o': continue
        cur=subprocess.run(['ar','p',str(ion/'libwrfio_nf.a'),m],capture_output=True,check=True).stdout
        if cur!=members_before[m]: unchanged_ok=False
    if not unchanged_ok: sys.exit('archive member bytes changed')
    def recheck(p,s):
        if sha(p)!=s: sys.exit('post-build recheck failed: %s'%p)
    for path,digest in input_pins.items(): recheck(path,digest)
    man={'status':'PASS','schema_version':1,'source_input_sha':sha(src),'patch_input_sha':sha(P),'archive_input_sha':sha(orig_arc),'compiler':str(FC),'compiler_version':ver,'compiler_banner':banner,'input_pins':input_pins,'commands':{'cpp':cpp,'compile':cc},'flags':FLAGS,'output_source_sha':sha(ion/'wrf_io.F90'),'output_object_sha':sha(ion/'wrf_io.o'),'output_archive_sha':sha(ion/'libwrfio_nf.a'),'unchanged_members_ok':unchanged_ok,'supported_type':'gnu13.3','scope':'project-only dependency capsule builder; actual new artifact requires oracle before application selection','original_pins_rechecked_after_build':True}
    (O/'build.json').write_text(json.dumps(man,indent=2)+'\n')
    print(json.dumps({'output':str(O),'archive':str(ion/'libwrfio_nf.a'),'archive_sha256':man['output_archive_sha']}))

if __name__=='__main__': main()
