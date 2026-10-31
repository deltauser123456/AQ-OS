# AQ OS

AQ OS is a small experimental 32-bit (i386) operating system project.

## Current features
- GRUB Multiboot boot
- VGA text-mode display
- Basic PS/2 keyboard input (US QWERTY map)
- Tiny shell: `help`, `about`, `version`, and `clear`

## Build on Windows 11 with WSL 2

Open PowerShell and run:

```powershell
wsl --install -d Ubuntu
```

Restart Windows if prompted. Open Ubuntu from the Start menu and install the tools:

```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib nasm grub-pc-bin xorriso mtools qemu-system-x86
```

Clone and build:

```bash
git clone https://github.com/deltauser123456/AQ-OS.git
cd AQ-OS
make
```

The ISO will be created at `dist/aqos.iso`.

Validate the kernel and run it in QEMU:

```bash
make check
make run
```

## AQ Browser networking progress

QEMU now provides an emulated Intel E1000 network adapter connected to user-mode networking. The kernel scans PCI and reports whether the adapter is present. This is device detection only; packet I/O, DHCP, DNS, TCP, HTTP, HTTPS, and webpage rendering are not implemented yet.

## Important
This is an educational hobby OS, not a replacement for Windows. Test it in QEMU first; do not write this early prototype to a physical drive. The keyboard map is currently US QWERTY.
