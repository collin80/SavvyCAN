#include <Qt>
#include "qcpaxistickerhex.h"

QString QCPAxisTickerHex::getTickLabel (double tick, const QLocale &locale, QChar formatChar, int precision)
{
    Q_UNUSED(locale);
    Q_UNUSED(formatChar);
    Q_UNUSED(precision);
    int64_t hexVal = static_cast<int64_t>(tick);
    return QString::number(hexVal, 16);
}
