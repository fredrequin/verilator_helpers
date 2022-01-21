// Copyright 2013-2022 Frederic Requin
//
// License: BSD
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//   - Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer 
//     in the documentation and/or other materials provided with the 
//     distribution.
//   - Neither the name of the author nor the names of its contributors 
//     may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
//
// Video output:
// -------------
//  - Allows to translate Video output signals from a simulation into BMP files
//  - It is designed to work with "Verilator" (www.veripool.org)
//  - Synchros polarities are configurable
//  - Active and total areas are configurable
//  - HS/VS or DE based scanning
//  - BMP files are saved on VS edge
//  - Support for RGB444, YUV444, YUV422 and YUV420 colorspaces

#ifndef _VIDEO_OUT_H_
#define _VIDEO_OUT_H_

#include "verilated.h"

#define HS_POS_POL (1)
#define HS_NEG_POL (0)
#define VS_POS_POL (2)
#define VS_NEG_POL (0)

class VideoOut
{
    public:
        // Constructor and destructor
        VideoOut(vluint8_t debug, vluint8_t depth, vluint8_t polarity, vluint16_t hoffset, vluint16_t hactive, vluint16_t voffset, vluint16_t vactive, const char *file);
        ~VideoOut();
        // Methods
        bool eval_RGB444_HV(vluint8_t clk, vluint8_t vs,   vluint8_t hs,   vluint8_t red,  vluint8_t green, vluint8_t blue);
        bool eval_RGB444_DE(vluint8_t clk, vluint8_t de,                   vluint8_t red,  vluint8_t green, vluint8_t blue);
        bool eval_YUV444_HV(vluint8_t clk, vluint8_t vs,   vluint8_t hs,   vluint8_t luma, vluint8_t cb,    vluint8_t cr);
        bool eval_YUV444_DE(vluint8_t clk, vluint8_t de,                   vluint8_t luma, vluint8_t cb,    vluint8_t cr);
        bool eval_YUV422_HV(vluint8_t clk, vluint8_t vs,   vluint8_t hs,   vluint8_t luma, vluint8_t chroma);
        bool eval_YUV422_DE(vluint8_t clk, vluint8_t de,                   vluint8_t luma, vluint8_t chroma);
        bool eval_YUV420_DE(vluint8_t clk, vluint8_t de_y, vluint8_t de_c, vluint8_t luma, vluint8_t chroma);
        int  get_hcount();
        int  get_vcount();
    private:
        // BMP file format
        #pragma pack(push, 1)
        typedef struct
        {
            vluint16_t bfType;
            vluint32_t bfSize;
            vluint16_t bfReserved1;
            vluint16_t bfReserved2;
            vluint32_t bfOffBits;
        } BITMAPFILEHEADER;
        typedef struct
        {
            vluint32_t biSize;
            vluint32_t biWidth;
            vluint32_t biHeight;
            vluint16_t biPlanes;
            vluint16_t biBitCount;
            vluint32_t biCompression;
            vluint32_t biSizeImage;
            vluint32_t biXPelsPerMeter;
            vluint32_t biYPelsPerMeter;
            vluint32_t biClrUsed;
            vluint32_t biClrImportant;
        } BITMAPINFOHEADER;
        #pragma pack(pop)
        void        write_bmp();
        void        yuv2rgb(vluint8_t lum, vluint8_t cb, vluint8_t cr, vluint8_t *buf);
        // YUV to RGB tables
        int         u_to_g[256];
        int         u_to_b[256];
        int         v_to_r[256];
        int         v_to_g[256];
        // Temporary variables for YUV422 conversion
        vluint8_t   y0;
        vluint8_t   u0;
        // Temporary buffers for YUV420 conversion
        vluint8_t  *y_buf[4];
        vluint8_t  *c_buf[2];
        // BMP file content
        BITMAPFILEHEADER bfh;
        BITMAPINFOHEADER bih;
        vluint8_t  *row_e;
        vluint8_t  *row_o;
        vluint8_t **img;
        // BMP file name
        char        filename[256];
        int         dump_ctr;
        // Image format
        int         hor_offs;
        int         ver_offs;
        int         hor_size;
        int         ver_size;
        // Horizontal & Vertical counters
        int         hcount;
        int         vcount;
        int         hcount1;
        int         hcount2;
        int         vcount1;
        int         vcount2;
        // Color depth
        int         bit_shift;
        vluint8_t   bit_mask;
        // Previous signal state
        vluint8_t   prev_clk;
        vluint8_t   prev_hs;
        vluint8_t   prev_vs;
        // Synchros polarities
        vluint8_t   hs_pol;
        vluint8_t   vs_pol;
        // First VS encountered
        bool        first_vs;
        // Debug mode
        bool        dbg_on;
        vluint64_t  cycle_ctr;
};

#endif /* _VIDEO_OUT_H_ */
