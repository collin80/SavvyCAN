#ifndef CANDATAITEMDELEGATE_H
#define CANDATAITEMDELEGATE_H

#include <QItemDelegate>
#include "dbc/dbchandler.h"
#include "canframemodel.h"

class CanDataItemDelegate : public QItemDelegate
{
public:
    using QItemDelegate::QItemDelegate;
    CanDataItemDelegate(CANFrameModel *model);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    void buildStringFromFrame(CANFrame frame, QString &tempString) const;
    DBCHandler *dbcHandler;
    CANFrameModel *model;
};

#endif