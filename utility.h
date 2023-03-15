#ifndef UTILITY_H
#define UTILITY_H

#include <Qt>
#include <stdint.h>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QApplication>
#include <QRect>
//#include <QDesktopWidget>

enum TimeStyle
{
    TS_SECONDS,
    TS_MICROS,
    TS_MILLIS,
    TS_CLOCK
};

class Utility
{
public:

    static bool decimalMode;
    static TimeStyle timeStyle;
    static QString timeFormat;

    //determines whether the window position is within any available screens. If it is not we default
    //back to 0,0 which is going to be on screen. This fixes a problem where some operating systems would
    //otherwise let you put windows on a second monitor, disconnect that monitor, and still put windows on it.
    static QPoint constrainedWindowPos(QPoint originalPos)
    {
        QScreen *screen = QGuiApplication::screenAt(originalPos);
        if (!screen)
        {
            return QPoint(0,0);
        }
        return originalPos;
    }

    static QString unQuote(QString inStr)
    {
        QStringList temp;
        temp = inStr.split('\"');
        if (temp.length() >= 3)
            return temp[1];
        return inStr;
    }

    static uint64_t ParseStringToNum(QByteArray input)
    {
        uint64_t temp = 0;

        input = input.toUpper();
        if (input.startsWith("0X") || input.startsWith("X")) //hex number
        {
            if (input.length() < 3) temp = 0;
            else temp = input.right(input.size() - 2).toLongLong(nullptr, 16);
        }
        else if (input.startsWith("0B") || input.startsWith("B")) //binary number
        {
            input = input.right(input.size() - 1); //remove the B
            for (int i = 0; i < input.length(); i++)
            {
                if (input[i] == '1') temp += (uint64_t)1 << (input.length() - i - 1);
            }
        }
        else //decimal number
        {
            temp = input.toLongLong();
        }

        return temp;
    }

    static uint64_t ParseStringToNum(QString input)
    {
        return ParseStringToNum(input.toUtf8());
    }

    static uint ParseStringToNum2(QString pInput, bool* pOk_p = nullptr)
    {
        if(pInput.startsWith("0b"))
        {
            pInput.remove(0, 2);
            return pInput.toUInt(pOk_p, 2);
        }

        return pInput.toUInt(pOk_p, 0);
    }

    static uint64_t GetTimeMS()
    {
        QDateTime stamp = QDateTime::currentDateTime();
        return (((static_cast<uint64_t>(stamp.time().hour()) * 3600ull) + (static_cast<uint64_t>(stamp.time().minute()) * 60ull)
                 + (static_cast<uint64_t>(stamp.time().second())) * 1000ull) + static_cast<uint64_t>(stamp.time().msec()));
    }

    //prints hex numbers in uppercase with 0's filling out the number depending
    //on the size needed. Promotes hex numbers to either 2, 4, or 8 digits
    static QString formatHexNum(uint64_t input)
    {
        if (input < 256)
            return "0x" + QString::number(input, 16).toUpper().rightJustified(2,'0');
        if (input < 65536)
            return "0x" + QString::number(input, 16).toUpper().rightJustified(4,'0');
        if (input < 4294967296)
            return "0x" + QString::number(input, 16).toUpper().rightJustified(8,'0');
        return "0x" + QString::number(input, 16).toUpper().rightJustified(16,'0');
    }

    //uses decimalMode to see if it should show value as decimal or hex
    static QString formatNumber(uint64_t value)
    {
        if (decimalMode)
        {
            return QString::number(value, 10);
        }
        else return formatHexNum(value);
    }

    static QString formatCANID(uint64_t id, bool extended)
    {
        if (decimalMode) return QString::number(id, 10);

        if (extended)
        {
            return "0x" + QString::number(id, 16).toUpper().rightJustified(8,'0');
        }
        else
        {
            id = id & 0x7FF;
            return "0x" + QString::number(id, 16).toUpper().rightJustified(3,'0');
        }
    }

    static QString formatCANID(uint64_t id)
    {
        if (id < 0x800) return formatCANID(id, false);
        return formatCANID(id, true);
    }

    static QString formatByteAsBinary(uint8_t value)
    {
        QString output;
        for (int b = 7; b >= 0; b--)
        {
            if (value & (1 << b)) output += "1";
            else output += "0";
        }
        return output;
    }

    static QString formatByteAsHex(uint8_t value)
    {
        return QString::number(value, 16).toUpper().rightJustified(2,'0');
    }

    static QVariant formatTimestamp(uint64_t timestamp)
    {
        switch (timeStyle)
        {
        case TS_CLOCK:
            return QDateTime::fromMSecsSinceEpoch(timestamp / 1000);
            break;
        case TS_MILLIS:
            return (double)timestamp / 1000.0;
            break;
        case TS_MICROS:
            return (unsigned long long)(timestamp);
            break;
        case TS_SECONDS:
            return (double)timestamp / 1000000.0;
            break;
        }
    }

    //parses the input string to grab as much of it as possible while staying alpha numeric
    static QString grabAlphaNumeric(QString &input)
    {
        QString builder;
        QChar thisChar;
        for (int i = 0; i < input.length(); i++)
        {
            thisChar = input[i];
            if (thisChar.isLetterOrNumber() || thisChar == ':' || thisChar == '~') builder.append(input[i]);
            else
            {
                //qDebug() << "i: "<< i << " len: " << input.length();
                if (i < (input.length() - 1)) input = input.right(input.length() - i);
                else input = "";
                return builder;
            }
        }
        //qDebug() << "Reached end of string in grabAlphaNumeric";
        input = "";
        return builder;
    }

    static QString grabOperation(QString &input)
    {
        QString builder;
        QChar thisChar = input[0];

        if (thisChar == '+' || thisChar == '-' || thisChar == '*' || thisChar == '/' || thisChar == '^' || thisChar == '&' || thisChar == '|' || thisChar == '=' || thisChar == '%')
        {
            input = input.right(input.length() - 1);
            builder = thisChar;
        }
        return builder;
    }

    static int getByteFromBitPosition(int bitPos)
    {
        return bitPos / 8;
    }

    static int getBitFromBitPosition(int bitPos)
    {
        return bitPos & 7;
    }

    //simple linear interpolation between value1 and value2. sample point is 0.0 to 1.0
    static double Lerp(double value1, double value2, double samplePoint)
    {
        return (value1 * (1.0 - samplePoint)) + (value2 * samplePoint);
    }

    /* A unified function that can extract a signal from the (up to) 64 bits of data bytes in a CAN frame
     * handles both little and big endian signals (and floats too).
    */
    static int64_t processIntegerSignal(const QByteArray data, int startBit, int sigSize, bool littleEndian, bool isSigned)
    {

        uint64_t result = 0;
        int bit = 0;

        int maxBytes = (startBit + sigSize) / 8;
        if (data.size() < maxBytes) return 0; //if signal extends past the end of data then abort

        if (littleEndian)
        {
/*
            int currByte = (startBit) / 8;
            int currOffset = startBit - (currByte * 8);
            int remainingBits = qMax(0, (sigSize - (8 - currOffset)) );
            int prevBits = qMin((8 - currOffset), sigSize);
            result = data[currByte] >> currOffset;
            result &= ( (1 << sigSize) - 1); //doesn't hurt to do this even if sigSize is way larger than the # of bits we've got so far

            while (remainingBits > 0)
            {
                currByte++;
                if (remainingBits >= 8) //use this entire byte, its easy
                {
                    result += data[currByte] << prevBits;
                    remainingBits -= 8;
                    prevBits += 8;
                }
                else //use only part of this byte. We're going to need to mask it
                {
                    result += ((data[currByte] & ((1 << remainingBits) - 1) ) << prevBits);
                    remainingBits = 0;
                }
            }*/

            bit = startBit;
            for (int bitpos = 0; bitpos < sigSize; bitpos++)
            {
                if (bit < 512) {
                    int bytePos = bit / 8;
                    if (bytePos >= data.count()) return 0; //error!
                    if (data[bit / 8] & (1 << (bit % 8)))
                        result += (1ULL << bitpos);
                }
                bit++;
            }

        }
        else //motorola / big endian mode
        {
            bit = startBit;
            for (int bitpos = 0; bitpos < sigSize; bitpos++)
            {
                if (bit < 512) {
                    int bytePos = bit / 8;
                    if (bytePos >= data.count()) return 0; //error!
                    if (data[bit / 8] & (1 << (bit % 8)))
                        result += (1ULL << (sigSize - bitpos - 1));
                }

                if ((bit % 8) == 0)
                    bit += 15;
                else bit--;

            }
        }

        if (isSigned)
        {
            uint64_t mask = (1ULL << (sigSize - 1));
            if ((result & mask) == mask) //is the highest bit possible for this signal size set?
            {
                /*
                 * if so we need to also set every bit higher in the result int too.
                 * This leads to the below two lines that are nasty. Here's the theory behind that...
                 * If the value is signed and the highest bit is set then it is negative. To create
                 * a negative value out of this even though the variable result is 64 bit we have to
                 * run 1's all of the way up to bit 63 in result. -1 is all ones for whatever size integer
                 * you have. So, it's 64 1's in this case.
                 * signedMask is done this way:
                 * first you take the signal size and shift 1 up that far. Then subtract one. Lets
                 * see that for a 16 bit signal:
                 * (1 << 16) - 1 = the first 16 bits set as 1's. So far so good. We then negate the whole
                 * thing which flips all bits. Thus signedMask ends up with 1's everwhere that the signal
                 * doesn't take up in the 64 bit signed integer result. Then, result has an OR operation on
                 * it with the old value and -1 masked so that the the 1 bits from -1 don't overwrite bits from the
                 * actual signal. This extends the sign bits out so that the integer result reads as the proper negative
                 * value. We dont need to do any of this if the sign bit wasn't set.
                */
                uint64_t signedMask = ~((1ULL << sigSize) - 1);
                result = (-1LL & signedMask) | result;
                return (int64_t)(result);
            }
        }

        return result;
    }
};

#endif // UTILITY_H
