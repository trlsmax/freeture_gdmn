/*
                            CameraFrames.cpp

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
*   Last modified:      20/07/2015
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraFrames.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    02/09/2014
* \brief   Fits frames in input of acquisition thread.
*/

#include "CameraFrames.h"
#include <spdlog/spdlog.h>
using namespace cv;

CameraFrames::CameraFrames(vector<string> locationList, int numPos, bool verbose)
    : mNumFramePos(numPos)
    , mReadDataStatus(false)
    , mCurrDirId(0)
    , mFirstFrameNum(0)
    , mLastFrameNum(0)
{

    if (locationList.size() > 0)
        mFramesDir = locationList;
    else
        throw "No frames directory in input.";

    mExposureAvailable = false;
    mGainAvailable = false;
    mInputDeviceType = SINGLE_FITS_FRAME;
    mVerbose = verbose;
}

CameraFrames::~CameraFrames(void)
{
}

bool CameraFrames::loadNextDataSet(string& location)
{

    spdlog::info(mCurrDirId);
    location = mFramesDir.at(mCurrDirId);
    //if(mCurrDirId !=0 ) {
    mReadDataStatus = false;
    if (!searchMinMaxFramesNumber(mFramesDir.at(mCurrDirId)))
        return false;
    //}
    return true;
}

bool CameraFrames::grabInitialization()
{
    return searchMinMaxFramesNumber(mFramesDir.at(mCurrDirId));
}

bool CameraFrames::getDataSetStatus()
{
    mCurrDirId++;
    if (mCurrDirId >= mFramesDir.size())
        return false;
    else
        return true;
}

bool CameraFrames::getCameraName()
{
    spdlog::info("Fits frames data.");
    return true;
}

bool CameraFrames::searchMinMaxFramesNumber(string location)
{

    namespace fs = ghc::filesystem;
    auto logger = spdlog::get("acq_logger");

    fs::path p(location);

    if (fs::exists(p)) {
        if (mVerbose)
            logger->info("Frame's directory exists : {}", location);

        int firstFrame = -1, lastFrame = 0;
        string filename = "";
        // Search first and last frames numbers in the directory.
        for (fs::directory_iterator file(p); file != fs::directory_iterator(); ++file) {
            fs::path curr(file->path());
            if (is_regular_file(curr)) {
                // Get file name.
                string fname = curr.filename().string();

                // Split file name according to the separator "_".
                vector<string> output = TimeDate::split(fname.c_str(), '_');

                // Search frame number according to the number position known in the file name.

                int i = 0, number = 0;
                for (int j = 0; j < output.size(); j++) {
                    if (j == mNumFramePos && j != output.size() - 1) {
                        number = atoi(output.at(j).c_str());
                        break;
                    }

                    // If the frame number is at the end (before the file extension).
                    if (j == mNumFramePos && j == output.size() - 1) {
                        vector<string> output2 = TimeDate::split(output.back().c_str(), '.');
                        number = atoi(output2.front().c_str());
                        break;
                    }
                    i++;
                }

                if (firstFrame == -1) {
                    firstFrame = number;
                } else if (number < firstFrame) {
                    firstFrame = number;
                }

                if (number > lastFrame) {
                    lastFrame = number;
                }
            }
        }

        if (mVerbose) {
            logger->info("First frame number in frame's directory : {}", firstFrame);
            logger->info("Last frame number in frame's directory : {}", lastFrame);
        }

        mLastFrameNum = lastFrame;
        mFirstFrameNum = firstFrame;
        return true;
    } else {
        if (mVerbose) {
            logger->error("Frame's directory not found.");
            spdlog::error("Frame's directory not found.");
        }
        return false;
    }
}

bool CameraFrames::getStopStatus()
{
    return mReadDataStatus;
}

bool CameraFrames::getFPS(double& value)
{
    value = 0;
    return false;
}

bool CameraFrames::grabImage(Frame& img)
{
    namespace fs = ghc::filesystem;
    auto logger = spdlog::get("acq_logger");
    bool fileFound = false;
    string filename = "";
    fs::path p(mFramesDir.at(mCurrDirId));

    /// Search a frame in the directory.
    for (fs::directory_iterator file(p); file != fs::directory_iterator(); ++file) {
        fs::path curr(file->path());

        if (is_regular_file(curr)) {
            list<string> ch;
            string fname = curr.filename().string();
            Conversion::stringTok(ch, fname.c_str(), "_");
            list<string>::const_iterator lit(ch.begin()), lend(ch.end());
            int i = 0;
            int number = 0;

            for (; lit != lend; ++lit) {
                if (i == mNumFramePos && i != ch.size() - 1) {
                    number = atoi((*lit).c_str());
                    break;
                }

                if (i == ch.size() - 1) {
                    list<string> ch_;
                    Conversion::stringTok(ch_, (*lit).c_str(), ".");
                    number = atoi(ch_.front().c_str());
                    break;
                }

                i++;
            }

            if (number == mFirstFrameNum) {
                mFirstFrameNum++;
                fileFound = true;
                spdlog::info("FILE:{}", file->path().string());
                logger->info("FILE:{}", file->path().string());

                filename = file->path().string();
                break;
            }
        }
    }

    if (mFirstFrameNum > mLastFrameNum || !fileFound) {
        mReadDataStatus = true;
        logger->info("End read frames.");
        return false;

    } else {
        logger->info("Frame found.");

        Mat resMat;
        CamPixFmt frameFormat = MONO8;

        Frame f = Frame(resMat, 0, 0);

        img = f;

        img.mFrameNumber = mFirstFrameNum - 1;
        img.mFrameRemaining = mLastFrameNum - mFirstFrameNum - 1;
        img.mFps = 1;
        img.mFormat = frameFormat;

        //waitKey(1000);

        return true;
    }
}
