#ifndef ISOTP_MESSAGE_H
#define ISOTP_MESSAGE_H

#include "main/can_structs.h"

#include <QVector>
#include <Qt>

// Now a child class of CANFrame. We just add the ability to track how long it
// was supposed to be and other ISOTP related details. But, mostly just CANFrame.
class ISOTP_MESSAGE : public CANFrame
{
public:
  int reportedLength;
  int lastSequence;
  bool isMultiframe;
};

#endif  // ISOTP_MESSAGE_H
