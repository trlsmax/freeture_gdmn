/*
                            CameraFrames.h

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
* \file    CameraFrames.h
* \author  Yoan Audureau -- Chiara Marmo -- GEOPS-UPSUD
* \version 1.2
* \date    19/03/2018
* \brief   Fits frames in input of acquisition thread.
*/

#pragma once
#include "config.h"
#include <chrono>
#include <iterator>
#include <list>

#include <filesystem.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "Conversion.h"
#include "ELogSeverityLevel.h"
#include "Fits.h"
#include "Fits2D.h"
#include "Frame.h"
#include "TimeDate.h"

#include "Camera.h"

using namespace std::chrono;
using namespace ghc;
using namespace std;

class CameraFrames : public Camera {

private:
    bool searchMinMaxFramesNumber(string location);
    vector<string> mFramesDir; // List of frames directories to process.
    int mNumFramePos; // Position of the frame number in its filename.
    int mFirstFrameNum; // First frame number in a directory.
    int mLastFrameNum; // Last frame number in a directory.
    bool mReadDataStatus; // Signal the end of reading data in a directory.
    int mCurrDirId; // Id of the directory to use.
    string mCurrDir; // Path of the directory to use.

public:
    CameraFrames(vector<string> locationList, int numPos, bool verbose);

    ~CameraFrames() override;

    bool acqStart() override { return true; };

    bool createDevice(int id) override { return true; };

    bool listCameras() override { return true; };

    bool grabInitialization();

    bool grabImage(Frame& img);

    bool getStopStatus();

    bool loadNextDataSet(string& location);

    bool getDataSetStatus();

    bool getFPS(double& value);

    bool setExposureTime(double exp) { return true; };

    bool setGain(int gain) { return true; };

    bool setFPS(double fps) { return true; };

    bool setPixelFormat(CamPixFmt format) { return true; };

    bool setSize(int startx, int starty, int width, int height, bool customSize) { return true; };

    bool getCameraName();
};

