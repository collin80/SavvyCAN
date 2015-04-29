#ifndef CANFRAMEMODEL_H
#define CANFRAMEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "can_structs.h"

class CANFrameModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    CANFrameModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int CANFrameModel::columnCount(const QModelIndex &) const;

    void addFrame(CANFrame &, bool);
    void sendRefresh();
    void sendRefresh(int);
    void clearFrames();


private:
    QList<CANFrame> frames;
};


#endif // CANFRAMEMODEL_H

