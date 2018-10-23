Sniffer Window
=================

.

.. image:: ./images/Sniffer.png

Using the Sniffer Window
=========================

This window is essentially a graphical version of the linux can_utils program "cansniffer". The general idea here is to display a list of frames such that you only see frames that are actively updating. If a given ID has not been seen in 5 seconds the ID portion will turn RED and then disappear from the list. In this way only frames that are updating are in the list. They are ordered by ID. Bytes that have deincremented will be red and bytes that have incremented will be green. You can use the "Filters" area to mask away some IDs so that they never show up. This can help to declutter the list. 

This window updates with a 200ms interval.

Notching and Unnotching
========================

While the window is running it keeps a running list each 200ms cycle of all the bits that changed in that timespan. Each 200ms this list is backed up and reset. If you push the notch button the system will remember all the bits that were set in the last 200ms window and will not color the output if those bits are toggled in the future. They will thus somewhat be ignored except that you can visually still see them updating. If you click the Notch button repeatedly it will add any new changed bits to the old changed bits. In this way you can build up a set of bits to ignore. Un-notching causes all notched (ignored) bits to be reset and thus all changes will be colored once again. 

