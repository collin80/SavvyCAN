/*
* gs_usb_definitions.h
*
* Ben Evans <ben@canbusdebugger.com>
*
* MIT License
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

/*
* NOTE REGARDING NAMING
*
* I'd like to change the name of the candle_dll repo to gs_usb_dll to be more generic as this driver
* should support all devices following the gs_usb driver protocol. However, I also don't want to take
* away from the original authors contribution to that code and so for now will keep the name.
*
* If development continues to add FD support etc. to the driver code, I'll make the name switch then
* and adjust all these function names/defines.
*
* https://github.com/BennyEvans/candle_dll
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CANDLE_ID_EXTENDED                  (0x80000000U)
#define CANDLE_ID_RTR                       (0x40000000U)
#define CANDLE_ID_ERROR                     (0x20000000U)

typedef enum {
    CANDLE_MODE_NORMAL                      = 0x00,
    CANDLE_MODE_LISTEN_ONLY                 = 0x01,
    CANDLE_MODE_LOOP_BACK                   = 0x02,
    CANDLE_MODE_TRIPLE_SAMPLE               = 0x04,
    CANDLE_MODE_ONE_SHOT                    = 0x08,
    CANDLE_MODE_HW_TIMESTAMP                = 0x10,
    CANDLE_MODE_PAD_PKTS_TO_MAX_PKT_SIZE    = 0x80
} candle_mode_t;

typedef enum {
    CANDLE_ERR_OK                           =  0,
    CANDLE_ERR_CREATE_FILE                  =  1,
    CANDLE_ERR_WINUSB_INITIALIZE            =  2,
    CANDLE_ERR_QUERY_INTERFACE              =  3,
    CANDLE_ERR_QUERY_PIPE                   =  4,
    CANDLE_ERR_PARSE_IF_DESCR               =  5,
    CANDLE_ERR_SET_HOST_FORMAT              =  6,
    CANDLE_ERR_GET_DEVICE_INFO              =  7,
    CANDLE_ERR_GET_BITTIMING_CONST          =  8,
    CANDLE_ERR_PREPARE_READ                 =  9,
    CANDLE_ERR_SET_DEVICE_MODE              = 10,
    CANDLE_ERR_SET_BITTIMING                = 11,
    CANDLE_ERR_BITRATE_FCLK                 = 12,
    CANDLE_ERR_BITRATE_UNSUPPORTED          = 13,
    CANDLE_ERR_SEND_FRAME                   = 14,
    CANDLE_ERR_READ_TIMEOUT                 = 15,
    CANDLE_ERR_READ_WAIT                    = 16,
    CANDLE_ERR_READ_RESULT                  = 17,
    CANDLE_ERR_READ_SIZE                    = 18,
    CANDLE_ERR_SETUPDI_IF_DETAILS           = 19,
    CANDLE_ERR_SETUPDI_IF_DETAILS2          = 20,
    CANDLE_ERR_MALLOC                       = 21,
    CANDLE_ERR_PATH_LEN                     = 22,
    CANDLE_ERR_CLSID                        = 23,
    CANDLE_ERR_GET_DEVICES                  = 24,
    CANDLE_ERR_SETUPDI_IF_ENUM              = 25,
    CANDLE_ERR_SET_TIMESTAMP_MODE           = 26,
    CANDLE_ERR_DEV_OUT_OF_RANGE             = 27,
    CANDLE_ERR_GET_TIMESTAMP                = 28,
    CANDLE_ERR_SET_PIPE_RAW_IO              = 29,
    CANDLE_ERR_DEVICE_FEATURE_UNAVAILABLE   = 30,
    CANDLE_ERR_WRITE_WAIT                   = 31,
    CANDLE_ERR_WRITE_TIMEOUT                = 32,
    CANDLE_ERR_WRITE_RESULT                 = 33,
    CANDLE_ERR_WRITE_SIZE                   = 34,
    CANDLE_ERR_BUFFER_LEN                   = 35
} candle_err_t;

typedef enum {
    CANDLE_DEVSTATE_AVAIL,
    CANDLE_DEVSTATE_INUSE
} candle_devstate_t;

typedef enum {
    CANDLE_FRAMETYPE_UNKNOWN,
    CANDLE_FRAMETYPE_RECEIVE,
    CANDLE_FRAMETYPE_ECHO,
    CANDLE_FRAMETYPE_ERROR,
    CANDLE_FRAMETYPE_TIMESTAMP_OVFL
} candle_frametype_t;

// Pack structures
#pragma pack(push,1)
typedef struct {
    uint32_t echo_id;
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t channel;
    uint8_t flags;
    uint8_t _reserved;
    uint8_t data[8];
    uint32_t timestamp_us;
} candle_frame_t;

typedef struct {
    uint32_t feature;
    uint32_t device_clock;
    uint32_t tseg1_min;
    uint32_t tseg1_max;
    uint32_t tseg2_min;
    uint32_t tseg2_max;
    uint32_t sjw_max;
    uint32_t brp_min;
    uint32_t brp_max;
    uint32_t brp_inc;
} candle_capability_t;

typedef struct {
    uint32_t prop_seg;
    uint32_t phase_seg1;
    uint32_t phase_seg2;
    uint32_t sjw;
    uint32_t brp;
} candle_bittiming_t;
#pragma pack(pop)

typedef void* candle_list_handle;
typedef void* candle_handle;

#ifdef __cplusplus
}
#endif