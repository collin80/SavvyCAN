UDS Scan Window
=================

.

.. image:: ./images/UDS_Scanner.png

Purpose of the UDS Scan Window
===============================

This is essentially another CAN fuzzing window but a very special one. This window is meant to search for UDS compliant (or nearly compliant) nodes on the CAN bus. It can also be used to do a blanket search for services, sub functions, and data items on a known UDS node.


Using the UDS Scan Window
==========================

UDS queries are sent out on the bus from "Starting ID" to "Ending ID". Usually UDS compliant ECUs will respond to 0x7E0 through 0x7E7 which is why those are the defaults. Some vehicles use UDS "like" protocols on other IDs. Usually UDS nodes reply with an ID 8 higher than the request ID. This is thus the default in the program. However, some nodes cheat and do not do this. It is quite common for responses to come from an address 16 higher instead. To deal with this situation there is a checkbox "Allow adaptive reply offset." If this is checked then replies will be accepted no matter what address they come from. Deselecting this will cause only replies of the proper offset to be accepted. The offset defaults to 8 but can be changed with the "Reply Offset" selector. Additionally, you can select which bus to scan and set how long you want to wait for replies. 

You need to also set a type of scan to do. You can select more than one type.

"Read By ID" - UDS allows one to read data from the ECU by an ID number. These are not defined anywhere and are custom to each ECU. But, you can use this to scan a range of IDs to see if you get a response to any of them. 
