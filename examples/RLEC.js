var newID = 0;

function setup ()
{
    host.log("RLEC ID Changer");
    can.setFilter(0x0, 0x0F, 0);
    can.sendFrame(0, 0x7E0, 8, [0x0d, 1, 0, 0, 0, 0, 0, 0]);       
}

function gotCANFrame (bus, id, len, data)
{
     var dataBytes = [];

     if (len == 8)
     {
         if (data[0] == 0xd && data[1] == 1 && data[2] == 0xAA)
         {
            host.log("Got challenge: 0x" + data[3].toString(16) + data[4].toString(16));
            var notData3 = ~data[3];
            var notData4 = ~data[4];            
            dataBytes[0] = 0xD;
            dataBytes[1] = 2;
            dataBytes[2] = ((notData4 & 0xF) << 4) + ((notData3 >> 4) & 0xF);
            dataBytes[3] = ((notData4 >> 4) & 0xF) + ((notData3 & 0xF) << 4);
            dataBytes[4] = 0;
            dataBytes[5] = 0;
            dataBytes[6] = 0;
            dataBytes[7] = 0;
            can.sendFrame(0, 0x7E0, 8, dataBytes);
         }
         if (data[0] == 0xd && data[1] == 2 && data[2] == 0xAA)
         {
             host.log("Passed security Check!");
             dataBytes[0] = 4;             
             dataBytes[1] = 0x15;
             dataBytes[2] = newID;
             can.sendFrame(0, 0x7E0, 8, dataBytes);
         }       
         if (data[0] ==4 && data[1] == 0x15 && data[2] == 0xAA)
         {
            host.log("ID Reprogramming Successful!");
         } 
     }
}