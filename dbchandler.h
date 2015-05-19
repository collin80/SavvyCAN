#ifndef DBCHANDLER_H
#define DBCHANDLER_H

#include <QObject>
#include "dbc_classes.h"

class DBCHandler : public QObject
{
    Q_OBJECT
public:
    explicit DBCHandler(QObject *parent = 0);
    void loadDBCFile(QString);

signals:

public slots:

private:
    QList<DBC_NODE> dbc_nodes;
    QList<DBC_MESSAGE> dbc_messages;

    DBC_NODE *findNodeByName(QString name);
    DBC_MESSAGE *findMsgByID(int id);
    DBC_SIGNAL *findSignalByName(DBC_MESSAGE *msg, QString name);
};

#endif // DBCHANDLER_H
