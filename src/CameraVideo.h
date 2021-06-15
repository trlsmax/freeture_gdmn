/*
                            CameraVideo.h

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
*   Last modified:      20/10/2014
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraVideo.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    03/06/2014
* \brief   Acquisition thread with video in input.
*/

#pragma once
#include "config.h"

#include <opencv2/imgproc/imgproc.hpp>

#include "Frame.h"
#include "SaveImg.h"
#include "TimeDate.h"
#include "Conversion.h"
#include "Camera.h"
#include <string>
#include <vector>
#include <filesystem.hpp>
#include <circular_buffer/circular_buffer.hpp>

using namespace ghc::filesystem;
using namespace cb;
using namespace std;

class CameraVideo : public Camera{
    private:
        int                 mFrameWidth;
        int                 mFrameHeight;
        cv::VideoCapture        mCap;
        bool                mReadDataStatus;
        int                 mVideoID;
        vector<string>      mVideoList;

    public:

        CameraVideo(vector<string> videoList, bool verbose);

        ~CameraVideo(void);

        bool createDevice(int id);

        bool acqStart() {return true;};

        bool listCameras() {return true;};

        bool grabImage(Frame &img);

        bool grabInitialization();

        bool getStopStatus();

        /**
        * Get data status : Is there another video to use in input ?
        *
        * @return If there is still a video to load in input.
        */
        bool getDataSetStatus();

        /**
        * Load next video if there is.
        *
        * @return Success status to load next data set.
        */
        bool loadNextDataSet(string &location);

        bool getFPS(double &value) {value = 0; return false;};

        bool setExposureTime(double exp){return true;};

        bool setGain(int gain) {return true;};

        bool setFPS(double fps){return true;};

        bool setPixelFormat(CamPixFmt format){return true;};

        bool setSize(int x, int y, int width, int height, bool customSize) {return true;};

};

