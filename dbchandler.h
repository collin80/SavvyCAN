#ifndef DBCHANDLER_H
#define DBCHANDLER_H

#include <QObject>
#include "dbc_classes.h"
#include "can_structs.h"

/*
 * For ease of quick testing and development this is all run together.
 * It should be decoupled once the functionality is confirmed.
*/

class DBCHandler : public QObject
{
    Q_OBJECT
public:
    explicit DBCHandler(QObject *parent = 0);
    void loadDBCFile(QString);
    void listDebugging();
    void processSignal(CANFrame *frame, DBC_SIGNAL *sig);
    DBC_NODE *findNodeByName(QString name);
    DBC_MESSAGE *findMsgByID(int id);
    DBC_SIGNAL *findSignalByName(DBC_MESSAGE *msg, QString name);

signals:

public slots:

private:
    QList<DBC_NODE> dbc_nodes;
    QList<DBC_MESSAGE> dbc_messages;

};

#endif // DBCHANDLER_H
