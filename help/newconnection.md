Adding a new connection
========================

![New Connection](./images/NewConnection.png)

At the moment it is possible to use any QT SerialBus compatible device and any GVRET compatible device in any of the supported operating systems. SerialBus supports socketcan on linux, passthrough on Linux and Windows 32 bit, and Vector, PeakCAN, and TinyCAN on supported OS's.

At this time GVRET compatible devices are: EVTVDue, EVTV CANDue (1.3/2/2.1/2.2), 
Teensy 3.1-3.6, Macchina M2, Macchina A0, EVTV ESP32 Due.

You can also use a variety of network based connections to gain access to remote capture hardware.


Connecting To GVRET Devices
==============================

SavvyCAN is able to connect to GVRET compatible devices to capture new traffic. These 
devices will present as serial ports on the connected PC. To connect to a dongle select "Serial Connection." This will bring up a list of serial ports on the machine. Select the proper one and then press "Create New Connection". This will close the window and bring you back to the connection manager window. If connection succeeds the status will show "Connected" for your newly set up device.

You can also connect to some GVRET devices over the network (A0, EVTV ESP32Due). These devices broadcast their address. Once you've selected "Network Connection (GVRET)" you should see a list of IP addresses that appear to have GVRET devices on them. You can also manually enter the proper IP address but if the device did not automatically register itself it is unlikely to work with a manual entry either.


Connecting to QT SerialBus Compatible Devices
=============================================

SavvyCAN can also connect to a wide variety of CAN hardware through the built-in QT
SerialBus drivers. These drivers vary by operating system but support socketcan on LINUX
and Vector tools on both LINUX and Windows. When you select "QT SerialBus Devices" you will
get a list of device types supported. Select a device type and for most devices you should see
the Port list fill out with all registered and valid ports for that driver. Socketcan devices, for
instance, are automatically detected now. Then push "Create New Connection" and you should 
see the new connection in the table on the left of the window. Note that SocketCAN devices 
don't support changing the baud rate within a program. You must do this when you set up 
the connection via console commands. This is outside the scope of this documentation. 
Consult the SocketCAN documentation for details on configuring such devices.

For CAN FD, tick "CAN FD" and pick the data phase rate before creating the connection (both can also be
changed later in the bus settings of the connection window). The CAN speed and data rate are passed to
the driver; drivers that set bit rates themselves (e.g. PEAK) use them, while for SocketCAN they are set
with "ip link" (e.g. `ip link set can0 type can bitrate 500000 dbitrate 2000000 fd on`) and only the CAN FD
setting matters. A virtual SocketCAN interface can carry CAN FD frames too: `ip link set vcan0 mtu 72`.
Received CAN FD frames show "D" in the Type column of the main frame list, "DB" with bit rate switching.

QT also includes a "virtualcan" device type. You can use this to create a bus that will loop back anything you send to it. This is useful for testing without needing to connect any devices or load any log files.

Connecting to a pico-flexray (FlexRay)
======================================

[pico-flexray](https://github.com/dynm/pico-flexray) is a Raspberry Pi Pico based FlexRay capture and man-in-the-middle board. Select "pico-flexray (FlexRay, USB)". The port list shows the serial numbers of the connected boards (they start with "picoflex"); leave it empty to use the first one found. The board appears as a Panda (USB VID 0x3801, PID 0xddcc).

- **Windows:** the board needs the WinUSB driver. Install it once with [Zadig](https://zadig.akeo.ie/): Options > List All Devices, pick the "panda" device (interface 0), choose WinUSB and press Install Driver.
- **Linux:** the user needs access to the USB device, e.g. a udev rule such as `SUBSYSTEM=="usb", ATTR{idVendor}=="3801", ATTR{idProduct}=="ddcc", MODE="0666"`.
- The option is greyed out if SavvyCAN was built without libusb-1.0.

Captured frames go to bus 0 (FR1) and bus 1 (FR2) of the connection and show "F" in the Type column. The ID is the FlexRay slot ID and the first data byte is the cycle count, followed by the FlexRay payload (up to 254 bytes). This matches the FlexRay DBC files used with pico-flexray (for example its `dbc/lateral.dbc`), which multiplex on the cycle count in byte 0, so those files decode the frames as they are. Null frames are dropped. Timestamps are taken by the PC when each USB transfer arrives, so frames that arrive together share a timestamp. The connection only listens: injecting frames isn't supported yet.

FlexRay frames are saved in SavvyCAN's native CSV format with "FR" in the flags column after the data. The Frame Info window's per-byte statistics cover the first 8 bytes and Flow View shows the first 64 bytes of longer frames.

Connecting to Socketcand
========================

This is a LINUX only solution which allows one to connect to a socketcan device that is registered on the local network. You can also set up SSH tunnels or VPN to expand the reach over the internet. It should fill out a list of any available socketcand interfaces. Setting up socketcand is outside the scope of this help file but may your GoogleFu be strong.

Connecting over MQTT
====================

Lastly, it is possible to connect to an MQTT broker to send and receive CAN traffic over the internet. This is much like socketcand but more cross platform and also supports easy broadcasting. For instance, for capture the flag events, it would be possible to connect the device over MQTT and have multiple participants and/or watchers all connected at once. Connection to the MQTT broker is set up in the main SavvyCAN preferences. In this window you merely select the topic name to subscribe to. There is currently no automatic way to list these topics so you will need to know the topic to subscribe to ahead of time. It should be noted that the bidirectional nature of this interface means that everyone is on equal footing. You can create an MQTT interface that others can connect to or you can connect to a topic that is currently being sent to from elsewhere and get the traffic. Additionally, the SavvyCAN source code at GitHub has a python script which can be used to connect a socketcan interface to MQTT. You can use this script on a remote system to connect it to the internet so that you can run SavvyCAN somewhere apart from the device under test.
