#ifndef ISOTP_MESSAGE_H
#define ISOTP_MESSAGE_H

#include <Qt>
#include <QVector>
#include <can_structs.h>

//Now a child class of CommFrame. We just add the ability to track how long it was supposed to be and other
//ISOTP related details. But, mostly just CommFrame.
class ISOTP_MESSAGE : public CommFrame
{
public:
    int reportedLength = 0;
    int lastSequence = 0;
    bool isMultiframe = false;
};

#endif // ISOTP_MESSAGE_H
