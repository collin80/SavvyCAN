#ifndef QCPAXISTICKERHEX_H
#define QCPAXISTICKERHEX_H

#include "qcustomplot.h"

class QCPAxisTickerHex: public QCPAxisTicker
{
public:
    QString getTickLabel (double tick, const QLocale &locale, QChar formatChar, int precision);
};

#endif // QCPAXISTICKERHEX_H
