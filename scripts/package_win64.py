#!/usr/bin/env python3
"""Build the Windows x64 distribution zip for Project Seven.

The VST3 bundle plus its documentation is packaged with forward-slash entry
names.  Windows' Compress-Archive writes backslashes as path separators, which
is not what the zip specification says and which some extractors mangle, so the
archive is written here instead.

Usage:  python scripts/package_win64.py [--bundle <path to .vst3>]
Output: ProjectSeven-<version>-win64.zip in the project root.
"""

import os
import re
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOC_FILES = [
    "README.md",
    "ARCHITECTURE.md",
    "BUILD.md",
    "CHANGELOG.md",
    "TESTING.md",
    "PROGRESS.md",
    "PHYSICAL_ENGINE_ARCHITECTURE.md",
    "PHYSICAL_COUPLING_SPECIFICATION.md",
    "PHYSICAL_MODEL_VALIDATION.md",
    "PHYSICS_ENGINE_REHAUL_ROADMAP.md",
]
ASSET_FILES = ["editor_preview.png"]

DEFAULT_BUNDLE = os.path.join(
    "build", "ProjectSeven_artefacts", "Release", "VST3", "Project Seven.vst3")


def project_version():
    text = open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read()
    match = re.search(r"project\s*\(\s*ProjectSeven\s+VERSION\s+([0-9.]+)", text)
    if not match:
        raise SystemExit("could not read the project version from CMakeLists.txt")
    return match.group(1)


def add_tree(archive, directory, arc_prefix):
    for base, _dirs, files in os.walk(directory):
        for name in files:
            path = os.path.join(base, name)
            relative = os.path.relpath(path, directory)
            arcname = os.path.join(arc_prefix, relative).replace(os.sep, "/")
            archive.write(path, arcname)


def main(argv):
    bundle = os.path.join(ROOT, DEFAULT_BUNDLE)
    if "--bundle" in argv:
        bundle = os.path.abspath(argv[argv.index("--bundle") + 1])

    if not os.path.isdir(bundle):
        raise SystemExit("VST3 bundle not found: %s\nBuild ProjectSeven_VST3 first." % bundle)

    binary = os.path.join(bundle, "Contents", "x86_64-win", os.path.basename(bundle))
    if not os.path.isfile(binary):
        raise SystemExit("bundle binary missing: %s" % binary)

    version = project_version()
    target = os.path.join(ROOT, "ProjectSeven-%s-win64.zip" % version)

    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        add_tree(archive, bundle, os.path.basename(bundle))
        for name in DOC_FILES:
            path = os.path.join(ROOT, "docs", name)
            if not os.path.isfile(path):
                raise SystemExit("missing document: %s" % path)
            archive.write(path, name)
        for name in ASSET_FILES:
            path = os.path.join(ROOT, "docs", name)
            if os.path.isfile(path):
                archive.write(path, name)

    size = os.path.getsize(target)
    with zipfile.ZipFile(target) as archive:
        names = archive.namelist()
    print("wrote %s (%.1f MB, %d entries)" % (os.path.basename(target), size / 1e6, len(names)))
    for name in sorted(names):
        print("  " + name)


if __name__ == "__main__":
    main(sys.argv[1:])
