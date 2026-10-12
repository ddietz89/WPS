"""Acquire the production six-hourly 0--144h GFS inputs into a new directory."""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import datetime, fcntl, hashlib, json, subprocess, sys, time, urllib.request

cycle = sys.argv[1]
start = datetime.datetime.strptime(cycle, '%Y%m%d%H')
assert start.hour in (0, 6, 12, 18)
root = Path(sys.argv[2]); root.mkdir(parents=True, exist_ok=True)
lock=(root/'download.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX)
base = f'https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs.{cycle[:8]}/{cycle[8:]}/atmos/'
receipt = {'cycle': cycle, 'status': 'downloading', 'files': [], 'start_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
def save():
    tmp=root/'download.json.tmp';tmp.write_text(json.dumps(receipt,indent=2)+'\n');tmp.replace(root/'download.json')
def get(hour):
    name = f'gfs.t{cycle[8:]}z.pgrb2.0p25.f{hour:03d}'
    target = root / name
    with urllib.request.urlopen(urllib.request.Request(base+name, method='HEAD'), timeout=60) as response:
        expected = int(response.headers['Content-Length']); etag = response.headers.get('ETag')
    if not target.exists():
        part = root / (name+'.part')
        subprocess.run(['curl','--fail','--location','--retry','4','--connect-timeout','30','--max-time','1800','--silent','--show-error','--continue-at','-','--output',str(part),base+name], check=True)
        assert part.stat().st_size == expected
        with part.open('rb') as f:
            assert f.read(4) == b'GRIB'; f.seek(-4,2); assert f.read() == b'7777'
        part.rename(target)
    assert target.stat().st_size == expected
    def identity():
        s=target.stat();return {'device':s.st_dev,'inode':s.st_ino,'bytes':s.st_size,'mtime_ns':s.st_mtime_ns,'ctime_ns':s.st_ctime_ns}
    before=identity()
    with target.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    assert identity()==before,'Input changed during checksum'
    return {'hour':hour, 'file':str(target),'bytes':expected,'sha256':digest,'verified_stat':before,'etag':etag,'url':base+name}
begin = time.monotonic()
with ThreadPoolExecutor(max_workers=12) as pool:
    for future in as_completed([pool.submit(get,h) for h in range(0,145,6)]):
        row=future.result();receipt['files'].append(row)
        save()
        print('downloaded',row['hour'],row['bytes'],flush=True)
receipt['files'].sort(key=lambda r:r['hour'])
assert [r['hour'] for r in receipt['files']] == list(range(0,145,6))
receipt.update(status='PASS',wall_seconds=time.monotonic()-begin,bytes=sum(r['bytes'] for r in receipt['files']))
save()
print('DOWNLOAD COMPLETE', receipt['bytes'],flush=True)
