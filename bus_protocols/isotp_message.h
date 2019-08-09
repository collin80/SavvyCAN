#ifndef ISOTP_MESSAGE_H
#define ISOTP_MESSAGE_H

//the same as the CANFrame struct but with arbitrary data size.
class ISOTP_MESSAGE
{
public:
    uint32_t ID;
    int bus;
    bool extended;
    bool isReceived;
    int len; //# of bytes this message should have (as reported)
    int actualSize; //# we actually got
    QVector<unsigned char> data;
    uint64_t timestamp;
};

#endif // ISOTP_MESSAGE_H
