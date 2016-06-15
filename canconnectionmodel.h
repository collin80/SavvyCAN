#ifndef CANCONNECTIONMODEL_H
#define CANCONNECTIONMODEL_H


#include <QAbstractTableModel>

#include "canbus.h"
#include "connections/canconnection.h"
#include "connectionwindow.h"


class CANConnectionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CANConnectionModel(QObject *parent = 0);

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void add(CANConnection* pConn_p);
    void remove(CANConnection* pConn_p);

    QList<CANConnection*>& getConnections();
    CANConnection* getAtIdx(int, int&) const;
    void refreshView();

private:
    QList<CANConnection*> mConns;
};

#endif // CANCONNECTIONMODEL_H
