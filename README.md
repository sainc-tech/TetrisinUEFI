# TetrisinUEFI

Tetris running directly in UEFI firmware. No OS. No runtime. Just bare metal.

Built with EDK2, GOP for graphics, and the PC speaker for audio.

![License](https://img.shields.io/badge/license-GPL--2.0-blue)

---

## Precompiled

Don't want to build it yourself? Grab the latest precompiled `.efi` from the [Releases](../../releases/latest) page.

Just download `TetrisinUEFI.efi` and skip straight to running it.

---

## Features

- Runs before any OS boots
- Dynamic resolution scaling (480p to 4K)
- No screen flicker (dirty cell rendering)
- Korobeiniki (Tetris theme) on the PC speaker
- WASD and arrow key support
- Score, lines, level, next piece preview
- Mute toggle with M

## Controls

| Key | Action |
|-----|--------|
| W / Up | Rotate |
| A / Left | Move left |
| D / Right | Move right |
| S / Down | Move down |
| Space | Hard drop |
| M | Mute / Unmute |
| R | Restart |
| Q | Quit |

---

## Building

## Building

### Linux (Fedora / Ubuntu / Debian)

**1. Install dependencies**

Fedora:
```bash
sudo dnf install gcc nasm iasl python3 libuuid-devel
```

Ubuntu / Debian:
```bash
sudo apt install build-essential nasm iasl python3 uuid-dev
```

**2. Clone EDK2**

```bash
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init
```

**3. Build BaseTools**

```bash
make -C BaseTools
source edksetup.sh
```

**4. Copy project into EDK2**

```bash
cp -r TetrisinUEFI/ /path/to/edk2/TetrisinUEFI/
```

**5. Build**

```bash
build -a X64 -t GCC -p OvmfPkg/OvmfPkgX64.dsc -m TetrisinUEFI/TetrisinUEFI.inf
```

Output: `Build/OvmfX64/DEBUG_GCC/X64/TetrisinUEFI.efi`

---

### Windows

**1. Install dependencies**

- [Visual Studio 2019 or 2022](https://visualstudio.microsoft.com/) with C++ workload
- [NASM](https://www.nasm.us/) — add to PATH
- [ASL compiler (iasl)](https://acpica.org/downloads) — add to PATH
- [Python 3](https://www.python.org/)

**2. Clone EDK2**

```cmd
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init
```

**3. Build BaseTools**

Open a Visual Studio Developer Command Prompt:

```cmd
edksetup.bat Rebuild
```

**4. Copy project into EDK2**

```cmd
xcopy /E /I TetrisinUEFI C:\path\to\edk2\TetrisinUEFI
```

**5. Build**

```cmd
build -a X64 -t VS2019 -p OvmfPkg\OvmfPkgX64.dsc -m TetrisinUEFI\TetrisinUEFI.inf
```

Output: `Build\OvmfX64\DEBUG_VS2019\X64\TetrisinUEFI.efi`

---

## Running on real hardware

Copy `TetrisinUEFI.efi` to a FAT32 USB drive and boot it from your UEFI firmware shell.

**Ventoy** — rename the file to `BOOTX64.EFI` and place it in the root of the Ventoy partition. It will appear in the boot menu automatically.

**Direct boot** — place the file at:

```
EFI/BOOT/BOOTX64.EFI
```

This makes the USB itself bootable — plug it in, select it in your BIOS boot menu, and Tetris launches immediately with no extra steps.

---

## License

GPL-2.0 — see [LICENSE](LICENSE)

Copyright (C) 2026 SA.inc
