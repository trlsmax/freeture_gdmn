/*                      CameraUsbAravis.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau -- FRIPON-GEOPS-UPSUD
*                   (C) 2018 Martin Cupak -- DFN
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
*   Last modified:      14/08/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraUsbAravis.h
* \author  Yoan Audureau -- FRIPON-GEOPS-UPSUD
* \version 1.0
* \date    07/08/2018
* \brief   Use Aravis library to pilot USB Cameras.
*          https://wiki.gnome.org/action/show/Projects/Aravis?action=show&redirect=Aravis
*/

#pragma once

#include "config.h"

#ifdef LINUX

    #include "opencv2/highgui/highgui.hpp"
    #include <opencv2/imgproc/imgproc.hpp>

    #include <iostream>
    #include <string>
    #include "Frame.h"
    #include "TimeDate.h"
    #include "Camera.h"
    #include "arv.h"
    #include "arvtypes.h"
    #include "arvinterface.h"
    #include <time.h>
    #include <algorithm>
    #include "EParser.h"

    using namespace cv;
    using namespace std;

    class CameraUsbAravis: public Camera{

        private:
            ArvCamera       *camera;                // Camera to control.
            ArvPixelFormat  pixFormat;              // Image format.
            ArvStream       *stream;                // Object for video stream reception.
            int             mStartX;                // Crop starting X.
            int             mStartY;                // Crop starting Y.
            int             mWidth;                  // Camera region's width.
            int             mHeight;                 // Camera region's height.
            double          fps;                    // Camera acquisition frequency.
            double          gainMin;                // Camera minimum gain.
            double          gainMax;                // Camera maximum gain.
            unsigned int    payload;                // Width x height.
            double          exposureMin;            // Camera's minimum exposure time.
            double          exposureMax;            // Camera's maximum exposure time.
            const char      *capsString;
            int             gain;                   // Camera's gain.
            double          exp;                    // Camera's exposure time.
            bool            shiftBitsImage;         // For example : bits are shifted for dmk's frames.
            guint64         nbCompletedBuffers;     // Number of frames successfully received.
            guint64         nbFailures;             // Number of frames failed to be received.
            guint64         nbUnderruns;
            int             frameCounter;           // Counter of success received frames.

        public :

            CameraUsbAravis(bool shift);

            CameraUsbAravis();

            ~CameraUsbAravis();

            vector<pair<int,string>> getCamerasList();

            bool listCameras();

            bool createDevice(int id);

            bool grabInitialization();

            void grabCleanse();

            bool acqStart();

            void acqStop();

            bool grabImage(Frame& newFrame);

            bool grabSingleImage(Frame &frame, int camID);

            bool getDeviceNameById(int id, string &device);

            void getExposureBounds(double &eMin, double &eMax);

            void getGainBounds(int &gMin, int &gMax);

            bool getPixelFormat(CamPixFmt &format);

            bool getFrameSize(int &w, int &h);
            bool getFrameSize(int &x, int &y, int &w, int &h);

            bool getFPS(double &value);

            string getModelName();

            double getExposureTime();

            bool setExposureTime(double exp);

            bool setGain(int gain);

            bool setFPS(double fps);

            bool setPixelFormat(CamPixFmt depth);

            void saveGenicamXml(string p);

            //bool setSize(int width, int height, bool customSize);
            bool setSize(int startx, int starty, int width, int height, bool customSize);                        

            bool setFrameSize(int startx, int starty, int width, int height, bool customSize);

            void getAvailablePixelFormats();

            void deviceReset();
    };

#endif
