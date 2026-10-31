"""Build the handheld firmware, and copy it onto the Pico.

    python tools/pico.py build [target]   # target: hw_test (default)
    python tools/pico.py flash [target]   # build, then load it onto the Pico

Uses the compiler and Pico SDK that the "Raspberry Pi Pico" VS Code extension
downloaded into ~/.pico-sdk (the newest version of each). The build goes in
build-pico/.

Flashing: if the Pico is already running one of our programs over USB,
picotool reboots it and loads the new one. Otherwise hold the BOOT button
while plugging in USB (a drive called RP2350 appears) and run flash again.
"""

import os
import re
import shutil
import string
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "platform" / "pico"
BUILD = ROOT / "build-pico"
TOOLS = Path.home() / ".pico-sdk"


def newest(kind):
    """The newest version folder of one of the extension's tools."""
    folder = TOOLS / kind
    versions = [p for p in folder.iterdir() if p.is_dir()] if folder.is_dir() else []
    if not versions:
        sys.exit(f"Nothing in {folder}. Install the Raspberry Pi Pico VS Code extension "
                 "and create one project with it, so it downloads the tools.")
    return max(versions, key=lambda p: [int(n) for n in re.findall(r"\d+", p.name)])


def exe(path):
    return str(path) + (".exe" if os.name == "nt" else "")


def build(target):
    sdk = newest("sdk")
    toolchain = newest("toolchain")
    cmake = exe(newest("cmake") / "bin" / "cmake")
    ninja = exe(newest("ninja") / "ninja")
    picotool_dir = newest("picotool") / "picotool"
    pioasm_dir = newest("tools") / "pioasm"

    env = dict(os.environ, PICO_SDK_PATH=str(sdk))
    if not (BUILD / "CMakeCache.txt").exists():
        subprocess.run([cmake, "-S", str(SOURCE), "-B", str(BUILD), "-G", "Ninja",
                        f"-DCMAKE_MAKE_PROGRAM={ninja}",
                        f"-DPICO_SDK_PATH={sdk}",
                        f"-DPICO_TOOLCHAIN_PATH={toolchain}",
                        f"-Dpicotool_DIR={picotool_dir}",
                        f"-Dpioasm_DIR={pioasm_dir}",
                        "-DCMAKE_BUILD_TYPE=Release"], check=True, env=env)
    subprocess.run([cmake, "--build", str(BUILD), "--target", target], check=True, env=env)
    return BUILD / f"{target}.uf2"


def boot_drive():
    """The Pico's USB drive when it's in BOOT mode, if one is plugged in."""
    if os.name != "nt":
        return None
    for letter in string.ascii_uppercase:
        drive = Path(f"{letter}:/")
        try:
            if (drive / "INFO_UF2.TXT").exists():
                return drive
        except OSError:
            pass
    return None


def flash(uf2):
    picotool = exe(newest("picotool") / "picotool" / "picotool")
    # -f reboots a running Pico into BOOT mode first; -x starts the new program.
    result = subprocess.run([picotool, "load", "-f", "-x", str(uf2)])
    if result.returncode == 0:
        return
    drive = boot_drive()
    if drive:
        print(f"picotool couldn't reach the Pico; copying to {drive} instead")
        shutil.copy(uf2, drive)
        return
    sys.exit("Couldn't find the Pico. Hold BOOT while plugging in USB, then run this again.")


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("build", "flash"):
        sys.exit(__doc__)
    target = sys.argv[2] if len(sys.argv) > 2 else "hw_test"
    uf2 = build(target)
    if sys.argv[1] == "flash":
        flash(uf2)
    print(f"Done: {uf2}")


if __name__ == "__main__":
    main()
