/*
                                    PixFmtConv.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                               GEOPS-UPSUD-CNRS
*
*   License:        GNU General Public License
*
*   FreeTure is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*   FreeTure is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*   You should have received a copy of the GNU General Public License
*   along with FreeTure. If not, see <http://www.gnu.org/licenses/>.
*
*   Last modified:      26/11/2015
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    PixFmtConv.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    26/11/2015
*/

#pragma once

#include <iostream>
#include <string>

#define CLIP(color) (unsigned char)(((color) > 0xFF) ? 0xff : (((color) < 0) ? 0 : (color)))

using namespace std;

class PixFmtConv {

    public :

        static void UYVY_to_BGR24(const unsigned char *src, unsigned char *dest, int width, int height, int stride);

        static void YUYV_to_BGR24(const unsigned char *src, unsigned char *dest, int width, int height, int stride);

        static void RGB565_to_BGR24(const unsigned char *src, unsigned char *dest, int width, int height);

};

