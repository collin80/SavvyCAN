function setup ()
{
    host.log("Example script 1.0");
    host.setTickInterval(20000);
    can.sendFrame(0, 0x7E0, 8, [0x0d, 1, 0, 0, 0, 0, 0, 0]);
    
    can.setFilter(0x100, 0x700, 0);
}

function gotCANFrame (bus, id, len, data)
{
    host.log("Bus: " + bus + "  id: " + id.toString(16));
}
function tick()
{
    host.log("TICK!");
}