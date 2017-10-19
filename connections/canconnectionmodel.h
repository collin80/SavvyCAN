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
    virtual ~CANConnectionModel();

    // from abstractmodel:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void add(CANConnection* pConn_p);
    void remove(CANConnection* pConn_p);

    CANConnection* getAtIdx(int, int&) const;
    void refresh(int pIndex=-1);
};

#endif // CANCONNECTIONMODEL_H
