/*
                                Device.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                       2018 Chiara Marmo
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
*   Last modified:      19/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    Device.h
* \author  Yoan Audureau -- Chiara Marmo -- GEOPS-UPSUD
* \version 1.2
* \date    19/03/2018
* \brief
*/

#pragma once

#include "config.h"

#include <opencv2/imgproc/imgproc.hpp>
#include "ELogSeverityLevel.h"
#include "EImgBitDepth.h"
#include "ECamPixFmt.h"
#include "EParser.h"
#include "Conversion.h"
#include "Camera.h"
//#include "CameraGigeAravis.h"
//#include "CameraUsbAravis.h"
//#include "CameraGigePylon.h"
//#include "CameraGigeTis.h"
#include "CameraVideo.h"
#include "CameraV4l2.h"
#include "CameraFrames.h"
#include "CameraWindows.h"
#include <vector>
#include <algorithm>
#include <string>
#include <filesystem.hpp>
#include <iterator>
#include <algorithm>
#include <circular_buffer/circular_buffer.hpp>
#include "EInputDeviceType.h"
#include "ECamSdkType.h"
#include "SParam.h"

using namespace ghc::filesystem;
using namespace cb;
using namespace std;

class Device {

    public :

        bool mVideoFramesInput; // TRUE if input is a video file or frames directories.

    private :
        vector<pair<int,pair<int,CamSdkType>>> mDevices;
        bool        mCustomSize;
        int         mStartX;
        int         mStartY;
        int         mSizeWidth;
        int         mSizeHeight;
        int         mNightExposure;
        int         mNightGain;
        int         mDayExposure;
        int         mDayGain;
        double      mFPS;
        int         mCamID;         // ID in a specific sdk.
        int         mGenCamID;      // General ID.
        Camera      *mCam;
        bool        mShiftBits;
        bool        mVerbose;
        framesParam mfp;
        videoParam  mvp;

    public :

        int         mNbDev;
        CamPixFmt   mFormat;
        string      mCfgPath;
        InputDeviceType mDeviceType;
        double      mMinExposureTime;
        double      mMaxExposureTime;
        int         mMinGain;
        int         mMaxGain;
        //int         mNbFrame;

    public :

        Device(cameraParam cp, framesParam fp, videoParam vp, int cid);

        Device();

        ~Device();

        InputDeviceType getDeviceType(CamSdkType t);

        CamSdkType getDeviceSdk(int id);

        void listDevices(bool printInfos);

        bool createCamera(int id, bool create);

        bool createCamera();

        bool initializeCamera();

        bool runContinuousCapture(Frame &img);

        bool runSingleCapture(Frame &img);

        bool startCamera();

        bool stopCamera();

        bool setCameraPixelFormat();

        bool getCameraGainBounds(int &min, int &max);

        void getCameraGainBounds();

        bool getCameraExposureBounds(double &min, double &max);

        void getCameraExposureBounds();

        bool getDeviceName();

        bool recreateCamera();

        InputDeviceType getDeviceType();

        bool setCameraNightExposureTime();

        bool setCameraDayExposureTime();

        bool setCameraNightGain();

        bool setCameraDayGain();

        bool setCameraExposureTime(double value);

        bool setCameraGain(int value);

        bool setCameraFPS();
        bool setCameraFPS(double fps);
          
        bool setCameraSize();

        bool getCameraFPS(double &fps);

        bool getCameraStatus();

        bool getCameraDataSetStatus();

        bool getSupportedPixelFormats();

        bool loadNextCameraDataSet(string &location);

        bool getExposureStatus();

        bool getGainStatus();

        bool setCameraSize(int x, int y, int w, int h);

        int getNightExposureTime() {return mNightExposure;};
        int getNightGain() {return mNightGain;};
        int getDayExposureTime() {return mDayExposure;};
        int getDayGain() {return mDayGain;};

        // bool setArvCameraAcquisitionMode( ArvAcquisitionMode acquisition_mode );
        
        void setVerbose(bool status);

    private :

        bool createDevicesWith(CamSdkType sdk);

};
