#!/usr/bin/env python3
"""Apply the tested video profile to QGroundControl Daily on Linux."""

import argparse
import configparser
import datetime
import difflib
import os
from pathlib import Path
import re
import shutil
import tempfile


def main():
    settings_root = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config", type=Path,
        default=settings_root / "QGroundControl" / "QGroundControl Daily.ini",
        help="Existing QGC INI file; default: QGroundControl Daily on Linux",
    )
    parser.add_argument("--dry-run", action="store_true", help="Show changes without writing")
    args = parser.parse_args()
    target = args.config.expanduser().resolve()
    if not target.is_file():
        parser.error(f"Config does not exist: {target}. Run QGC once, then close it.")

    profile = configparser.ConfigParser(interpolation=None)
    profile.optionxform = str
    profile.read(Path(__file__).resolve().parent / "profile" / "video.ini")
    original = target.read_bytes()
    text = original.decode("utf-8")
    newline = "\r\n" if "\r\n" in text else "\n"
    text = text.replace("\r\n", "\n")
    match = re.search(r"(?ms)^\[Video\]\n(.*?)(?=^\[|\Z)", text)
    section = match.group(0) if match else "[Video]\n"
    for key, value in profile["Video"].items():
        pattern = rf"(?m)^{re.escape(key)}=.*$"
        if re.search(pattern, section):
            section = re.sub(pattern, lambda _: f"{key}={value}", section)
        else:
            section = section.rstrip("\n") + f"\n{key}={value}\n"
    if match:
        updated = text[:match.start()] + section + text[match.end():]
    else:
        updated = text.rstrip("\n") + "\n\n" + section
    candidate = updated.replace("\n", newline).encode("utf-8")
    if candidate == original:
        print("The video profile is already applied.")
        return
    print("".join(difflib.unified_diff(
        text.splitlines(True), updated.splitlines(True),
        fromfile=str(target), tofile=str(target) + " (video profile)",
    )), end="")
    if args.dry_run:
        return

    for proc in Path("/proc").glob("[0-9]*/comm"):
        try:
            name = proc.read_text().strip()
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
        if name == "QGroundControl":
            parser.error("Close QGroundControl before applying its video profile.")
    if target.read_bytes() != original:
        parser.error("Config changed while preparing the profile; run the command again.")
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup = target.with_name(target.name + ".before-fpv-" + stamp + ".bak")
    with backup.open("xb") as output:
        output.write(original)
    shutil.copystat(target, backup)
    fd, temporary = tempfile.mkstemp(prefix=".qgc-fpv-", dir=target.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(candidate)
            output.flush()
            os.fsync(output.fileno())
        os.chmod(temporary, target.stat().st_mode & 0o777)
        os.replace(temporary, target)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print(f"Applied: {target}\nBackup: {backup}")


if __name__ == "__main__":
    main()
