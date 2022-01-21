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

#include "verilated.h"
#include "video_out.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Constructor
VideoOut::VideoOut(vluint8_t debug, vluint8_t depth, vluint8_t polarity, vluint16_t hoffset, vluint16_t hactive, vluint16_t voffset, vluint16_t vactive, const char *file)
{
    // color depth
    if (depth <= 8)
    {
        bit_mask  = (1 << depth) - 1;
        bit_shift = (int)(8 - depth);
    }
    else
    {
        bit_mask  = (vluint8_t)0xFF;
        bit_shift = (int)0;
    }
    // synchros polarities
    hs_pol              = (polarity & HS_POS_POL) ? (vluint8_t)1 : (vluint8_t)2;
    vs_pol              = (polarity & VS_POS_POL) ? (vluint8_t)1 : (vluint8_t)2;
    // screen format initialized
    hor_offs            = (int)hoffset;
    hor_size            = (int)hactive;
    ver_offs            = (int)voffset;
    ver_size            = (int)vactive;
    // debug mode
    dbg_on              = (debug) ? true : false;
    cycle_ctr           = (vluint64_t)0;
    // initialize BMP headers
    bih.biSize          = sizeof(BITMAPINFOHEADER);
    bih.biWidth         = (vluint32_t)hactive;
    bih.biHeight        = (vluint32_t)vactive;
    bih.biSizeImage     = (vluint32_t)hactive * vactive * 3;
    bih.biPlanes        = (vluint16_t)1;
    bih.biBitCount      = (vluint16_t)24;
    bih.biCompression   = 0; // BI_RGB
    bih.biXPelsPerMeter = (vluint32_t)3780;
    bih.biYPelsPerMeter = (vluint32_t)3780;
    bih.biClrUsed       = (vluint32_t)0;
    bih.biClrImportant  = (vluint32_t)0;
    //
    bfh.bfType          = (vluint16_t)0x4D42;
    bfh.bfSize          = sizeof(BITMAPFILEHEADER)
                        + sizeof(BITMAPINFOHEADER)
                        + bih.biSizeImage;
    bfh.bfReserved1     = (vluint16_t)0;
    bfh.bfReserved2     = (vluint16_t)0;
    bfh.bfOffBits       = sizeof(BITMAPFILEHEADER)
                        + sizeof(BITMAPINFOHEADER);
    // allocate pixels
    img = new vluint8_t *[vactive];
    for (int i = 0; i < vactive; i++)
    {
        img[i] = new vluint8_t[hactive * 3];
    }
    row_e = img[0];
    row_o = img[1];
    // copy the filename
    strncpy(filename, file, 255);
    // internal variables cleared
    hcount      = -hor_offs;
    hcount1     = 0;
    hcount2     = 0;
    vcount      = -ver_offs;
    vcount1     = 0;
    vcount2     = 0;
    prev_clk    = (vluint8_t)0;
    prev_hs     = (vluint8_t)0;
    prev_vs     = (vluint8_t)0;
    first_vs    = false;
    dump_ctr    = 0;
    // initialize YUV to RGB tables
    for (int i = 0; i < 256; i++)
    {
        u_to_g[i] = (vluint16_t)(i * 44);
        u_to_b[i] = (vluint16_t)(i * 226);
        v_to_r[i] = (vluint16_t)(i * 180);
        v_to_g[i] = (vluint16_t)(i * 91);
    }
    // allocate YUV buffer
    for (int i = 0; i < 2; i++)
    {
        y_buf[i]   = new vluint8_t[hactive];
        y_buf[i+2] = new vluint8_t[hactive];
        c_buf[i]   = new vluint8_t[hactive];
    }
}

// Destructor
VideoOut::~VideoOut()
{
    for (int i = 0; i < 2; i++)
    {
        delete [] y_buf[i];
        delete [] y_buf[i+2];
        delete [] c_buf[i];
    }
    for (int i = 0; i < ver_size; i++)
    {
        delete img[i];
    }
    delete img;
}

// Cycle evaluate : RGB444 with synchros
bool VideoOut::eval_RGB444_HV
(
    // Clock
    vluint8_t  clk,
    // Synchros
    vluint8_t  vs,
    vluint8_t  hs,
    // RGB colors
    vluint8_t  red,
    vluint8_t  green,
    vluint8_t  blue
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if ((vcount >= 0) && (vcount < ver_size) &&
            (hcount >= 0) && (hcount < hor_size) &&
            (first_vs))
        {
            *row_e++ = (blue  & bit_mask) << bit_shift;
            *row_e++ = (green & bit_mask) << bit_shift;
            *row_e++ = (red   & bit_mask) << bit_shift;
            hcount++;
        }
        else
        {
            // Rising edge on VS
            if ((vs | prev_vs) == vs_pol)
            {
                if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                vcount = -ver_offs;
                hcount = -hor_offs;
                row_e  = img[0];
                
                if (first_vs)
                {
                    write_bmp();
                    ret = true;
                }
                first_vs = true;
            }
            
            // Rising edge on HS
            if ((hs | prev_hs) == hs_pol)
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                if (hcount >= 0)
                {
                    vcount++;
                    if ((vcount >= 0) && (vcount < ver_size)) row_e = img[vcount];
                }
                hcount = -hor_offs;
            }
            else
            {
                hcount++;
            }
            
            // Delayed HS and VS
            prev_vs = vs << 1;
            prev_hs = hs << 1;
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : RGB444 with data enable
bool VideoOut::eval_RGB444_DE
(
    // Clock
    vluint8_t  clk,
    // Data enable
    vluint8_t  de,
    // RGB colors
    vluint8_t  red,
    vluint8_t  green,
    vluint8_t  blue
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if (de)
        {
            *row_e++ = (blue  & bit_mask) << bit_shift;
            *row_e++ = (green & bit_mask) << bit_shift;
            *row_e++ = (red   & bit_mask) << bit_shift;
            
            hcount++;
            if (hcount == hor_size)
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                hcount = 0;
                
                vcount++;
                if (vcount == ver_size)
                {
                    if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                    vcount = 0;
                    
                    write_bmp();
                    ret = true;
                }
                row_e = img[vcount];
            }
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : YUV444 with synchros
bool VideoOut::eval_YUV444_HV
(
    // Clock
    vluint8_t  clk,
    // Synchros
    vluint8_t  vs,
    vluint8_t  hs,
    // YUV colors
    vluint8_t  luma,
    vluint8_t  cb,
    vluint8_t  cr
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if ((vcount >= 0) && (vcount < ver_size) &&
            (hcount >= 0) && (hcount < hor_size) &&
            (first_vs))
        {
            yuv2rgb(luma, cb, cr, row_e);
            row_e += 3;
            hcount++;
        }
        else
        {
            // Rising edge on VS
            if ((vs | prev_vs) == vs_pol)
            {
                if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                vcount = -ver_offs;
                hcount = -hor_offs;
                row_e  = img[0];
                
                if (first_vs)
                {
                    write_bmp();
                    ret = true;
                }
                first_vs = true;
            }
            
            // Rising edge on HS
            if ((hs | prev_hs) == hs_pol)
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                if (hcount >= 0)
                {
                    vcount++;
                    if ((vcount >= 0) && (vcount < ver_size)) row_e = img[vcount];
                }
                hcount = -hor_offs;
            }
            else
            {
                hcount++;
            }
            
            // Delayed HS and VS
            prev_vs = vs << 1;
            prev_hs = hs << 1;
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : YUV444 with data enable
bool VideoOut::eval_YUV444_DE
(
    // Clock
    vluint8_t  clk,
    // Data enable
    vluint8_t  de,
    // YUV colors
    vluint8_t  luma,
    vluint8_t  cb,
    vluint8_t  cr
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if (de)
        {
            yuv2rgb(luma, cb, cr, row_e);
            row_e += 3;
            
            hcount++;
            if (hcount == hor_size)
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                hcount = 0;
                
                vcount++;
                if (vcount == ver_size)
                {
                    if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                    vcount = 0;
                    
                    write_bmp();
                    ret = true;
                }
                row_e = img[vcount];
            }
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : YUV422 with synchros
bool VideoOut::eval_YUV422_HV
(
    // Clock
    vluint8_t  clk,
    // Synchros
    vluint8_t  vs,
    vluint8_t  hs,
    // YUV colors
    vluint8_t  luma,
    vluint8_t  chroma
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if ((vcount >= 0) && (vcount < ver_size) &&
            (hcount >= 0) && (hcount < hor_size) &&
            (first_vs))
        {
            if (hcount & 1)
            {
                // Odd pixel
                yuv2rgb(y0, u0, chroma, row_e);
                row_e += 3;
                yuv2rgb(luma, u0, chroma, row_e);
                row_e += 3;
            }
            else
            {
                // Even pixel
                y0 = luma;
                u0 = chroma;
            }
            hcount++;
        }
        else
        {
            // Rising edge on VS
            if ((vs == vs_pol) && (prev_vs != vs_pol))
            {
                if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                vcount = -ver_offs;
                hcount = -hor_offs;
                row_e  = img[0];
                
                if (first_vs)
                {
                    write_bmp();
                    ret = true;
                }
                first_vs = true;
            }
            
            // Rising edge on HS
            if ((hs == hs_pol) && (prev_hs != hs_pol))
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                if (hcount > 4) row_e = img[++vcount];
                hcount = -hor_offs;
            }
            else
            {
                hcount++;
            }
            
            // Delayed HS and VS
            prev_vs = vs << 1;
            prev_hs = hs << 1;
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : YUV422 with data enable
bool VideoOut::eval_YUV422_DE
(
    // Clock
    vluint8_t  clk,
    // Data enable
    vluint8_t  de,
    // YUV colors
    vluint8_t  luma,
    vluint8_t  chroma
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if (de)
        {
            if (hcount & 1)
            {
                // Odd pixel
                yuv2rgb(y0, u0, chroma, row_e);
                row_e += 3;
                yuv2rgb(luma, u0, chroma, row_e);
                row_e += 3;
            }
            else
            {
                // Even pixel
                y0 = luma;
                u0 = chroma;
            }
            
            hcount++;
            if (hcount == hor_size)
            {
                if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
                hcount = 0;
                
                vcount++;
                if (vcount == ver_size)
                {
                    if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                    vcount = 0;
                    
                    write_bmp();
                    ret = true;
                }
                row_e = img[vcount];
            }
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

// Cycle evaluate : YUV420 with data enables
bool VideoOut::eval_YUV420_DE
(
    // Clock
    vluint8_t  clk,
    // Data enables
    vluint8_t  de_y,
    vluint8_t  de_c,
    // YUV colors
    vluint8_t  luma,
    vluint8_t  chroma
)
{
    bool ret = false;
    
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        // Grab active area
        if (de_y)
        {
            y_buf[vcount1 & 3][hcount1] = luma;
            hcount1 ++;
            if (hcount1 == hor_size)
            {
                hcount1 = 0;
                vcount1 ++;
            }
        }
        if (de_c)
        {
            c_buf[vcount2 & 1][hcount2] = chroma;
            hcount2 ++;
            if (hcount2 == hor_size)
            {
                hcount2 = 0;
                vcount2 ++;
            }
        }
        
        // 2 lines of pixel are ready
        if (((vcount1 - vcount) >= 2) && ((vcount2 * 2 - vcount) >= 2))
        {
            vluint8_t y, u, v;
            
            // YUV420 to RGB444 conversion
            for (int i = 0; i < hor_size; i = i + 2)
            {
                u = c_buf[(vcount2 & 1) ^ 1][i];
                v = c_buf[(vcount2 & 1) ^ 1][i+1];
                
                y = y_buf[(vcount1 & 2) ^ 2][i];
                yuv2rgb(y, u, v, row_e);
                row_e += 3;
                
                y = y_buf[(vcount1 & 2) ^ 2][i+1];
                yuv2rgb(y, u, v, row_e);
                row_e += 3;
                
                y = y_buf[(vcount1 & 2) ^ 3][i];
                yuv2rgb(y, u, v, row_o);
                row_o += 3;
                
                y = y_buf[(vcount1 & 2) ^ 3][i+1];
                yuv2rgb(y, u, v, row_o);
                row_o += 3;
            }
            
            if (dbg_on) printf(" Rising edge on HS @ cycle #%llu (vcount = %d)\n", cycle_ctr, vcount);
            
            vcount += 2;
            
            if (vcount == ver_size)
            {
                if (dbg_on) printf(" Rising edge on VS @ cycle #%llu\n", cycle_ctr);
                
                vcount   = 0;
                vcount1 -= ver_size;
                vcount2 -= ver_size / 2;
                
                write_bmp();
                ret = true;
            }
            row_e = img[vcount];
            row_o = img[vcount+1];
        }
        if (dbg_on) cycle_ctr++;
    }
    prev_clk = clk;
    
    return ret;
}

int VideoOut::get_hcount()
{
    return hcount;
}

int VideoOut::get_vcount()
{
    return vcount;
}

void VideoOut::write_bmp()
{
    char tmp[264];
    FILE *fh;
    
    sprintf(tmp, "%s_%04d.bmp", filename, dump_ctr);
    dump_ctr++;
    fh = fopen (tmp, "wb");
    if (fh)
    {
        int y = ver_size;
        
        fwrite (&bfh, sizeof(BITMAPFILEHEADER), 1, fh);
        fwrite (&bih, sizeof(BITMAPINFOHEADER), 1, fh);
        while (y--)
        {
            fwrite (img[y], hor_size * 3, 1, fh);
        }
        fclose (fh);
        printf(" Save snapshot in file \"%s\"\n", tmp);
    }
    else
    {
        printf(" Cannot save file \"%s\" !!!\n", tmp);
    }
}

void VideoOut::yuv2rgb
(
    vluint8_t  lum,
    vluint8_t  cb,
    vluint8_t  cr,
    vluint8_t *buf
)
{
    int y, u, v;
    int r, g, b;
    
    y = ((int)lum & bit_mask) << (bit_shift + 7);
    u = ((int)cb  & bit_mask) << bit_shift;
    v = ((int)cr  & bit_mask) << bit_shift;
    
    r = (y + v_to_r[v] - 22906) >> 7;
    g = (y - u_to_g[u] - v_to_g[v] + 17264) >> 7;
    b = (y + u_to_b[u] - 28928) >> 7;
    
    buf[0] = (b < 0x00) ? 0x00 : (b > 0xFF) ? 0xFF : b;
    buf[1] = (g < 0x00) ? 0x00 : (g > 0xFF) ? 0xFF : g;
    buf[2] = (r < 0x00) ? 0x00 : (r > 0xFF) ? 0xFF : r;
}
