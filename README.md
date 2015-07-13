# SavvyCAN
QT based cross platform canbus tool 
(C) 2015 EVTV and Collin Kidder

A QT5 based cross platform tool which can be used to load, save, and capture canbus frames.
This tool is designed to help with visualization, reverse engineering, debugging, and
capturing of canbus frames.

Really requires at a resolution of at least 1024x768. Fully multi-monitor capable.

Currently canbus capture requires a CANDue board from EVTV:
(http://store.evtv.me/proddetail.php?prod=ArduinoDueCANBUS&cat=23)

The CANDue board must be running the GVRET firmware which can also be found
within the collin80 repos.

It should, however, be noted that use of a capture device is not required to make use
of this program. It can load and save in several formats:

1. BusMaster log file

2. Microchip log file

3. CRTD format (OVMS log file format from Mark Webb-Johnson)

4. GVRET native format

5. Generic CSV file (ID,D0 D1 D2 D3 D4 D5 D6 D7)

