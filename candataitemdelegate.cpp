

#include "candataitemdelegate.h"
#include <QPainter>
#include <QDebug>
#include "can_structs.h"

#define DATA_ITEM_LEFT_PADDING 5

CanDataItemDelegate::CanDataItemDelegate(CANFrameModel *_model) : QItemDelegate() {

    dbcHandler = DBCHandler::getReference();
    model = _model;
}

void CanDataItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const
{
    const auto frame = index.data().value<CANFrame>();
    const unsigned char *data = reinterpret_cast<const unsigned char *>(frame.payload().constData());

    bool overwriteDups = model->getOverwriteMode();
    bool markChangedBytes = model->getMarkChangedBytes();
    int bytesPerLine = model->getBytesPerLine();
    bool useHexMode = model->getHexMode();

    // cache the painter state so it can be restored after this item is done painting
    painter->save();
    QStyleOptionViewItem opt = setOptions(index, option);
    drawBackground(painter, opt, index);
    
    auto pen = painter->pen();
    const auto defaultColor = pen.color();

    // FontMetrics are used to measure the bounding rects for drawn text, allowing 
    // us to properly position subsequent draw calls
    const auto fm = painter->fontMetrics();
    
    int dataLen = frame.payload().count();
    if (dataLen < 0) dataLen = 0;
    if (frame.frameType() != QCanBusFrame::RemoteRequestFrame) {
        QRect boundingRect;
        QPoint startPoint = option.rect.topLeft();
        const auto charBounds = fm.boundingRect("0");
        int x = startPoint.x() + DATA_ITEM_LEFT_PADDING;
        int y = startPoint.y() + charBounds.height();

        // Draw each data byte manually, advancing the start point as we go
        // This allows us to change the styling of individual draw calls.
        // We use this to highlight data bytes that have changed since the last
        // frame when overwriteDups mode is on.
        for (int i = 0; i < dataLen; i++)
        {
            if((frame.changedPayloadBytes & (1 << i)) && overwriteDups && markChangedBytes) {
                painter->setPen(Qt::red);
            }
            else {
                painter->setPen(defaultColor);
            }

            QString dataStr = 
                useHexMode ? 
                    QString::number(data[i], 16).toUpper().rightJustified(2, '0') : 
                    QString::number(data[i], 10);
            const auto bounds = fm.boundingRect(dataStr);

            painter->drawText(x, y, dataStr);

            x += bounds.width();

            if (!((i+1) % bytesPerLine) && (i != (dataLen - 1))) {
                // Wrap to next line
                y += bounds.height();
                x = startPoint.x() + DATA_ITEM_LEFT_PADDING;
            }
            else {
                // Advance a character width for a space
                x += charBounds.width();
            }
        }

        painter->setPen(defaultColor);

        QString tempString;
        buildStringFromFrame(frame, tempString);

        if (tempString.length())
            painter->drawText(x, y, tempString);
    }
    
    drawFocus(painter, opt, option.rect);
    painter->restore();
}

void CanDataItemDelegate::buildStringFromFrame(CANFrame frame, QString &tempString) const {
    bool interpretFrames = model->getInterpretMode();
    bool overwriteDups = model->getOverwriteMode();
    tempString.append("");
    if (frame.frameType() == frame.ErrorFrame)
    {
        if (frame.error() & frame.TransmissionTimeoutError) tempString.append("\nTX Timeout");
        if (frame.error() & frame.LostArbitrationError) tempString.append("\nLost Arbitration");
        if (frame.error() & frame.ControllerError) tempString.append("\nController Error");
        if (frame.error() & frame.ProtocolViolationError) tempString.append("\nProtocol Violation");
        if (frame.error() & frame.TransceiverError) tempString.append("\nTransceiver Error");
        if (frame.error() & frame.MissingAcknowledgmentError) tempString.append("\nMissing ACK");
        if (frame.error() & frame.BusOffError) tempString.append("\nBus OFF");
        if (frame.error() & frame.BusError) tempString.append("\nBus ERR");
        if (frame.error() & frame.ControllerRestartError) tempString.append("\nController restart err");
        if (frame.error() & frame.UnknownError) tempString.append("\nUnknown error type");
    }
    //TODO: technically the actual returned bytes for an error frame encode some more info. Not interpreting it yet.

    //now, if we're supposed to interpret the data and the DBC handler is loaded then use it
    if ( (dbcHandler != nullptr) && interpretFrames && (frame.frameType() == frame.DataFrame) )
    {
        DBC_MESSAGE *msg = dbcHandler->findMessage(frame);
        if (msg != nullptr)
        {
            tempString.append("   <" + msg->name + ">\n");
            if (msg->comment.length() > 1) tempString.append(msg->comment + "\n");
            for (int j = 0; j < msg->sigHandler->getCount(); j++)
            {                        
                QString sigString;
                DBC_SIGNAL* sig = msg->sigHandler->findSignalByIdx(j);

                if ( (sig->multiplexParent == nullptr) && sig->processAsText(frame, sigString))
                {
                    tempString.append(sigString);
                    tempString.append("\n");
                    if (sig->isMultiplexor)
                    {
                        qDebug() << "Multiplexor. Diving into the tree";
                        tempString.append(sig->processSignalTree(frame));
                    }
                }
                else if (sig->isMultiplexed && overwriteDups) //wasn't in this exact frame but is in the message. Use cached value
                {
                    bool isInteger = false;
                    if (sig->valType == UNSIGNED_INT || sig->valType == SIGNED_INT) isInteger = true;
                    tempString.append(sig->makePrettyOutput(sig->cachedValue.toDouble(), sig->cachedValue.toLongLong(), true, isInteger));
                    tempString.append("\n");
                }
            }
        }
    }
}