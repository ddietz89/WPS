"""Download and run WPS only, using unchanged production configuration snapshots."""
from pathlib import Path
import argparse, datetime, fcntl, hashlib, json, os, re, resource, shutil, socket, subprocess, tempfile, time

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(path, report):
    tmp=path.with_suffix('.json.tmp');tmp.write_text(json.dumps(report,indent=2)+'\n');tmp.replace(path)
def get_env(script, base):
    return json.loads(subprocess.check_output(['bash','-c','source "$1"; /usr/bin/python3 -c "import os,json;print(json.dumps(dict(os.environ)))"','env',str(script)],env=base))
def main():
    resource.setrlimit(resource.RLIMIT_STACK,(-1,-1))
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cycle');p.add_argument('--mode',choices=['control47','new'],default='new')
    p.add_argument('--root',type=Path);p.add_argument('--skip-download',action='store_true')
    a=p.parse_args();start=datetime.datetime.strptime(a.cycle,'%Y%m%d%H');assert start.hour in (0,6,12,18)
    end=start+datetime.timedelta(hours=144)
    root=a.root or Path('/pool/wrf/WPS_new_runs')/a.cycle
    root=root.resolve()
    root.relative_to(Path('/pool/wrf/WPS_new_runs').resolve())
    root.mkdir(parents=True,exist_ok=True)
    lock=(root/'setup.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX)
    assert not (root/a.mode).exists(),'Refuse to replace an existing run; choose a new root'
    inputs=root/'input'
    if not a.skip_download:
        subprocess.run(['/usr/bin/python3',str(Path(__file__).with_name('download.py')),a.cycle,str(inputs)],check=True)
    acquisition=json.loads((inputs/'download.json').read_text());assert acquisition['status']=='PASS' and acquisition['cycle']==a.cycle
    assert [r['hour'] for r in acquisition['files']]==list(range(0,145,6))
    for row in acquisition['files']:
        file=inputs/Path(row['file']).name;s=file.stat()
        current={'device':s.st_dev,'inode':s.st_ino,'bytes':s.st_size,'mtime_ns':s.st_mtime_ns,'ctime_ns':s.st_ctime_ns}
        assert s.st_size==row['bytes']
        if current!=row.get('verified_stat'):assert sha(file)==row['sha256']
    original=Path('/pool/wrf/WPS');runtime=Path('/pool/wrf/WPS_new');build=Path('/pool/wrf-experiment/builds/m2/gnu/WPS') if a.mode=='control47' else runtime
    d=root/a.mode;d.mkdir();(d/'outputs').mkdir();(d/'config').mkdir()
    sources={'namelist.template':original/'namelist.template','Vtable':original/'Vtable','GEOGRID.TBL':original/'geogrid/GEOGRID.TBL','METGRID.TBL':original/'metgrid/METGRID.TBL','runwrf_scratch.sh':Path('/pool/wrf/runwrf_scratch.sh')}
    original_pins={str(f):sha(f) for f in sources.values()}
    for name,f in sources.items():shutil.copy2(f,d/'config'/name)
    for stage in ['geogrid','ungrib','metgrid']:original_pins[str(original/(stage+'.exe'))]=sha(original/(stage+'.exe'))
    text=(d/'config/namelist.template').read_text()
    assert text.count('START_DATE')==text.count('END_DATE')==1
    text=text.replace('START_DATE',start.strftime('%Y-%m-%d_%H:%M:%S')).replace('END_DATE',end.strftime('%Y-%m-%d_%H:%M:%S'))
    (d/'namelist.wps').write_text(text)
    for stage,table in [('geogrid','GEOGRID.TBL'),('metgrid','METGRID.TBL')]:
        (d/stage).mkdir();(d/stage/table).symlink_to(d/'config'/table)
    (d/'Vtable').symlink_to(d/'config/Vtable')
    for i,row in enumerate(acquisition['files']):
        # The production input cadence needs only AAA through AAY.
        (d/('GRIBFILE.AA'+chr(ord('A')+i))).symlink_to(inputs/Path(row['file']).name)
    base={k:v for k,v in os.environ.items() if not k.startswith(('OMPI_','PMIX_','UCX_','WPS_','I_MPI_')) and k not in ['LD_PRELOAD','LD_LIBRARY_PATH','OPAL_PREFIX']}
    base.update(PATH='/usr/bin:/bin',WRFIO_NCD_LARGE_FILE_SUPPORT='1',OMP_DYNAMIC='FALSE')
    env=get_env(runtime/'env.sh',base)
    if a.mode=='new':
        pins={'geogrid':'2e374c0ffc65de9890ffafc2b2b91d5b1dd422a2f3a44ab8b82329072d867d66','ungrib':'c9ff28624990f1b33f224387c831b4bbe195fa79f83d69eaaecb850cab0c30b0','metgrid':'db1ebea6d213b195e7404957ea7e79e136799f59a04b6e34d93696fd6a87c622'}
    else:
        pins={'geogrid':'45fdb74e7b65484da86e12491c2f111b0011cc6fdcc8f8665074eec065f250e9','ungrib':'61464195f90fc95d83a0cd3a73f0df635c8d48ac5499a6e728fb7dd9372b93e2','metgrid':'46fd4e8bfbcc1a440bb199cd0e62567adeed81c66747b86784442fe713c9d7f8'}
        for key in ['WPS_PRECOMPRESSED_NETCDF_PREFIX','WPS_ZLIB_NG_LIBRARY']:env.pop(key,None)
        shutil.copy2('/pool/wrf-experiment/results/m2/gnu/build-manifest.json',d/'control-build-manifest.json')
    for stage,digest in pins.items():assert sha(build/(stage+'.exe'))==digest
    session=Path(tempfile.mkdtemp(prefix='wps-setup-',dir='/tmp'))
    env.update(TMPDIR=str(session),OMPI_MCA_orte_tmpdir_base=str(session))
    cg=Path('/sys/fs/cgroup'+Path('/proc/self/cgroup').read_text().strip().split('::')[-1])
    def cpu():return int(next(x.split()[1] for x in (cg/'cpu.stat').read_text().splitlines() if x.startswith('usage_usec ')))
    cores=sorted(os.sched_getaffinity(0));assert len(cores)==16,'Run inside the supplied 16-core service'
    report={'status':'running','cycle':a.cycle,'mode':a.mode,'host':socket.gethostname(),'directory':str(d),'original_pins':original_pins,'input_receipt_sha256':sha(inputs/'download.json'),'configuration':'Production template, tables and Vtable; only date placeholders rendered. 0--144h every6h. No REAL/WRF/postprocessing.','stages':[],'resources':{'cores':cores,'cgroup':str(cg),'cpu_max':(cg/'cpu.max').read_text().strip(),'memory_max':(cg/'memory.max').read_text().strip(),'memory_swap_max':(cg/'memory.swap.max').read_text().strip()},'timing_scope':'Sequential geogrid, full-range ungrib, full-range metgrid; configuration and input checks outside clocks; Unmodified GNU WPS4.7 versus optimized GNU WPS4.7; optimized run completed first. Shared-load variation remains.'}
    target=d/'execution.json';save(target,report);begin=time.monotonic();initial_cpu=cpu()
    try:
        for stage in ['geogrid','ungrib','metgrid']:
            ee=env.copy();ee['OMP_NUM_THREADS']='4' if a.mode=='new' and stage in ['ungrib','metgrid'] else '1'
            ee.update(OMP_PLACES='cores',OMP_PROC_BIND='close')
            exe=build/(stage+'.exe')
            if a.mode=='new' and stage=='metgrid':
                ee=json.loads(subprocess.check_output([str(build/'tools/metgrid'),'--','/usr/bin/python3','-c','import os,json;print(json.dumps(dict(os.environ)))'],env=ee))
                (d/'provider').mkdir();ee.update(WPS_CHUNK_PROOF_DIR=str(d/'provider'),WPS_CHUNK_PROOF_RUN_ID=f'production-{a.cycle}-new')
            closure=subprocess.check_output(['ldd',str(exe)],env=ee,text=True);(d/(stage+'-ldd.txt')).write_text(closure);assert 'not found' not in closure,closure
            if stage!='ungrib':
                assert str(runtime/'runtime/mpi/lib/libmpi.so.40') in closure
            if a.mode=='new':assert '/pool/wrf-experiment/' not in closure
            cmd=[str(exe)];ranks=1
            if stage!='ungrib':
                ranks=1 if stage=='geogrid' else (16 if a.mode=='control47' else 4)
                rankfile=d/(stage+'.rankfile');width=4 if a.mode=='new' and stage=='metgrid' else 1
                assert cores==list(range(32,48))
                rankfile.write_text(''.join(f'rank {i}={socket.gethostname()} slot=0:{32+width*i}-{31+width*(i+1)}\n' for i in range(ranks)))
                cmd=[str(Path(ee['OPAL_PREFIX'])/'bin/mpirun'),'--prefix',ee['OPAL_PREFIX'],'--host',socket.gethostname()+':'+str(ranks),'-np',str(ranks),'--rankfile',str(rankfile),'--bind-to','core','--report-bindings']
                for k in ['LD_LIBRARY_PATH','LD_PRELOAD','OMP_NUM_THREADS','OMP_PLACES','OMP_PROC_BIND','WPS_CHUNK_PROOF_DIR','WPS_CHUNK_PROOF_RUN_ID']:
                    if k in ee:cmd+=['-x',k]
                cmd+=[str(exe)]
            before=cpu();t=time.monotonic();proof={}
            print('START',a.mode,stage,flush=True)
            with (d/(stage+'-stdout.log')).open('w') as log:
                child=subprocess.Popen(['/usr/bin/time','-v','-o',str(d/(stage+'-time.txt')),'timeout','--kill-after=30','3600',*cmd],cwd=d,env=ee,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT)
                while child.poll() is None:
                    for proc in Path('/proc').iterdir():
                        if not proc.name.isdigit():continue
                        try:
                            if (proc/'exe').resolve()!=exe.resolve() or (proc/'cwd').resolve()!=d:continue
                            assert (proc/'cgroup').read_text()==Path('/proc/self/cgroup').read_text()
                            paths=sorted(set(x.split()[-1] for x in (proc/'maps').read_text().splitlines() if any(y in x for y in ['libmpi.so','libnetcdf.so','libz.so','libucp.so','libuct.so'])))
                            threads={t.name:sorted(os.sched_getaffinity(int(t.name))) for t in (proc/'task').iterdir()}
                            assert all(set(aff)<=set(cores+[c+128 for c in cores]) for aff in threads.values())
                            variables=dict(x.split(b'=',1) for x in (proc/'environ').read_bytes().split(b'\0') if b'=' in x)
                            rank=int(variables.get(b'OMPI_COMM_WORLD_RANK',variables.get(b'PMI_RANK',b'0')))
                            if stage!='ungrib':
                                if str((runtime/'runtime/mpi/lib/libmpi.so.40').resolve()) not in paths:continue
                                owned=list(range(32+rank*width,32+(rank+1)*width))
                                assert all(set(aff)<=set(owned+[c+128 for c in owned]) for aff in threads.values())
                            if a.mode=='new' and stage=='metgrid':
                                if str((build/'runtime/netcdf-owner/lib/libnetcdf.so.19').resolve()) not in paths or str(build/'runtime/zlib-ng/lib/libz.so.1.3.1.zlib-ng') not in paths:continue
                            old=proof.get(proc.name)
                            if old is None or len(threads)>len(old['threads']):proof[proc.name]={'rank':rank,'cgroup':(proc/'cgroup').read_text().strip(),'threads':threads,'libraries':paths}
                        except (OSError,ValueError):pass
                    time.sleep(.5)
            row={'stage':stage,'wall_seconds':time.monotonic()-t,'cpu_seconds_cgroup':(cpu()-before)/1e6,'exit_code':child.returncode,'executable_sha256':sha(exe),'command':cmd,'live_processes':proof}
            report['stages'].append(row);save(target,report)
            assert child.returncode==0,(stage,child.returncode)
            assert len(proof)==ranks,(stage,len(proof),ranks)
            if a.mode=='new' and stage in ['ungrib','metgrid']:assert all(len(v['threads'])>=4 for v in proof.values())
            print('DONE',a.mode,stage,row['wall_seconds'],flush=True)
        products=sorted([*d.glob('geo_em*.nc'),*d.glob('FILE:*'),*d.glob('met_em*.nc')],key=lambda x:x.name)
        assert len(list(d.glob('geo_em*.nc')))==1 and len(list(d.glob('FILE:*')))==25 and len(list(d.glob('met_em*.nc')))==25
        dates=[start+datetime.timedelta(hours=h) for h in range(0,145,6)]
        expected={'geo_em.d01.nc'}|{'FILE:'+t.strftime('%Y-%m-%d_%H') for t in dates}|{'met_em.d01.'+t.strftime('%Y-%m-%d_%H:%M:%S')+'.nc' for t in dates}
        assert {f.name for f in products}==expected
        report.update(wall_seconds=time.monotonic()-begin,cpu_seconds_cgroup=(cpu()-initial_cpu)/1e6,status='EXECUTED_PENDING_COMPARISON')
        for file in products:shutil.move(str(file),d/'outputs'/file.name)
        report['products']=[{'file':str(f),'bytes':f.stat().st_size} for f in sorted((d/'outputs').iterdir())]
        for f,h in original_pins.items():assert sha(f)==h,'Existing production file changed: '+f
        report['original_files_unchanged']=True;save(target,report)
    except BaseException:
        import traceback
        report.update(status='FAIL',traceback=traceback.format_exc());save(target,report);raise
    finally:shutil.rmtree(session)
    print('WPS SETUP COMPLETE',a.mode,report['wall_seconds'],flush=True)

if __name__=='__main__':main()
