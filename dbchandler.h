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
};

#endif // DBCHANDLER_H
