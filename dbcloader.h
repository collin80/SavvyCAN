#ifndef DBCLOADER_H
#define DBCLOADER_H

#include <QObject>
#include "dbc_classes.h"

class DBCLoader : public QObject
{
    Q_OBJECT
public:
    explicit DBCLoader(QObject *parent = 0);

signals:

public slots:
};

#endif // DBCLOADER_H
