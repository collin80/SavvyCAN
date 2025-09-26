#pragma once

class ISOTP_HANDLER;
class UDS_HANDLER;

class HandlerFactory
{
public:

    // Factory methods
    static ISOTP_HANDLER* createISOTPHandler();
    static UDS_HANDLER* createUDSHandler();
};
