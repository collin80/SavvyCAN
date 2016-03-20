#ifndef CANCONNECTIONMODEL_H
#define CANCONNECTIONMODEL_H

#include "canconnection.h"
#include "canconnectioncontainer.h"

#include <QAbstractTableModel>

class CANConnectionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CANConnectionModel(QObject *parent = 0);

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void addConnection(CANConnection *conn);

private:
    QList<CANConnectionContainer *> connections;
    QList<CAN_Bus> buses;

    CAN_Bus *findBusByNum(int bus);
};

#endif // CANCONNECTIONMODEL_H
