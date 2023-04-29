ADLIB21.DRV
===========

This is an updated version of msadlib.drv
with support for OPL2LPT and OPL3LPT devices.

BUILDING
========

See Makefile for details on building. You will
need the Windows 3.1 DDK and VC++ 5.2.

BUILD.BAT is used to recreate the prebuilts under PUB\

PREBUILTS
=========

PUB\ADLIB is for OPL2/OPL3 Ad Lib cards.
PUB\LPT3BC is for OPL2LPT/OPL3LPT on port 0x3BC (LPT1 on some machines)
PUB\LPT378 is for OPL2LPT/OPL3LPT on port 0x378 (LPT1 on most machines)

There are no prebuilts with OPL=3 as frankly the driver doesn't do
anything special today outside of some tweaked I/O timings.

INSTALLING
==========

Copy over the exising file under \WINDOWS\SYSTEM.
Then, proceed to install Ad Lib support as usual
using Control Panel / Drivers.

If you are using OPL2/3 via LPT, you can comment
out the vadlibd.386 driver in \WINDOWS\SYSTEM.INI,
as it is not used. Find the line that reads:

device=vadlibd.386

...and comment it out, by prepending a semicolon,
like this:

;device=vadlibd.386

If you had previously installed a 3rd-party sound
card driver, your original \WINDOWS\SYSTEM\MIDIMAP.CFG
file might have been overwritten. You can either copy
over MIDIMAP.CFG from this repository, or add the
missing configurations using the "MIDI Sequencer" Control
Panel applet. The 'Ad Lib' configuration (which sounds
as expected) only sets up channels 13 through 16
(no patch maps). The 'Ad Lib' general sets up all 16
channels.
