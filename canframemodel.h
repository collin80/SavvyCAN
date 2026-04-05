#ifndef COMMFRAMEMODEL_H
#define COMMFRAMEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QVector>
#include <QDebug>
#include <QMutex>
#include "can_structs.h"
#include "dbc/dbchandler.h"
#include "connections/canconnection.h"
#include "utility.h"

enum class Column {
    TimeStamp = 0, ///< The timestamp when the frame was transmitted or received
    FrameId   = 1, ///< The frames CAN identifier (Standard: 11 or Extended: 29 bit)
    Type      = 2, ///< Type of packet (CAN, RemoteReq, Err, CANFD, LIN, FlexRay) + X if extended (CAN/FD)
    FrameCount= 3, ///< Number of frames of this ID that have come in (only when in overwrite mode)
    Direction = 4, ///< Whether the frame was transmitted or received
    Bus       = 5, ///< The bus where the frame was transmitted or received
    Length    = 6, ///< The frames payload data length
    ASCII     = 7, ///< The payload interpreted as ASCII characters
    Data      = 8, ///< The frames payload data
    NUM_COLUMN
};

class CommFrameModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    CommFrameModel(QObject *parent = 0);
    virtual ~CommFrameModel();

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
    void setInterpretMode(bool);
    bool getInterpretMode();
    void setOverwriteMode(bool);
    void setHexMode(bool);
    void setClearMode(bool mode);
    void setTimeStyle(TimeStyle newStyle);
    void setIgnoreDBCColors(bool mode);
    void setFilterState(unsigned int ID, bool state);
    void setBusFilterState(unsigned int BusID, bool state);
    void setAllFilters(bool state);
    void setTimeFormat(QString);
    void setBytesPerLine(int bpl);
    void loadFilterFile(QString filename);
    void saveFilterFile(QString filename);
    void normalizeTiming();
    void recalcOverwrite();
    bool needsFilterRefresh();
    void insertFrames(const QVector<CommFrame> &newFrames);
    void sortByColumn(int column);
    int getIndexFromTimeID(unsigned int ID, double timestamp);
    const QVector<CommFrame> *getListReference() const; //thou shalt not modify these frames externally!
    const QVector<CommFrame> *getFilteredListReference() const; //Thus saith the Lord, NO.
    const QMap<int, bool> *getFiltersReference() const; //this neither
    const QMap<int, bool> *getBusFiltersReference() const; //this neither

public slots:
    void addFrame(const CommFrame&, bool);
    void addFrames(const CANConnection*, const QVector<CommFrame>&);

signals:
    void updatedFiltersList();

private:
    void qSortCommFrameAsc(QVector<CommFrame>* frames, Column column, int lowerBound, int upperBound);
    void qSortCommFrameDesc(QVector<CommFrame>* frames, Column column, int lowerBound, int upperBound);
    uint64_t getCommFrameVal(QVector<CommFrame> *frames, int row, Column col);
    bool any_filters_are_configured(void);
    bool any_busfilters_are_configured(void);

    QVector<CommFrame> frames;
    QVector<CommFrame> filteredFrames;
    QMap<int, bool> filters;
    QMap<int, bool> busFilters;
    DBCHandler *dbcHandler;
    QMutex mutex;
    bool interpretFrames; //should we use the dbcHandler?
    bool overwriteDups; //should we display all frames or only the newest for each ID?
    bool filtersPersistDuringClear;
    QString timeFormat;
    TimeStyle timeStyle;
    bool useHexMode;
    bool needFilterRefresh;
    bool ignoreDBCColors;
    int64_t timeOffset;
    int lastUpdateNumFrames;
    uint32_t preallocSize;
    bool sortDirAsc;
    int bytesPerLine;
};


#endif // CommFrameMODEL_H

