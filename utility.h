#ifndef UTILITY_H
#define UTILITY_H

#include <QByteArray>
#include <QDateTime>



class Utility
{
public:
    static int ParseStringToNum(QByteArray input)
    {
        int temp = 0;

        input = input.toUpper();
        if (input.startsWith("0X")) //hex number
        {
            temp = input.right(input.size() - 2).toInt(NULL, 16);
        }
        else if (input.startsWith("B")) //binary number
        {
            input = input.right(input.size() - 1); //remove the B
            for (int i = 0; i < input.length(); i++)
            {
                if (input[i] == '1') temp += 1 << (input.length() - i - 1);
            }
        }
        else //decimal number
        {
            temp = input.toInt();
        }

        return temp;
    }

    static int ParseStringToNum(QString input)
    {
        return ParseStringToNum(input.toUtf8());
    }

    static long GetTimeMS()
    {
        QDateTime stamp = QDateTime::currentDateTime();
        return (long)(((stamp.time().hour() * 3600) + (stamp.time().minute() * 60) + (stamp.time().second()) * 1000) + stamp.time().msec());
    }

    //prints hex numbers in uppercase with 0's filling out the number depending
    //on the size needed. Promotes hex numbers to either 2, 4, or 8 digits
    static QString formatHexNum(int input)
    {
        if (input < 256)
            return QString::number(input, 16).toUpper().rightJustified(2,'0');
        if (input < 65536)
            return QString::number(input, 16).toUpper().rightJustified(4,'0');

        return QString::number(input, 16).toUpper().rightJustified(8,'0');
    }
};

#endif // UTILITY_H
