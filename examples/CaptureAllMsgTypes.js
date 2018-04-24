function setup ()
{
    host.log("Example script 1.0");
    host.setTickInterval(20000);
    can.sendFrame(0, 0x200, 4, [1,30,20,10]);
    uds.sendUDS(0, 0x604, 0x3E, 1, 0, 0, 0);
    can.setFilter(0x100, 0x700, 0);
    uds.setFilter(0x600, 0x7F0, 0);

}

function gotCANFrame (bus, id, len, data)
{
    host.log("Bus: " + bus + "  id: " + id.toString(16));
}

function gotISOTPMessage(bus, id, len, data)
{
    host.log("ISOTP bus " + bus + "  ID: " + id.toString(16));
}

function gotUDSMessage(bus, id, service, subFunc, len, data)
{
    host.log("UDS Bus: " + bus + "  ID: " + id.toString(16) + "    Sv: " + service.toString(16));
}

function tick()
{
    host.log("TICK!");
}