"""Download small, pinned upstream descriptions and check parsing (not rendering).

Textures, PLY and other rendering assets are deliberately not downloaded.
The killeroo Include is a 287 KB loopsubdiv description. Nothing is committed.
"""
import argparse
import hashlib
import pathlib
import subprocess
import urllib.request

COMMIT = "30cf4a0346ae5a80a2d7a530a3ef7d0fa4f70572"
FILES = ["killeroos/killeroo-simple.pbrt", "killeroos/killeroo-moving.pbrt",
         "killeroos/geometry/killeroo.pbrt", "ganesha/ganesha.pbrt", "head/head.pbrt"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--info", required=True, type=pathlib.Path, help="Built pbrt4_info executable")
    parser.add_argument("--destination", type=pathlib.Path, default=pathlib.Path("out/official-scenes"))
    args = parser.parse_args()
    for filename in FILES:
        url = f"https://raw.githubusercontent.com/mmp/pbrt-v4-scenes/{COMMIT}/{filename}"
        data = urllib.request.urlopen(url, timeout=30).read(2_000_001)
        if len(data) > 2_000_000:
            raise RuntimeError("Unexpected download size: " + filename)
        path = args.destination / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(filename, len(data), hashlib.sha256(data).hexdigest(), flush=True)
    for filename in FILES:
        if "/geometry/" in filename:
            continue
        output = subprocess.run([str(args.info.resolve()), str(args.destination / filename)],
                                check=True, text=True, capture_output=True)
        print(output.stdout, end="")
        print(output.stderr, end="")
        counts = dict(line.split(": ", 1) for line in output.stdout.splitlines() if ": " in line)
        expected = 5 if filename.startswith("killeroos/") else 4 if filename.startswith("ganesha/") else 1
        if counts.get("shapes") != str(expected) or counts.get("diagnostics") != "0":
            raise RuntimeError("Unexpected scene summary: " + filename)
        if filename.endswith("killeroo-moving.pbrt") and counts.get("animated shapes") != "2":
            raise RuntimeError("Animated transform endpoints were not retained")
    print("PASS: four official descriptions, including nested Include and animation.")
    print("Evidence is Scene IR parsing only; external PLY/image decoding and rendering are not tested.")
    print("Upstream asset licenses: https://github.com/mmp/pbrt-v4-scenes/tree/" + COMMIT)


if __name__ == "__main__":
    main()
