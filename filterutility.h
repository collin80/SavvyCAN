#ifndef FILTERUTILITY_H
#define FILTERUTILITY_H

#include <QListWidget>


class FilterUtility
{

public:
    static QListWidgetItem * createFilterItem(uint32_t id, QListWidget* parent=NULL);   // if parent is given, add item automatically to listwidget
    static QListWidgetItem * createCheckableFilterItem(uint32_t id, bool checked, QListWidget* parent=NULL);
    static QListWidgetItem * createBusFilterItem(uint32_t id, QListWidget* parent=NULL);   // if parent is given, add item automatically to listwidget
    static QListWidgetItem * createCheckableBusFilterItem(uint32_t id, bool checked, QListWidget* parent=NULL);

    static uint32_t getIdAsInt( QListWidgetItem * item );
    static QString getId( QListWidgetItem * item );
    static QString getId( QString itemText );

    static uint32_t getGMLanArbitrationId(int32_t id);
    static uint32_t getGMLanSenderId(int32_t id);
    static uint32_t getGMLanPriorityBits(int32_t id);
};

#endif // DBCLOADSAVEWINDOW_H
