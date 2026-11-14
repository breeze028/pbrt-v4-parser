"""Small GitHub REST fallback for repo/Release/Pages APIs absent from the connector.

Uses the existing Git credential helper in memory; never prints or stores tokens.
All mutations require an explicit HTTP method and caller-supplied JSON/file body.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

def credential():
    env = os.environ.copy()
    env.update(GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never')
    result = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n',
                            text=True, capture_output=True, env=env, timeout=30)
    if result.returncode:
        raise RuntimeError('No noninteractive GitHub credential is available from the Git credential helper')
    fields = dict(line.split('=',1) for line in result.stdout.splitlines() if '=' in line)
    if not fields.get('password'):
        raise RuntimeError('Git credential helper returned no GitHub credential')
    return fields['password']

def request(path, method='GET', body=None, upload=None):
    url = path if path.startswith('https://') else 'https://api.github.com'+path
    if urllib.parse.urlparse(url).hostname not in ('api.github.com','uploads.github.com'):
        raise ValueError('Only official GitHub API hosts are supported')
    headers = {'Authorization':'Bearer '+credential(), 'Accept':'application/vnd.github+json',
               'X-GitHub-Api-Version':'2022-11-28', 'User-Agent':'pbrt-v4-parser-publish'}
    data = None
    if upload:
        data = Path(upload).read_bytes()
        headers['Content-Type'] = 'application/octet-stream'
    elif body is not None:
        data = json.dumps(body).encode('utf-8')
        headers['Content-Type'] = 'application/json'
    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=600 if upload else 60) as response:
            content = response.read()
            return json.loads(content) if content else {'status':response.status}
    except urllib.error.HTTPError as error:
        detail = error.read().decode('utf-8', errors='replace')
        raise RuntimeError(f'GitHub HTTP {error.code}: {detail}') from None

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', help='/repos/owner/repo/... or an official upload URL')
    parser.add_argument('--method', choices=['GET','POST','PUT','PATCH','DELETE'], default='GET')
    parser.add_argument('--body', type=Path, help='UTF-8 JSON body file')
    parser.add_argument('--upload', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--field', action='append', help='Print only these top-level fields')
    args = parser.parse_args()
    body = json.loads(args.body.read_text(encoding='utf-8-sig')) if args.body else None
    result = request(args.path, args.method, body, args.upload)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    if args.field:
        result = {key:result.get(key) for key in args.field}
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
