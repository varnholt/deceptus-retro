"""Install a project-local SNES SDK and optional emulator core, without global setup."""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "4.7.0"
RELEASE = "470"


def download(url, target):
    print(f"downloading {url}", flush=True)
    urllib.request.urlretrieve(url, target)
    return hashlib.sha256(target.read_bytes()).hexdigest()


def extract(archive, destination):
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as zipped:
        for entry in zipped.infolist():
            target = (destination / entry.filename).resolve()
            if not target.is_relative_to(destination):
                raise ValueError("archive path escapes the tools folder")
        zipped.extractall(destination)
        if platform.system() != "Windows":
            for entry in zipped.infolist():
                permissions = entry.external_attr >> 16
                if permissions:
                    (destination / entry.filename).chmod(permissions)


def bootstrap(emulator=False):
    folder = ROOT / ".tools"
    folder.mkdir(exist_ok=True)
    operating_system = {"Windows": "windows", "Linux": "linux", "Darwin": "darwin"}[platform.system()]
    filename = f"pvsneslib_{RELEASE}_64b_{operating_system}_release.zip"
    archive = folder / filename
    url = f"https://github.com/alekmaul/pvsneslib/releases/download/{VERSION}/{filename}"
    checksum = download(url, archive)
    extract(archive, folder)
    manifest = {"pvsneslib": {"version": VERSION, "url": url, "sha256": checksum}}
    if emulator:
        if operating_system != "windows" or platform.machine().lower() not in ("amd64", "x86_64"):
            raise SystemExit("SDK installed; provide a native Snes9x libretro core with --core on this platform")
        url = "https://buildbot.libretro.com/nightly/windows/x86_64/latest/snes9x_libretro.dll.zip"
        archive = folder / "snes9x_libretro.zip"
        checksum = download(url, archive)
        extract(archive, folder / "snes9x")
        manifest["snes9x"] = {"url": url, "sha256": checksum, "channel": "nightly; hash records the fetched version"}
    (folder / "downloads.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("project-local tools installed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", action="store_true", help="also fetch Windows x64 Snes9x libretro")
    args = parser.parse_args()
    bootstrap(args.emulator)
