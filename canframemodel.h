#ifndef CANFRAMEMODEL_H
#define CANFRAMEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVector>
#include <QDebug>
#include <QMutex>
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
    int totalFrameCount();

    void addFrame(CANFrame &, bool);
    void sendRefresh();
    void sendRefresh(int);
    void sendBulkRefresh(int);
    void clearFrames();
    void setDBCHandler(DBCHandler *);
    void setInterpetMode(bool);
    void setOverwriteMode(bool);
    void setHexMode(bool);
    void setFilterState(int ID, bool state);
    void setSecondsMode(bool);
    void loadFilterFile(QString filename);
    void saveFilterFile(QString filename);
    void normalizeTiming();
    void recalcOverwrite();
    bool needsFilterRefresh();
    void insertFrames(const QVector<CANFrame> &newFrames);
    const QVector<CANFrame> *getListReference() const; //thou shalt not modify these frames externally!
    const QVector<CANFrame> *getFilteredListReference() const; //Thus saith the Lord, NO.
    const QMap<int, bool> *getFiltersReference() const; //this neither

signals:
    void updatedFiltersList();

private:
    QVector<CANFrame> frames;
    QVector<CANFrame> filteredFrames;
    QMap<int, bool> filters;
    DBCHandler *dbcHandler;
    QMutex mutex;
    bool interpretFrames; //should we use the dbcHandler?
    bool overwriteDups; //should we display all frames or only the newest for each ID?
    bool useHexMode;
    bool timeSeconds;
    bool needFilterRefresh;
    uint64_t timeOffset;
};


#endif // CANFRAMEMODEL_H

