#include "utility.h"
#include "filterutility.h"
#include "dbc/dbchandler.h"
#include <QSettings>

uint32_t FilterUtility::getIdAsInt( QListWidgetItem * item )
{
    return Utility::ParseStringToNum(getId(item));
}

QString FilterUtility::getId( QString itemText )
{
    if (itemText.contains(" "))
        // Strip away the filter label
        return itemText.left(itemText.indexOf(" "));
    else
        return itemText;
}

QString FilterUtility::getId( QListWidgetItem * item )
{
    return getId(item->text());
}

uint32_t FilterUtility::getGMLanArbitrationId(int32_t id)
{
    return (id  >> 13) & 0x1FFF;
}

uint32_t FilterUtility::getGMLanPriorityBits(int32_t id)
{
    return (id  >> 26) & 0x7;
}

uint32_t FilterUtility::getGMLanSenderId(int32_t id)
{
    return id & 0x1FFF;
}

QListWidgetItem * FilterUtility::createCheckableFilterItem(uint32_t id, bool checked, QListWidget* parent)
{
    QListWidgetItem * thisItem = createFilterItem(id,parent);
    thisItem->setFlags(thisItem->flags() | Qt::ItemIsUserCheckable);
    if (checked) 
        thisItem->setCheckState(Qt::Checked);
    else 
        thisItem->setCheckState(Qt::Unchecked);
    return thisItem;
}

QListWidgetItem * FilterUtility::createCheckableBusFilterItem(uint32_t id, bool checked, QListWidget* parent)
{
    QListWidgetItem * thisItem = createBusFilterItem(id,parent);
    thisItem->setFlags(thisItem->flags() | Qt::ItemIsUserCheckable);
    if (checked)
        thisItem->setCheckState(Qt::Checked);
    else
        thisItem->setCheckState(Qt::Unchecked);
    return thisItem;
}


QListWidgetItem * FilterUtility::createFilterItem(uint32_t id, QListWidget* parent)
{
    QSettings settings;
    DBCHandler * dbcHandler = DBCHandler::getReference();
    QListWidgetItem *thisItem = new QListWidgetItem(parent);
    QString filterItemName = Utility::formatCANID(id);

    //Note, there are multiple filter labeling preferences. There is one in main settings to globally
    //enable or disable them all. Then each loaded DBC file also can be selected on/off
    //Both must be enabled for you to see labeling.
    if (settings.value("Main/FilterLabeling", false).toBool())
    {
        // Filter labeling (show interpreted frame names next to the CAN addr ID)
        MatchingCriteria_t matchingCriteria;
        DBC_MESSAGE *msg = dbcHandler->findMessageForFilter(id,&matchingCriteria);
        if (msg != nullptr)
        {
            filterItemName.append(" ");
            filterItemName.append(msg->name);

            // Create tooltip to show the whole name just in case it's too long to fit in the filter window.
            // Also if the matching criteria is set to GMLAN, show the Arbitration ID as well
            QString tooltip;
            if (matchingCriteria == GMLAN)
                tooltip.append("0x" + QString::number(FilterUtility::getGMLanArbitrationId(id), 16).toUpper().rightJustified(4,'0') + ": ");
            tooltip.append(msg->name);
            thisItem->setToolTip(tooltip);
        }
    }

    thisItem->setText(filterItemName);
    return thisItem;
}

QListWidgetItem * FilterUtility::createBusFilterItem(uint32_t id, QListWidget* parent)
{
    QSettings settings;
    DBCHandler * dbcHandler = DBCHandler::getReference();
    QListWidgetItem *thisItem = new QListWidgetItem(parent);
    QString filterItemName = QStringLiteral("%1").arg(id);

    if (settings.value("Main/FilterLabeling", false).toBool())
    {
        // Filter labeling (show interpreted frame names next to the CAN addr ID)
        MatchingCriteria_t matchingCriteria;
        DBC_MESSAGE *msg = dbcHandler->findMessageForFilter(id,&matchingCriteria);
        if (msg != NULL)
        {
            filterItemName.append(" ");
            filterItemName.append(msg->name);
        }
    }

    thisItem->setText(filterItemName);
    return thisItem;
}
