#ifndef CANCONCONST_H
#define CANCONCONST_H

namespace CANCon {

    /**
     * @brief The status enum
     */
    enum status
    {
        NOT_CONNECTED,  /*!< device is not connected */
        CONNECTED       /*!< device is connected */
    };

    enum type
    {
        GVRET_SERIAL,
        KVASER,
        SOCKETCAN,
        NONE
    };
}

#endif // CANCONCONST_H
