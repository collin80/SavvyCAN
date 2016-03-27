#include "canconnection.h"
#include "canconnectioncontainer.h"

CANConnectionContainer::CANConnectionContainer(CANConnection *conn)
{
    thread = new QThread();
    connection = conn;
    connection->moveToThread(thread);

    connect(thread, &QThread::finished, conn, &QObject::deleteLater);
    connect(thread, &QThread::started, conn, &CANConnection::run); //setup timers within the proper thread
    connect(conn, &CANConnection::frameUpdateRapid, MainWindow::getReference(), &MainWindow::gotFrames, Qt::QueuedConnection);
    connect(MainWindow::getReference(), &MainWindow::sendCANFrame, conn, &CANConnection::sendFrame, Qt::QueuedConnection);
    thread->start();
    thread->setPriority(QThread::HighPriority);
}

CANConnectionContainer::~CANConnectionContainer()
{
    //have to stop the actual execution first before deleting
    delete thread;
    delete connection;
}

CANConnection* CANConnectionContainer::getRef()
{
    return connection;
}
