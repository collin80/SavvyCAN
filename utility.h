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
};

#endif // UTILITY_H
