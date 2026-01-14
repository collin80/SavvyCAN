/*
* gs_usb_functions.h
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

#include "gs_usb_definitions.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool __stdcall candle_list_scan(candle_list_handle *list);
bool __stdcall candle_list_free(candle_list_handle list);
bool __stdcall candle_list_length(candle_list_handle list, uint8_t *len);
bool __stdcall candle_dev_get(candle_list_handle list, uint8_t dev_num, candle_handle *hdev);
bool __stdcall candle_dev_get_state(candle_handle hdev, candle_devstate_t *state);
wchar_t* __stdcall candle_dev_get_path(candle_handle hdev);
bool __stdcall candle_dev_open(candle_handle hdev);
bool __stdcall candle_dev_get_timestamp_us(candle_handle hdev, uint32_t *timestamp_us);
bool __stdcall candle_dev_close(candle_handle hdev);
bool __stdcall candle_dev_free(candle_handle hdev);
bool __stdcall candle_channel_count(candle_handle hdev, uint8_t *num_channels);
bool __stdcall candle_channel_get_capabilities(candle_handle hdev, uint8_t ch, candle_capability_t *cap);
bool __stdcall candle_channel_set_timing(candle_handle hdev, uint8_t ch, candle_bittiming_t *data);
bool __stdcall candle_channel_set_bitrate(candle_handle hdev, uint8_t ch, uint32_t bitrate);
bool __stdcall candle_channel_start(candle_handle hdev, uint8_t ch, uint32_t flags);
bool __stdcall candle_channel_stop(candle_handle hdev, uint8_t ch);
bool __stdcall candle_frame_send(candle_handle hdev, uint8_t ch, candle_frame_t *frame,bool wait_send, uint32_t timeout_ms);
bool __stdcall candle_frame_read(candle_handle hdev, candle_frame_t *frame, uint32_t timeout_ms);
candle_frametype_t __stdcall candle_frame_type(candle_frame_t *frame);
uint32_t __stdcall candle_frame_id(candle_frame_t *frame);
bool __stdcall candle_frame_is_extended_id(candle_frame_t *frame);
bool __stdcall candle_frame_is_rtr(candle_frame_t *frame);
uint8_t __stdcall candle_frame_dlc(candle_frame_t *frame);
uint8_t* __stdcall candle_frame_data(candle_frame_t *frame);
uint32_t __stdcall candle_frame_timestamp_us(candle_frame_t *frame);
candle_err_t __stdcall candle_dev_last_error(candle_handle hdev);

#ifdef __cplusplus
}
#endif