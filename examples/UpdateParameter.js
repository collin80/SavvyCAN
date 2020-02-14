var testingVar = 10;

function setup()
{
   host.addParameter("testingVar");
   host.setTickInterval(2000);
}

function tick()
{
   testingVar++;
}