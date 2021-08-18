# SavvyCAN
Qt based cross platform canbus tool 
(C) 2015-2021 EVTV and Collin Kidder

A Qt5 based cross platform tool which can be used to load, save, and capture canbus frames.
This tool is designed to help with visualization, reverse engineering, debugging, and
capturing of canbus frames.

Please use the "Discussions" tab here on GitHub to ask questions and interact with the community.

Requires a resolution of at least 1024x768. Fully multi-monitor capable. Works on 4K monitors as well.

You are highly recommended to use the 
[CANDue board from EVTV](http://store.evtv.me/proddetail.php?prod=ArduinoDueCANBUS&cat=23).

The CANDue board must be running the GVRET firmware which can also be found
within the collin80 repos.

It is now possible to use any Qt SerialBus driver (socketcan, Vector, PeakCAN, TinyCAN).
There may, however, be some loss of some functionality as
some functions of SavvyCAN are designed for use directly with the
EVTVDue and CANDue 2.0 boards.

It should, however, be noted that use of a capture device is not required to make use
of this program. It can load and save in several formats:

1. BusMaster log file
2. Microchip log file
3. CRTD format (OVMS log file format from Mark Webb-Johnson)
4. GVRET native format
5. Generic CSV file (ID, D0 D1 D2 D3 D4 D5 D6 D7)
6. Vector Trace files
7. IXXAT Minilog files
8. CAN-DO Logs
9. Vehicle Spy log files
10. CANDump / Kayak (Read only)
11. PCAN Viewer (Read Only)

## Dependencies

Now this code does not depend on anything other than what is in the source tree or available
from the Qt installer.

Uses QCustomPlot available at:

http://www.qcustomplot.com/ 

However, this source code is integrated into the source for SavvyCAN and one isn't required 
to download it separately.

This project requires 5.14.0 or higher because of a dependency on QtSerialBus and other new additions to Qt. 

NOTE: Qt6 currently lacks support for QtSerialBus and many other Qt sub-features. At this time you cannot
use Qt6 to compile SavvyCAN. Support for Qt6 should be possible around Qt6.2.

## Instructions for compiling:

[Download the newest stable version of Qt directly from qt.io](https://www.qt.io/download/) (You need 5.14.x or newer)

```sh
cd ~

git clone https://github.com/collin80/SavvyCAN.git

cd SavvyCAN

~/Qt/5.14/gcc_64/bin/qmake

make
```

Now run SavvyCAN

```
./SavvyCAN
```

### Compiling in debug mode for additional information

```sh
qmake CONFIG+=debug

make
```

## What to do if your compile failed?

The very first thing to do is try:

```
qmake

make clean

make
```

Did that fix it? Great! If not, ensure that you selected SerialBUS support
when you installed Qt.

### What to do if `qmake` fails with error `Project ERROR: Unknown module(s) in QT: qml serialbus help` on Ubuntu? :

`sudo apt install libqt5serialbus5-dev qtdeclarative5-dev qttools5-dev`

### Used Items Requiring Attribution

nodes by Adrien Coquet from the Noun Project

message by Vectorstall from the Noun Project

signal by shashank singh from the Noun Project

signal by juli from the Noun Project

signal by yudi from the Noun Project

Death by Adrien Coquet from the Noun Project

