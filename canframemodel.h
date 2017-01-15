#ifndef CANFRAMEMODEL_H
#define CANFRAMEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVector>
#include <QDebug>
#include <QMutex>
#include "can_structs.h"
#include "dbc/dbchandler.h"
#include "connections/canconnection.h"

class CANFrameModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    CANFrameModel(QObject *parent = 0);
    virtual ~CANFrameModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int columnCount(const QModelIndex &) const;
    int totalFrameCount();    

    void sendRefresh();
    void sendRefresh(int);
    int  sendBulkRefresh();
    void clearFrames();
    void setInterpetMode(bool);
    void setOverwriteMode(bool);
    void setHexMode(bool);
    void setFilterState(unsigned int ID, bool state);
    void setAllFilters(bool state);
    void setSecondsMode(bool);
    void loadFilterFile(QString filename);
    void saveFilterFile(QString filename);
    void normalizeTiming();
    void recalcOverwrite();
    bool needsFilterRefresh();
    void insertFrames(const QVector<CANFrame> &newFrames);
    int getIndexFromTimeID(unsigned int ID, double timestamp);
    const QVector<CANFrame> *getListReference() const; //thou shalt not modify these frames externally!
    const QVector<CANFrame> *getFilteredListReference() const; //Thus saith the Lord, NO.
    const QMap<int, bool> *getFiltersReference() const; //this neither

public slots:
    void addFrame(const CANFrame&, bool);
    void addFrames(const CANConnection*, const QVector<CANFrame>&);

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
    int lastUpdateNumFrames;
    uint32_t preallocSize;
};


#endif // CANFRAMEMODEL_H

