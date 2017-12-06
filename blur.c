/*
 * Copyright Â© 2008 Kristian HÃ¸gsberg
 * Copyright Â© 2009 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <math.h>
#include "blur.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

/* Performs a simple 2D Gaussian blur of radius @radius on surface @surface. */
void
blur_image_surface (cairo_surface_t *surface, int radius)
{
    cairo_surface_t *tmp;
    int width, height;
    uint32_t *src, *dst;

    if (cairo_surface_status (surface))
    return;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    switch (cairo_image_surface_get_format (surface)) {
    case CAIRO_FORMAT_A1:
    default:
    /* Don't even think about it! */
    return;

    case CAIRO_FORMAT_A8:
    /* Handle a8 surfaces by effectively unrolling the loops by a
     * factor of 4 - this is safe since we know that stride has to be a
     * multiple of uint32_t. */
    width /= 4;
    break;

    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    break;
    }

    tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (tmp))
    return;

    src = (uint32_t*)cairo_image_surface_get_data (surface);

    dst = (uint32_t*)cairo_image_surface_get_data (tmp);
    
#ifdef __SSE3__
    blur_impl_ssse3(src, dst, width, height, 4.5);
#elif __SSE2__
    blur_impl_sse2(src, dst, width, height, 4.5);
#else
    int src_stride = cairo_image_surface_get_stride (surface);
    int dst_stride = cairo_image_surface_get_stride (tmp);
    blur_impl_naive(src, dst, width, height, src_stride, dst_stride, 10000);
#endif
    cairo_surface_destroy (tmp);
    cairo_surface_flush (surface);
    cairo_surface_mark_dirty (surface);
}

void blur_impl_naive(uint32_t* _src, uint32_t* _dst, int width, int height, int src_stride, int dst_stride, int radius)
{
    int x, y, z, w;
    uint32_t *s, *d, a, p;
    int i, j, k;
    uint8_t kernel[17];
    const int size = ARRAY_LENGTH (kernel);
    const int half = size / 2;

    uint8_t *src = (uint8_t*)_src;
    uint8_t *dst = (uint8_t*)_dst;

    a = 0;
    for (i = 0; i < size; i++) {
    double f = i - half;
    a += kernel[i] = exp (- f * f / 30.0) * 80;
    }

    /* Horizontally blur from surface -> tmp */
    for (i = 0; i < height; i++) {
    s = (uint32_t *) (src + i * src_stride);
    d = (uint32_t *) (dst + i * dst_stride);
    for (j = 0; j < width; j++) {
        if (radius < j && j < width - radius) {
        d[j] = s[j];
        continue;
        }

        x = y = z = w = 0;
        for (k = 0; k < size; k++) {
        if (j - half + k < 0 || j - half + k >= width)
            continue;

        p = s[j - half + k];

        x += ((p >> 24) & 0xff) * kernel[k];
        y += ((p >> 16) & 0xff) * kernel[k];
        z += ((p >>  8) & 0xff) * kernel[k];
        w += ((p >>  0) & 0xff) * kernel[k];
        }
        d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
    }
    }

    /* Then vertically blur from tmp -> surface */
    for (i = 0; i < height; i++) {
    s = (uint32_t *) (dst + i * dst_stride);
    d = (uint32_t *) (src + i * src_stride);
    for (j = 0; j < width; j++) {
        if (radius <= i && i < height - radius) {
        d[j] = s[j];
        continue;
        }

        x = y = z = w = 0;
        for (k = 0; k < size; k++) {
        if (i - half + k < 0 || i - half + k >= height)
            continue;

        s = (uint32_t *) (dst + (i - half + k) * dst_stride);
        p = s[j];

        x += ((p >> 24) & 0xff) * kernel[k];
        y += ((p >> 16) & 0xff) * kernel[k];
        z += ((p >>  8) & 0xff) * kernel[k];
        w += ((p >>  0) & 0xff) * kernel[k];
        }
        d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
    }
    }
}
