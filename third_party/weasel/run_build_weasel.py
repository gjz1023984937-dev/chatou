"""Compatibility wrapper for Chatou's reproducible xmake build."""

from pathlib import Path
import subprocess


repo_root = Path(__file__).resolve().parents[2]
build_script = repo_root / "_build_weasel_xmake.ps1"

if not build_script.is_file():
    raise SystemExit(f"Build script not found: {build_script}")

command = [
    "powershell.exe",
    "-NoProfile",
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    str(build_script),
]
raise SystemExit(subprocess.call(command, cwd=repo_root))
