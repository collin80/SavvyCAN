#ifndef CANFRAMEMODEL_H
#define CANFRAMEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVector>
#include <QDebug>
#include "can_structs.h"
#include "dbchandler.h"

class CANFrameModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    CANFrameModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int columnCount(const QModelIndex &) const;

    void addFrame(CANFrame &, bool);
    void sendRefresh();
    void sendRefresh(int);
    void sendBulkRefresh(int);
    void clearFrames();
    void setDBCHandler(DBCHandler *);
    void setInterpetMode(bool);
    void setOverwriteMode(bool);
    void normalizeTiming();
    void recalcOverwrite();
    QVector<CANFrame> *getListReference();


private:
    QVector<CANFrame> frames;
    DBCHandler *dbcHandler;
    QMutex mutex;
    bool interpretFrames; //should we use the dbcHandler?
    bool overwriteDups; //should we display all frames or only the newest for each ID?
    uint64_t timeOffset;
};


#endif // CANFRAMEMODEL_H

