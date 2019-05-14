#ifndef CANCONNECTIONMODEL_H
#define CANCONNECTIONMODEL_H


#include <QAbstractTableModel>

#include "canbus.h"

class CANConnection;

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

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void add(CANConnection* pConn_p);
    void remove(CANConnection* pConn_p);
    void replace(int idx , CANConnection* pConn_p);

    CANConnection* getAtIdx(int) const;
    void refresh(int pIndex=-1);
};

#endif // CANCONNECTIONMODEL_H
