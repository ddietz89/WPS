#!/usr/bin/env python3
"""Build the qualified NetCDF owner extension without editing retained sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ORIGINALS = {'libhdf5/hdf5var.c': '6c5dc9b7e0d8f2bbac0dcc91d1a5aae97a0971b9f837f4576203d8107bf24c28', 'libdispatch/nc.c': '36b61d75e462163eb5ef8f0de4a1e4af5314aeefb56d2e4a2a4c24f0542f36cc', 'libdispatch/dfile.c': '0f67a99f1c6de5edd1dee5570f6866a82543cf8c22425e985e907d06039c9142'}
PATCH_SHA = 'c14342f147595d734758a92c3c7acad9983760472dc9776cea0480250b219d49'
HEADER_SHA = 'f050797799f235dea8be9400198d0b8d44fbfca2acb45ed4a914b75b717311b4'

def sha(path):
    with Path(path).open('rb') as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

def cache_values(path):
    values = {}
    for line in path.read_text().splitlines():
        if line.startswith(('#', '//')) or '=' not in line or ':' not in line.split('=', 1)[0]:
            continue
        name_type, value = line.split('=', 1)
        name, kind = name_type.split(':', 1)
        values[name] = (kind, value)
    return values

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path, help='Unmodified NetCDF-C 4.9.2 source')
    parser.add_argument('--reference-cache', required=True, type=Path, help='Qualified original CMakeCache.txt')
    parser.add_argument('--dependency-prefix', required=True, type=Path, help='Retained serial HDF5 1.14.3 and zlib prefix')
    parser.add_argument('--prefix', required=True, type=Path, help='New empty install prefix')
    parser.add_argument('--build-root', required=True, type=Path, help='New work directory (source copy/build/log/receipt)')
    parser.add_argument('--cmake', default='cmake', help='CMake executable')
    parser.add_argument('--compiler', help='Optional explicit C compiler, otherwise original cache compiler')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    require(platform.system() == 'Linux', 'Qualified provider scope requires Linux main-thread detection')
    require(args.jobs > 0, '--jobs must be positive')
    source, deps, install, work = (p.resolve() for p in (args.source, args.dependency_prefix, args.prefix, args.build_root))
    require(source.is_dir() and deps.is_dir(), 'Source/dependency prefix must exist')
    require(not work.exists() and not install.exists(), 'Build root and install prefix must both be new')
    for destination in (work, install):
        require(not destination.is_relative_to(source) and not source.is_relative_to(destination), 'Source and destination paths must be disjoint')
        require(not destination.is_relative_to(deps) and not deps.is_relative_to(destination), 'Dependencies and destination paths must be disjoint')
    require(not work.is_relative_to(install) and not install.is_relative_to(work), 'Build and install prefixes must be disjoint')
    patch, header = HERE / 'netcdf-c-4.9.2.patch', HERE / 'netcdf_precompressed.h'
    require(sha(patch) == PATCH_SHA and sha(header) == HEADER_SHA, 'Package patch/header identity mismatch')
    protected = {str(source / name): sha(source / name) for name in ORIGINALS}
    for name, expected in ORIGINALS.items():
        require(protected[str(source / name)] == expected, 'Unsupported source: ' + name)
    values = cache_values(args.reference_cache.resolve())
    require(values.get('ENABLE_NETCDF_4', ('', ''))[1] == 'ON', 'Reference must enable NetCDF4')
    require(values.get('ENABLE_PARALLEL4', ('', 'OFF'))[1] != 'ON', 'Parallel HDF5 is outside qualified scope')
    for name in ('libhdf5.so', 'libhdf5_hl.so', 'libz.so'):
        require((deps / 'lib' / name).exists(), 'Missing retained dependency ' + name)
    keys = ['BUILD_SHARED_LIBS', 'CMAKE_BUILD_TYPE', 'CMAKE_C_COMPILER', 'CMAKE_C_FLAGS', 'CMAKE_C_FLAGS_RELEASE']
    keys += sorted(k for k, (kind, _) in values.items() if k.startswith('ENABLE_') and kind in ('BOOL', 'STRING'))
    options = {k: values[k] for k in keys if k in values}
    require(options.get('BUILD_SHARED_LIBS', ('', ''))[1] == 'ON', 'Reference must build shared NetCDF')
    for name in ('CMAKE_C_FLAGS', 'CMAKE_C_FLAGS_RELEASE'):
        require('-ffast-math' not in options.get(name, ('', ''))[1] and '-Ofast' not in options.get(name, ('', ''))[1], 'Fast-math is outside qualified flags')
    if args.compiler:
        options['CMAKE_C_COMPILER'] = ('FILEPATH', str(Path(args.compiler).resolve()))
    options.update({
        'CMAKE_INSTALL_PREFIX': ('PATH', str(install)),
        'CMAKE_INSTALL_RPATH': ('STRING', str(deps / 'lib')),
        'HDF5_ROOT': ('PATH', str(deps)),
        'HDF5_C_LIBRARY': ('FILEPATH', str(deps / 'lib/libhdf5.so')),
        'HDF5_HL_LIBRARY': ('FILEPATH', str(deps / 'lib/libhdf5_hl.so')),
        'HDF5_INCLUDE_DIR': ('PATH', str(deps / 'include')),
        'ZLIB_ROOT': ('PATH', str(deps)),
        'ZLIB_INCLUDE_DIR': ('PATH', str(deps / 'include')),
        'ZLIB_LIBRARY': ('FILEPATH', str(deps / 'lib/libz.so')),
        'CMAKE_EXPORT_COMPILE_COMMANDS': ('BOOL', 'ON'),
    })
    before = {**protected, str(args.reference_cache.resolve()): sha(args.reference_cache.resolve()), **{str(deps / 'lib' / n): sha(deps / 'lib' / n) for n in ('libhdf5.so', 'libhdf5_hl.so', 'libz.so')}}
    work.mkdir(parents=True)
    copied, build = work / 'source', work / 'build'
    shutil.copytree(source, copied, symlinks=True, ignore=shutil.ignore_patterns('.git'))
    for name in ORIGINALS:
        require(not (copied / name).is_symlink(), 'Patched source must be regular: ' + name)
    with patch.open('rb') as stream:
        subprocess.run(['patch', '--batch', '--fuzz=0', '-p1'], cwd=copied, stdin=stream, check=True)
    shutil.copy2(header, copied / 'include/netcdf_precompressed.h')
    commands = [[args.cmake, '-S', str(copied), '-B', str(build), *['-D' + k + ':' + kind + '=' + value for k, (kind, value) in options.items()]], [args.cmake, '--build', str(build), '--parallel', str(args.jobs)], [args.cmake, '--install', str(build)]]
    env = dict(os.environ)
    env.pop('LD_PRELOAD', None)
    env['LD_LIBRARY_PATH'] = str(deps / 'lib') + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    started = time.monotonic()
    try:
        with (work / 'build.log').open('w') as log:
            for command in commands:
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        shutil.copy2(header, install / 'include/netcdf_precompressed.h')
        lib = install / 'lib/libnetcdf.so.19'
        require(lib.exists(), 'Expected libnetcdf.so.19 missing')
        dynamic = subprocess.check_output(['readelf', '-d', str(lib)], text=True)
        require('Library soname: [libnetcdf.so.19]' in dynamic, 'Unexpected SONAME')
        symbols = subprocess.check_output(['nm', '-D', str(lib)], text=True)
        require(all(' ' + symbol in symbols for symbol in ('nc_precompressed_provider_begin', 'nc_precompressed_provider_end')), 'Provider API symbols missing')
        require(all(sha(p) == digest for p, digest in before.items()), 'Retained artifact changed during build')
        receipt = {'status': 'BUILD_PASS_PENDING_DEPLOYMENT_QUALIFICATION', 'patch_sha256': PATCH_SHA, 'header_sha256': HEADER_SHA, 'library': str(lib.resolve()), 'library_sha256': sha(lib), 'original_artifacts_before_after': before, 'commands': commands, 'elapsed_seconds': time.monotonic() - started, 'package_build_script_sha256': sha(__file__)}
        (work / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))
    except BaseException as error:
        (work / 'failure.json').write_text(json.dumps({'status': 'BUILD_FAILED', 'error': str(error)}) + '\n')
        raise

if __name__ == '__main__':
    main()
