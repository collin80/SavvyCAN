Frame Details Window
======================

![Frame Details Window](./images/FrameInfoWindow.png)

The Purpose of Frame Details Window
===================================

This window is used to get detailed statistics about frames. 

It provides information about a given frame ID across all frames with that ID. You can get such information as the number of frames, the number of data bytes that frame ID has, the average interval between frames with that ID, and the minimum and maximum interval. 

Also listed are detailed statistics for each data byte in that frame. Each byte has listed which bits changed, the range of values found, and a histogram both graphically (at the right-hand side of the window) and textually. The textual representation shows the number of times a specific value occurred. 

If you have a DBC file loaded which matches the ID you've selected then you will also see details about how the various signals changed over the capture.

The top right graph is a histogram of all the bits and the number of times each bit was set. This can be used to quickly visually see where data has changed. 

The bottom right has 8 graphs, one for each possible byte in a standard CAN frame (CAN-FD support is coming... eventually) Double clicking one of these graphs will size it up and remove the other 7 graphs. Double clicking again brings the 8 byte view back.

As in other windows, it is possible to use the mouse wheel to scale all the graphs on this form and it is also possible to pan around by clicking and dragging. Selecting one axis will also let you scale / pan just that one axis.

In the middle there is a bitfield view. This is color coded based upon how often each bit changes. It is called the "heatmap" for this reason. Bits that don't change often are cold and colored blue. Bits that are hot get increasingly red. In the picture you can see distinct areas where the bits are blue, light blue, green, orange. This is highly indicative of a counter. When a counter is found you will find that the lower bit changes basically every frame, the next bit up every other frame, the next bit every fourth frame, etc. This produces a very distinct pattern in the heatmap. Bits that never change are black. This view thus allows one to see where changing data is found within a frame. Chances are you can ignore all the black parts and focus only on places where some change has happened.

The interval histogram is in logarithmic scale and shows a listing of what intervals were seen between frames. This can be used to visually see the frame timing. Some frames get sent very regularly. They will show a very pronouced bell curve. Other frames might get sent on demand. These frames will have peaks at odd places and not conform to a nice distribution. For instance, in the picture you can see that the frame shows quite a few messages around 100ms but messages extend out to around 600ms as well. Still, the logrithmic scale means that the faster interval is, by far, the most common.
