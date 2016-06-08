#ifndef CANCONNECTIONMODEL_H
#define CANCONNECTIONMODEL_H

#include "canconnection.h"
#include "canbus.h"
#include "canconnectioncontainer.h"

#include <QAbstractTableModel>

class CANConnectionContainer;

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

    void addConnection(CANConnectionContainer *conn);
    void removeConnection(CANConnection*);
    void addBus(CANBus &bus);
    void removeBus(int busIdx);
    CANBus* getBus(int bus);
    CANConnection* getConnection(int conn);
    void refreshView();

private:
    QList<CANConnectionContainer *> connections;
    QList<CANBus> buses;

    CANBus *findBusByNum(int bus);
};

#endif // CANCONNECTIONMODEL_H
