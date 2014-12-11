#include <stdint.h>

#include "el_std.h"    // xw

/* Define these to something appropriate in your build */
EL_STATIC const int32_t yuv2rgb565_table[];

void yuv420_2_rgb565(uint16_t *dst_ptr,
               const uint8_t  *y_ptr,
               const uint8_t  *u_ptr,
               const uint8_t  *v_ptr,
                     int32_t   width,
                     int32_t   height,
                     int32_t   y_span,
                     int32_t   uv_span,
                     int32_t   dst_span,
               const int32_t  *tables,
                     int32_t   dither);

static void el_rgb565_convert(uint8_t *dst_ptr, const uint8_t *y_ptr, int32_t pic_width, int32_t pic_height)
{
    int32_t   y_buf_len = pic_width * pic_height;
    const uint8_t  *u_ptr = y_ptr + y_buf_len;
    const uint8_t  *v_ptr = u_ptr + (y_buf_len >> 2);

    yuv420_2_rgb565(
                    (uint16_t *)dst_ptr,
                    y_ptr,
                    u_ptr,
                    v_ptr,
                    pic_width,
                    pic_height,
                    pic_width,     // y_span
                    (pic_width >> 1),
                    (pic_width << 1),
                    yuv2rgb565_table,
                    1
                );
}


