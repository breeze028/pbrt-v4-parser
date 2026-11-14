"""Build or fetch pinned original PBRT scene assets. No .lt/.ivol files are used."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

def digest(data):
    return hashlib.sha256(data).hexdigest()

def safe_path(name):
    path = PurePosixPath(name)
    if not name or '\\' in name or path.is_absolute() or any(p in ('', '.', '..') for p in name.split('/')):
        raise ValueError('Unsafe archive path: '+name)
    if path.suffix.lower() not in ('.pbrt', '.ply', '.tga', '.txt'):
        raise ValueError('Unexpected scene resource: '+name)
    return path

def build(args):
    args.output.mkdir(parents=True, exist_ok=True)
    scenes = []
    for name in ('spaceship', 'bathroom'):
        bundle = ROOT/'web/public/scenes'/name
        manifest = json.loads((bundle/'manifest.json').read_text(encoding='utf-8'))
        packed = (bundle/'resources.bin').read_bytes()
        if len(packed) != manifest['bytes'] or manifest['root'] != 'scene-v4.pbrt':
            raise ValueError('Invalid prepared bundle: '+name)
        files = {f['path']: f for f in manifest['files']}
        for required in ('scene-v4.pbrt', 'LICENSE.txt'):
            if required not in files:
                raise ValueError('Missing required file '+required)
        archive = args.output/(name+'-original-v1.tar.gz')
        with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0, compresslevel=9) as gz:
            with tarfile.open(fileobj=gz, mode='w', format=tarfile.USTAR_FORMAT) as tar:
                def add(path, data):
                    info = tarfile.TarInfo(path)
                    info.size, info.mode, info.mtime = len(data), 0o644, 0
                    tar.addfile(info, io.BytesIO(data))
                add('bundle-manifest.json', json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
                for path, f in sorted(files.items()):
                    safe_path(path)
                    data = packed[f['offset']:f['offset']+f['size']]
                    if len(data) != f['size'] or digest(data) != f['sha256']:
                        raise ValueError('Prepared bundle hash mismatch: '+path)
                    add(path, data)
        data = archive.read_bytes()
        scenes.append({'name':name, 'archive':archive.name,
                       'url':f'https://github.com/{args.repo}/releases/download/{args.tag}/{archive.name}',
                       'sha256':digest(data), 'bytes':len(data), 'files':len(files),
                       'root_sha256':files['scene-v4.pbrt']['sha256'], 'license_sha256':files['LICENSE.txt']['sha256']})
        print(f'{archive}: {len(data):,} bytes sha256={digest(data)}')
    lock = {'version':1, 'tag':args.tag, 'repository':args.repo, 'scenes':scenes}
    args.lock.parent.mkdir(parents=True, exist_ok=True)
    args.lock.write_text(json.dumps(lock, indent=2)+'\n', encoding='utf-8')

def verify_archive(archive, entry, destination):
    if archive.stat().st_size != entry['bytes'] or digest(archive.read_bytes()) != entry['sha256']:
        raise ValueError('Archive checksum/length mismatch: '+archive.name)
    destination = destination.resolve()
    with tarfile.open(archive, 'r:gz') as tar:
        members = tar.getmembers()
        if len(members) > 3000 or sum(m.size for m in members) > 512*1024*1024:
            raise ValueError('Oversized archive')
        if len({m.name for m in members}) != len(members) or any(not m.isfile() for m in members):
            raise ValueError('Nonregular or duplicate archive entry')
        manifest = json.loads(tar.extractfile('bundle-manifest.json').read())
        files = {f['path']:f for f in manifest['files']}
        if set(files) != {m.name for m in members}-{'bundle-manifest.json'} or len(files) != entry['files']:
            raise ValueError('Archive file manifest mismatch')
        if files['scene-v4.pbrt']['sha256'] != entry['root_sha256'] or files['LICENSE.txt']['sha256'] != entry['license_sha256']:
            raise ValueError('Scene root/license lock mismatch')
        for name, f in files.items():
            parts = safe_path(name).parts
            target = destination.joinpath(*parts).resolve()
            if not target.is_relative_to(destination):
                raise ValueError('Extraction escapes scene directory')
            member = tar.getmember(name)
            if member.size != f['size'] or member.size > 256*1024*1024:
                raise ValueError('Resource length mismatch: '+name)
            data = tar.extractfile(member).read()
            if digest(data) != f['sha256']:
                raise ValueError('Resource hash mismatch: '+name)
            if target.exists():
                if not target.is_file() or digest(target.read_bytes()) != f['sha256']:
                    raise ValueError('Refusing to overwrite modified scene resource: '+str(target))
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
    print(f'PASS {entry["name"]}: {len(files)} original files + LICENSE verified in {destination}')

def fetch(args):
    lock = json.loads(args.lock.read_text(encoding='utf-8'))
    if lock['version'] != 1:
        raise ValueError('Unsupported asset lock version')
    args.cache.mkdir(parents=True, exist_ok=True)
    for entry in lock['scenes']:
        if entry['name'] not in ('spaceship', 'bathroom'):
            raise ValueError('Unknown scene')
        archive = (args.local/entry['archive']) if args.local else args.cache/entry['archive']
        if not args.local and (not archive.exists() or digest(archive.read_bytes()) != entry['sha256']):
            request = urllib.request.Request(entry['url'], headers={'User-Agent':'pbrt-v4-parser-assets'})
            part = archive.with_suffix(archive.suffix+'.part')
            total = 0
            with urllib.request.urlopen(request, timeout=60) as response, part.open('wb') as output:
                while block := response.read(1024*1024):
                    total += len(block)
                    if total > entry['bytes']:
                        raise ValueError('Download exceeds locked size')
                    output.write(block)
            if total != entry['bytes'] or digest(part.read_bytes()) != entry['sha256']:
                raise ValueError('Download checksum/length mismatch')
            part.replace(archive)
        verify_archive(archive, entry, args.destination/entry['name'])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    b = sub.add_parser('build')
    b.add_argument('--repo', default='breeze028/pbrt-v4-parser')
    b.add_argument('--tag', default='scenes-v1')
    b.add_argument('--output', type=Path, default=ROOT/'out/scene-assets')
    b.add_argument('--lock', type=Path, default=ROOT/'web/assets.lock.json')
    f = sub.add_parser('fetch')
    f.add_argument('--lock', type=Path, default=ROOT/'web/assets.lock.json')
    f.add_argument('--destination', type=Path, default=ROOT/'scenes')
    f.add_argument('--cache', type=Path, default=ROOT/'.reference/scene-downloads')
    f.add_argument('--local', type=Path, help='Verify local archives without network access')
    args = parser.parse_args()
    build(args) if args.command == 'build' else fetch(args)

if __name__ == '__main__':
    main()
