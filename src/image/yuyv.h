#pragma once
#include <cstdint>

// Convert a w x h YUYV (YUY2) image to (w/2) x (h/2) packed RGB, averaging each 2x2 block
// (equivalent to OpenCV INTER_AREA at 0.5x). BT.601 limited range, integer fixed point.
// w and h must be even.
void yuyv_to_rgb_half(const uint8_t* yuyv, int w, int h, uint8_t* rgb_out);

// Convert a w x h YUYV image to w x h packed RGB (no scaling). Same BT.601 limited-range
// fixed-point math as yuyv_to_rgb_half; each pixel keeps its own Y, chroma is per pair.
// w must be even.
void yuyv_to_rgb(const uint8_t* yuyv, int w, int h, uint8_t* rgb_out);
