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
    void saveDBCFile(QString);
    void listDebugging();
    QString processSignal(const CANFrame &frame, const DBC_SIGNAL &sig);
    DBC_NODE *findNodeByName(QString name);
    DBC_NODE *findNodeByIdx(int idx);

    DBC_MESSAGE *findMsgByID(int id);
    DBC_MESSAGE *findMsgByIdx(int idx);
    DBC_MESSAGE *findMsgByName(QString name);

    DBC_SIGNAL *findSignalByName(DBC_MESSAGE *msg, QString name);
    DBC_SIGNAL *findSignalByIdx(DBC_MESSAGE *msg, int idx);

    QList<DBC_NODE> dbc_nodes;
    QList<DBC_MESSAGE> dbc_messages;

signals:

public slots:

private:

    unsigned char reverseBits(unsigned char);
    unsigned char processByte(unsigned char, int, int);
};

#endif // DBCHANDLER_H
