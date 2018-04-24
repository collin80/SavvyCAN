Sniffer Window
=================

.

.. image:: ./images/Sniffer.png

Using the Sniffer Window
=========================

This window is essentially a graphical version of the linux can_utils. The general idea here is to display a list of frames such that you only see frames that are actively updating. If a given ID has not been seen in 4 seconds the ID portion will turn RED and then disappear from the list. In this way only frames that are updating are in the list. They are ordered by last update time. Bytes that have deincremented will be red and bytes that have incremented will be green. You can use the "Filters" area to mask away some IDs so that they never show up. This can help to declutter the list. 

Notching and Unnotching
========================

Honestly, I don't know. It seems that notching means to store the current value of the data bytes for each frame and then use that notch data for the comparison for increment / deincrement to color the bytes. Unnotching would then be clearing that out so that it uses the previous value from the last time the frame ID was seen. But, don't quote me on that. This should be figured out definitively and corrected.


