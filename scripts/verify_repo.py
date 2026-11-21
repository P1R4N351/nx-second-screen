# SPDX-License-Identifier: MIT
"""Verify SPDX MIT headers, accidental local-path/secret leaks, and make test."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CODE_SUFFIXES = (".c", ".h", ".java", ".py")
SKIP_DIRS = {".git", "build", "__pycache__", ".pytest_cache"}
LOCAL_PATH_RE = re.compile(r"/Users/|/home/[a-zA-Z0-9_-]+/|gh[pou]_[A-Za-z0-9]{20,}")


def collect_files(root):
    """Collect up to 512 repository files outside build and git directories."""
    assert root.is_dir(), f"Missing repo root: {root}"
    files = []
    for p in sorted(root.rglob("*")):
        if len(files) >= 512:
            break
        if p.is_file() and not any(part in SKIP_DIRS for part in p.parts):
            files.append(p)
    assert len(files) <= 512, "File limit exceeded"
    return files


def check_spdx(files):
    """Require SPDX-License-Identifier: MIT in every source file header."""
    assert isinstance(files, list), "files must be a list"
    assert len(files) <= 512, "Unexpected file list size"
    bad = []
    for p in files:
        if p.suffix not in CODE_SUFFIXES:
            continue
        head = p.read_text(encoding="utf-8", errors="replace")[:512]
        if "SPDX-License-Identifier: MIT" not in head:
            bad.append(str(p.relative_to(ROOT)))
    return bad


def check_paths_and_tokens(files):
    """Scan repository files for local machine paths or embedded auth tokens."""
    assert isinstance(files, list), "files must be a list"
    assert len(files) <= 512, "Unexpected file list size"
    hits = []
    for p in files:
        if p.name == "verify_repo.py":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        for idx, line in enumerate(text.splitlines()[:4000], start=1):
            if LOCAL_PATH_RE.search(line):
                hits.append(f"{p.relative_to(ROOT)}:{idx}")
    return hits


def main():
    """Run license check, path/token scan, and make test."""
    assert ROOT.is_dir(), f"Invalid repository root: {ROOT}"
    files = collect_files(ROOT)
    assert len(files) > 0, "No repository files found"
    bad_spdx = check_spdx(files)
    if bad_spdx:
        print("Missing MIT SPDX header:", bad_spdx, file=sys.stderr)
        return 1
    leaks = check_paths_and_tokens(files)
    if leaks:
        print("Path/token scan hit:", leaks, file=sys.stderr)
        return 1
    rc = subprocess.run(["make", "-C", str(ROOT), "test"], check=False).returncode
    if rc != 0:
        print("make test failed with code", rc, file=sys.stderr)
        return rc
    print(f"verify_repo: ok ({len(files)} files checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
