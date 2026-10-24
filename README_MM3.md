# Nokia MikroMikko 3 (ASC) work in progress

Experimental MAME drivers and devices for the Nokia MikroMikko 3 family (Nokia Data, Finland, 1986-1989),
developed in this fork by Mr. MikroMikko. This is **work in progress**, not intended for upstream as is;
MAME developers are welcome to take whatever is useful.

**AI disclosure:** most of the code, the firmware analysis and this documentation were written with the help
of an AI coding assistant (Claude, by Anthropic, used through Claude Code). See "AI assistance" below.

The goal is an emulation that is comparable to the real machine at hardware level, because the system is also
used for research (software archaeology, timing, firmware behaviour). Every simplification is therefore listed
below instead of hidden.

## Branches

| Branch | Based on | Contents |
|---|---|---|
| `mm3-common` | `master` | Devices shared by several MM3 models: NDC 286 display controller, MDA/CGA stand-in cards, HLE keyboard, Nokia D8742 firmware option, IDE timing fixes, **WD1003-WA2 MFM controller** (calibrated against a real drive), **Paradise Autoswitch EGA2** (Nokia EGA card, experimental) |
| `mm3-asc` | `mm3-common` | `mm3asc` (NDC in CGA mode), `mm3asc_mda` (NDC in MDA mode) and `mm3asc_ega` (Nokia EGA card), SBC 286 board, Phoenix BIOS 1.50 |
| `mm3-m215` | `mm3-common` | `mm3ttm215` (MikroMikko 3TT m215): Chips NEAT chip set, Nokia ROM BIOS 7.02, PS/2-type keyboard controller (HLE); passes POST and boots MS-DOS from a floppy, hard disk not tested |

Rule used: a part moves into `mm3-common` when more than one model uses it (the WD1003-WA2 and the EGA card are used by the ASC and the 3TT/AWS machines).

## What is emulated

- **SBC 286 mother board** (`mm3asc`, `mm3asc_mda`, `mm3asc_ega`): 80286 at 8 MHz, 1 MB RAM, Phoenix 80286 ROM BIOS 1.50
  (Nokia customised, versions G and I selectable), AT-compatible chip set from the existing `at_mb` device.
  A default CMOS image matching the real machine setup (1.2 MB floppy, fixed disk type 13) is embedded.
- **Keyboard controller**: the real firmware of the Nokia D8742 (9544D) runs on the MCS-48 core; the keyboard
  itself is a temporary AT keyboard stand-in (`mm3_hle_kbd`, Finnish layout via the Nokia driver in DOS).
- **NDC 286 display adapter**: the real 8031 firmware (729508/G, FW70) runs on the MCS-51 core and talks to the
  host through ports 3B3/3B6/3B7. Video itself is *not* emulated at chip level: the card appears as a monochrome
  or colour text adapter (MDA 720x400 with three soft key label rows, CGA 640x200), character sets come from the
  firmware image, colours were measured from photographs of a real monitor.
- **Nokia EGA card** (`mm3asc_ega`, experimental): the Paradise Autoswitch EGA2 (PEGA 2A) that Nokia shipped as
  its EGA adapter. It runs the real card BIOS (Paradise PEGA BIOS 1986, 22030-C1201, version string 22030-C12, dumped
  from a Nokia MM3 card and verified against a second card). The card is its own device (`pega2a`) on MAME's generic
  EGA core, with what disassembling the card BIOS and port reads of the real card (a DOS program, IN only) showed:
  - configuration switches as on the real card: levers 1-4 through Input Status Register 0 bit 4 (sense 3 = lever 1),
    levers 5/6 through bits 6/5; default = the setting of the Nokia card (levers 2 and 3 ON = Enhanced RGB, 350 line);
  - a 1 KB scratch RAM window at A0000-A03FF (3DF bit 6) with a shadow of the CGA/MDA register writes, used by the BIOS
    for its state;
  - the AutoSwitch NMI trap: a write to the CGA/MDA compatible ports raises IOCHCK (NMI) when the BIOS has armed it
    (3DF bit 7); the BIOS NMI handler emulates the write. With the Nokia switch setting the BIOS does not install the
    handler, so the trap is normally idle;
  - dot clock selection with the card's oscillators (Misc Output bits 3-2: 14.318, 16.257, 25.000, 27.256 MHz) for the
    480 line (BIOS modes 50h-53h) and 132 column (54h/55h) modes;
  - the input status bits and open bus behaviour measured on the real card (3C2/3C3 low nibble, 3CC-3CF, 3D8, 3DC-3DF);
  - a border (overscan colour) only between the active display and the start of blanking, from the CRTC blanking
    registers, as a monitor shows it.

  Left out on purpose because the Nokia setting does not use them: the hardware CGA/MDA/Hercules/Plantronics emulation
  of the chip (the card BIOS emulates MDA/CGA writes in software through the NMI trap when it is enabled).
- **WD1003-WA2 hard disk controller**: high-level emulation of the Winchester side written from the
  preliminary OEM manual (7 Jan 1986). Task file at 1F0-1F7, 3F6 alternate status / fixed disk register,
  IRQ14, Restore, Seek, Read/Write Sector (Long, multi sector across tracks), Format Track, Read Verify, Diagnose,
  Set Parameters. The drive is modelled mechanically (rotation, 1:3 interleave, index gap, measured seek curve of
  a NEC D5126). The floppy half of the card is still provided by the generic ISA FDC card.

## Status: tested and not tested

Tested:
- POST completes, NDC self test passes, default CMOS is accepted.
- Nokia MS-DOS 3.20 (v2.20) boots from a floppy image and from a type 13 hard disk image (1024/8/17) through the
  WD1003-WA2 device; DIR, CHKDSK, file copy work.
- Soft key rows, MDA/CGA text, XMODE character set switching, keyboard through the real D8742 firmware.
- `mm3asc_ega`: the Paradise EGA BIOS starts the card, the Phoenix POST text and the DOS prompt appear on the EGA
  output (80 x 25, 8x14 characters, 656 x 350 visible raster at 59.9 Hz with the real card's switch setting), the hard disk
  is found. The five settings of the card's monitor table (mono, standard 40/80 column, enhanced 200/350 line) give
  the expected video modes (checked by video mode and character height only). The Phoenix BIOS accepts the
  configuration only with CMOS display type EGA/VGA. Port reads (3B0-3DF) of the emulated card equal those of the
  real card on all decoded ports. BIOS modes 55h (132x25) and 54h (132x43) run at 60.3 Hz, 50h-53h (640x480) at
  59.3 / 62.2 Hz; the utility disk programs of the card (13225.COM, 13243.COM) and its utility PEGA.EXE run.
  Commander Keen 1 (EGA mode 0Dh, PC speaker sound) is playable.
- **Disk timing**: rotation, interleave layout, index gap, seek curve and track read time of the WD1003-WA2 with a
  NEC D5126 drive were calibrated against a real machine (same BIOS) and match within about 1 % for raw INT 13h
  operations.

Not tested or not done:
- **CPU and bus timing**: file access with small DOS chunks does not match the real machine (real: one revolution per
  512-byte call, MAME: about 3 ms) because the emulated 286 and ISA bus timing is not accurate yet: MAME's loops are
  about 20 % too fast, `rep movs/stos` about 20 % too slow and ISA I/O cycles about 25 % too fast. The controller
  command overhead and self test time of the WD1003-WA2 are estimates; the drive model has one drive profile (NEC D5126).
- No chip-level video for the NDC (CRTC, display RAM, PALs - the NDC PALs could not be read), no Hercules / Nokia
  720x400 graphics modes, no dual display (NDC + EGA), no VGA/NVGA cards (ROM dumps missing).
- **EGA card**: not emulated:
  the 2A locking/extension registers that the BIOS and PEGA.EXE use (a handshake of two reads of 3D8 followed by
  writes to CRTC index 00h/1Ah and 3DB; port 3DE), the hardware CGA/MDA/Hercules emulation of the 2A, mono 132 column
  modes 56h/57h (they need a mono setting and monitor). Not compared with the real card: timing, colours, graphics
  modes, the 480 line sync frequencies, the border width on a monitor. Which of levers 5/6 is read in bit 6 and which
  in bit 5 is a guess (both OFF on the real card). The screen aspect is fixed at 4:3, which suits the 350 line mode.
- WD1015-37 firmware of the WA2 is a masked ROM that has not been dumped; its behaviour is reproduced from the
  manual. ECC bytes and retries are not modelled, Format Track ignores the host interleave table.
- `mm3ttm215`: the keyboard controller is a high level stand-in. The BIOS needs a PS/2-type controller (auxiliary
  port, command byte bit 5 = auxiliary clock off); the real P8742AH is not dumped yet, so its exact command set,
  timing and the password commands are not reproduced. No mouse is connected. The hard disk controller of the m215
  is not known and not emulated; only floppy boot is tested.
- No MM3/386 (Chips CS8230 chip set), no MM3TT/AWS variant, no LAN Manager networking.
- Shared code changed outside the MM3 devices: the AT board (`at_mb_device`) now treats bit 7 of port 70h as the NMI
  mask (1 = disabled) as on a real AT (it was inverted, so an IOCHCK NMI never reached the CPU); `isa8_ega_device`
  got a virtual dot clock and `crtc_ega_device` getters for its blanking registers (neither changes the behaviour of
  other machines). Only the `mm3` subtarget was
  built and run; other machines using these devices were not tested.
- Apart from the disk timing and the EGA port reads, nothing has been compared side by side with the real machine.
  `-validate` passes for the MM3 drivers (the `mm3` subtarget reports three errors of other `at.cpp` drivers whose
  parents are in other source files); built only for the `mm3` subtarget on Windows/MinGW.

## ROMs

ROMs are **not** included and will never be committed. Only the project's own dumps of the real machine are used
(no IBM ROMs). BIOS images go in the machine folder (`roms/mm3asc/`, `roms/mm3ttm215/`); ROMs of shared devices go
in the folder of the device (`roms/ndc286/`, `roms/pega2a/`, `roms/at_keybc/`), so that every machine using the
device finds them:

| File | Used for |
|---|---|
| `9542g.u75`, `9543g.u84` | BIOS 1.50G (even/odd) |
| `9542i.u75`, `9543i.u84` | BIOS 1.50I (even/odd) |
| `9544d.bin` (`roms/at_keybc/`) | Keyboard controller firmware (D8742) |
| `729508g.ic38` (`roms/ndc286/`) | NDC 286 firmware (27256) |
| `22030-c1201.bin` (`roms/pega2a/`) | Nokia EGA card BIOS (Paradise PEGA BIOS 1986, 16 KB, 27128) |
| `tva1195511r2_full64.bin` | 3TT m215 BIOS 7.02 (64 KB) |

Checksums are in the ROM definitions of `src/mame/pc/at.cpp`, `src/devices/machine/at_keybc.cpp`,
`src/devices/bus/isa/ndc286.cpp` and `src/devices/bus/isa/pega2a.cpp`.

## Running

Build (MSYS2/MinGW64, subtarget `mm3`):

    make SUBTARGET=mm3 SOURCES=src/mame/pc/at.cpp

Start, with a hard disk image as `-hard1` (CHD) and an optional floppy image as `-flop1`:

    mm3 mm3asc -rompath roms -hard1 disk.chd -flop1 boot.img

Variants: `mm3asc_mda` (NDC in monochrome mode) and `mm3asc_ega` (Nokia EGA card); the display card can also be chosen
as a slot option (`-isa1 ndccga` / `-isa1 ndcmda` / `-isa1 pega2a`), but then the CMOS display type has to match
(the BIOS reports "Invalid configuration" otherwise; the CMOS is kept in `nvram/<machine>/`, delete it after
changing the defaults). Work on copies of floppy and disk images, never on the originals.

Over a remote desktop (RDP) use `-keyboardprovider dinput -mouseprovider none -ui_mouse`: keys then reach the
emulated keyboard as held keys (needed by games). With `-natural` only characters get through, as short taps.

## Timing measurement tools

DOS programs that measure the real drive and the emulated one the same way (PIT timer, INT 13h reads and DOS
file operations) are kept next to the project documentation: `DISKTIME.COM` (raw read timing: rotation,
interleave, seek curve), `FILETIME.COM` (write/read of a 128 KB test file in `C:\TEMP`), `CPUTIME.COM`
(CPU, memory and I/O bus speed, to check the 286 core timing), `ROMDUMP.COM` (dumps option ROMs from
`C000-EFFF`) and `PRTRD.COM` (reads the display ports 3B0-3DF, IN only, to compare a real card with the model). The programs were run on a real machine on 2026-10-23 and the results were used to calibrate the drive model.

## AI assistance

This work was done by a human (Mr. MikroMikko) together with Claude, an AI coding assistant from Anthropic
(Claude Code). Concretely, the assistant wrote and edited the C++ of the drivers and devices, disassembled and
analysed the dumped firmware (8031 NDC, UPI-41 keyboard controller), read the machine manuals, wrote the DOS
measurement programs, and drafted this README and the commit messages. Commits made with its help carry a
`Co-Authored-By: Claude` trailer.

What the human did: chose the goals, supplied the real machine documentation, ROM dumps and photographs,
decided on design questions, ran and checked the results in the emulator, and is responsible for the published
result. What has been verified against real hardware: the disk timing (measured on one real machine), the ROM dumps and the
port reads and switch settings of the EGA card.
Everything else, in particular the HLE of the WD1015 firmware, the NDC video behaviour and the EGA card emulation,
comes from manuals, firmware analysis and estimates, not from measurements.
Treat everything as unverified until the status section says otherwise, and review the code as you would any
untrusted contribution.

## Credits

Machine documentation, ROM dumps and photographs: the MikkoMuseo collection. Emulation and reverse engineering:
Mr. MikroMikko, with Claude (Anthropic).
