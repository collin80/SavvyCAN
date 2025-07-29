#pragma once

#include <QSpinBox>


class HexSpinBox : public QSpinBox {

public:
    HexSpinBox(QWidget * parent = nullptr) :
        QSpinBox(parent)
    {
        setDisplayIntegerBase(16);
    }

protected:
    // this magic is needed to place minus sign BEFORE '0x' prefix
    QString textFromValue(int value) const override
    {
        QString text = QString::number(qAbs(value), 16);
        if (value < 0) return "-0x" + text;
        else return "0x" + text;
    }

};
