#include <Qt>
#include "qcpaxistickerhex.h"

QString QCPAxisTickerHex::getTickLabel (double tick, const QLocale &locale, QChar formatChar, int precision)
{
    int64_t hexVal = static_cast<int64_t>(tick);
    return QString::number(hexVal, 16);
}
