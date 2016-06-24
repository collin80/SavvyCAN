#include <QtTest>

#include "tst_lfqueue.h"
#include "tst_cancon.h"


int main(int argc, char** argv)
{
   QCoreApplication app(argc, argv);

   int status = 0;
   auto ASSERT_TEST = [&status, argc, argv](QObject* obj) {
     status |= QTest::qExec(obj, argc, argv);
     delete obj;
   };

   ASSERT_TEST(new TestLFQueue());
   ASSERT_TEST(new TestCanCon(CANCon::SOCKETCAN, "vcan0", 1));

   return status;
}
