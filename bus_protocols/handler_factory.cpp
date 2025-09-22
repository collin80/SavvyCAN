#include "handler_factory.h"
#include "isotp_handler.h"
#include "connections/canconmanager.h"
#include "mainwindow.h"

ISOTP_HANDLER* HandlerFactory::createISOTPHandler()
{
    return new ISOTP_HANDLER(*MainWindow::getReference()->getCANFrameModel()->getListReference(),
            /* canSendCallback */ [](const CANFrame& pFrame){ CANConManager::getInstance()->sendFrame(pFrame); },
            /* getNoOfBusesCallback */ [](void){  return CANConManager::getInstance()->getNumBuses(); },
            /* pendingConn */ [](ISOTP_HANDLER* handler) -> QMetaObject::Connection {
            return QObject::connect(
                    CANConManager::getInstance(),
                    &CANConManager::framesReceived,
                    handler,
                    [handler](void*, const QVector<CANFrame>& frames){
                    handler->rapidFrames(frames);
                    });
            });
}
