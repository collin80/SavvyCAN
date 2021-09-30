#ifndef SNIFFERMODEL_H
#define SNIFFERMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QTimer>

#include "can_structs.h"
#include "connections/canconnection.h"
#include "snifferitem.h"


enum fltType
{
    ALL,
    ADD,
    REMOVE,
    NONE
};

class SnifferModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SnifferModel(QObject *parent = 0);
    virtual ~SnifferModel();

    /* from QAbstractItemModel */
    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex parent(const QModelIndex &index) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    void refresh();
    void clear();
    void filter(fltType pType, int pId=0);
    bool getNeverExpire();
    bool getFadeInactive();
    bool getMuteNotched();
    void setNeverExpire(bool val);
    void setFadeInactive(bool val);
    void setMuteNotched(bool val);
    void setExpireInterval(int newVal);
    void updateNotchPoint();


public slots:
    void update(CANConnection*, QVector<CANFrame>&);
    void notch();
    void unNotch();

signals:
    void idChange(int, bool);

private:
    QMap<quint32, SnifferItem*> mMap;
    QMap<quint32, SnifferItem*> mFilters;
    bool                        mFilter;
    bool                        mNeverExpire;
    bool                        mFadeInactive;
    bool                        mMuteNotched;
    bool                        mDarkMode;
    quint32                     mTimeSequence;
    quint32                     mExpireInterval;
};

#endif // SNIFFERMODEL_H
