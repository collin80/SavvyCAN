#include "canfilter.h"

CANFilter::CANFilter()
{
    ID = 0;
    mask = 0;
    bus = -1;
}

void CANFilter::setFilter(uint32_t id, uint32_t mask, int bus)
{
    this->ID = id;
    this->mask = mask;
    this->bus = bus;
}

bool CANFilter::checkFilter(uint32_t id, int bus)
{
    if (bus == -1 || bus == this->bus)
    {
        uint32_t result = id & this->mask;
        if (result == this->ID) return true;
    }
    return false;

}

