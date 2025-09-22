#pragma once

class ISOTP_HANDLER;

class HandlerFactory
{
public:

    // Factory methods
    static ISOTP_HANDLER* createISOTPHandler();
};
