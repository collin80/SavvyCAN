Preference Window
=================

![Preference Window](./images/Preferences.png)

There are a variety of preferences that can be set in the program and persisted for future sessions.

General Settings
====================

"Save/Restore window positions and sizes": If this is set then the size and placement of the various windows in this application will be saved when the program is closed and loaded when each window is brought back up in the future. This can be used to create your own preferred layout. If you'd rather things come up in their default state every time then you can uncheck this box.

"Save/Restore CAN bus connections": This will cause the program to remember the devices you were connected to last time you ran the program and attempt to reconnect to them upon start up.

"Display values as hexadecimal": A lot of the time people who are doing CAN reverse engineering like to see values in hexadecimal (base 16) instead of the more familiar decimal (base 10) system. Checking this box will cause most of the values in the application to show up in hex. This applies to CAN ids and data bytes. Unchecking this causes values to default to decimal instead. The reason for using hex is that each hex digit is 4 bits. Integers on a computer tend to be in multiples of 8 - 8, 16, 32, 64. So, hex digits have a direct mapping to the underlying binary. Decimal does not have this correspondence AT ALL. But, the choice is yours.

"Require validation of GVRET connection": GVRET style devices run over a serial connection. Serial connections can be finicky sometimes and so the connection can be validated to prove that everything is really still operating and talking. There probably isn't any reason to turn this off except while debugging to see if it changes anything. Mostly just don't touch this.

"Label filters using messages from DBC files": Part of a two part system. If this is selected and a given DBC file also has its checkbox checked then the IDs list on the main screen (lower right) used for filtering will have not only the IDs but also the message name from the DBC file. This will allow for more easily determining what a given message means. Sometimes IDs can be hard to remember. Do note that this checkbox must be checked or the checkboxes for each loaded DBC file won't do anything. This is the master on/off switch.

"Time Keeping": There are a variety of ways one could timestamp CAN frames as they come into the program. Selecting "Seconds" will cause the timestamp to be expressed as seconds since the frame list was last cleared. This tends to be an easy choice to work with. "Microseconds" will express the timestamp as millionths of a second since the last time the frame list was cleared. This is exactly like "Seconds" mode but without any decimal point. You might find this to be a bit hard to conceptualize. The last option is "System Clock" this will timestamp frames with the current system time when the frame came in. This is still very precise but now you'll get an absolute time stamp with the full date and time. The display of this mode can be changed by editing the "Time Format String" value. It defaults to an output that looks like "JAN-10 12:34:53.234" But you can set it to other values. Look here to find a reference for how you can create new format strings: http://doc.qt.io/qt-4.8/qdatetime.html#toString

"Use filtered frames in sub-windows": The main window has a filtering interface where you can uncheck IDs to hide them. Ordinarily when you bring up one of the other windows it will still use the main unfiltered list. Sometimes you really do want to deal with the filtered list of frames even in the other windows. If this is checked then the other windows will see the filtered list and not the unfiltered actual list of frames that have been captured.

"OpenGL Accelerated AntiAliased Graphing": Checking this will cause all of the graphs to use OpenGL 3D acceleration. Most modern machines have some form of 3D acceleration so this option should be OK to use. If you check this your graphs will look a lot better and on good hardware should also be faster. In the future other options are likely to be added to the graphing screen that will likely only be enabled if OpenGL mode is also enabled. Try enabling this and see if performance is still good. It's safe to leave it off if in doubt.

"Autoscroll main frame window by default": If checked it will automatically check the relevant checkbox on the main screen when the program starts. This will cause the view to automatically scroll to the bottom by default. This is merely a convenience option if you'd prefer it to be the default.


Flow View Settings
==================
"Use timestamp mode by default": This is another convenience option. With this checked the Flow view will default to using time stamps on the graphing area instead of frame numbers.

"Set Auto Reference by default": The Flow view can either use static referencing or dynamic referencing (see the Flow view documentation for more info). If you'd like to use dynamic referencing and automatically set the reference by default then check this option.


Playback Window Settings
========================
"Loop by default": Set the playback window to default to looping infinitely by default.

"Default playback speed (ms)": Set the default timing for playback

"Default Sending Bus": Set a default for which bus to send frames on.


File Info And Comparator Window Settings
========================================
"Auto expand all nodes": Both of the referenced windows have tree views that potentially have a large number of nodes. It's more neat not to expand them all by default but it also then requires more clicks if you have to expand them to view the information. So, you can set whether you'd like to expand them all by default or not.


MQTT Settings
=============

MQTT allows SavvyCAN to connec to a broker for CAN transmission over the internet. You will need to set up the host, port, username, and password. These things are determined by your broker. You can run your own on Linux with Mosquitto.


Final Word of Warning
=====================

After changing preferences it is the best practice to close and reopen the application to ensure that all settings have taken effect. Some settings do take effect immediately but others do not.
