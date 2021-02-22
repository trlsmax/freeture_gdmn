/*
                                Mask.cpp

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
* \file    Mask.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    26/11/2014
*/

#include "Mask.h"
using namespace cv;

Mask::Mask(int timeInterval, bool customMask, string customMaskPath, bool downsampleMask, CamPixFmt format, bool updateMask):
mUpdateInterval(timeInterval), mUpdateMask(updateMask) {

    mMaskToCreate = false;
    updateStatus = false;
    time_point<system_clock, std::chrono::seconds> refTimeSec =
            time_point_cast<std::chrono::seconds>(system_clock::now());
    satMap = circular_buffer<Mat>(2);

    printFrameStats = false;
    
    // Load a mask from file.
    if(customMask) {

        mOriginalMask = imread(customMaskPath, IMREAD_GRAYSCALE);
        //SaveImg::saveJPEG(mOriginalMask, "/home/mOriginalMask");
        if(!mOriginalMask.data)
            throw "Fail to load the mask from its path.";

        if(downsampleMask)
            pyrDown(mOriginalMask, mOriginalMask, Size(mOriginalMask.cols/2, mOriginalMask.rows/2));

        mOriginalMask.copyTo(mCurrentMask);

    }else{

        mMaskToCreate = true;

    }

    // Estimate saturated value.
    switch(format) {

        case MONO12 :

                saturatedValue = 4092;

            break;

        default :

                saturatedValue = 254;

    }

}

bool Mask::applyMask(Mat &currFrame) {

    if(mMaskToCreate) {

        mOriginalMask = Mat(currFrame.rows, currFrame.cols, CV_8UC1,Scalar(255));
        mOriginalMask.copyTo(mCurrentMask);
        mMaskToCreate = false;

    }

    if(mUpdateMask) {

        if(updateStatus) {

            if(mCurrentMask.rows != currFrame.rows || mCurrentMask.cols != currFrame.cols) {

                throw "Mask's size is not correct according to frame's size.";

            }

            // Reference date.
            refTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());

            Mat saturateMap = ImgProcessing::buildSaturatedMap(currFrame, saturatedValue);

            // Dilatation of the saturated map.
            int dilation_size = 10;
            Mat element = getStructuringElement(MORPH_RECT, Size(2*dilation_size + 1, 2*dilation_size+1), Point(dilation_size, dilation_size));
            cv::dilate(saturateMap, saturateMap, element);

            satMap.push_back(saturateMap);

            if(satMap.size() == 2) {

                Mat temp = satMap.front() & satMap.back();
                bitwise_not(temp,temp);
                temp.copyTo(mCurrentMask, mOriginalMask);

            }

            Mat temp; currFrame.copyTo(temp, mCurrentMask);
            temp.copyTo(currFrame);

            updateStatus = false;
            return true; // Mask not applied, only computed.

        }

        time_point<system_clock, std::chrono::seconds> nowTimeSec =
                time_point_cast<std::chrono::seconds>(system_clock::now());

        auto d = nowTimeSec - refTimeSec;
        long diffTime = d.count();
        if(diffTime >= mUpdateInterval) {

            updateStatus = true;

        }

        if( printFrameStats )
	  {
	    cout <<"\033[20;0H" << "NEXT MASK : " << (mUpdateInterval - (int)diffTime) << "s" << endl;
	  }

    }

    if(!mCurrentMask.data || (mCurrentMask.rows != currFrame.rows && mCurrentMask.cols != currFrame.cols)) {
        mMaskToCreate = true;
        return true;
    }

    Mat temp; currFrame.copyTo(temp, mCurrentMask);
    temp.copyTo(currFrame);

    return false; // Mask applied.

}

void Mask::resetMask() {

    mOriginalMask.copyTo(mCurrentMask);
    updateStatus = true;
    satMap.clear();

}

void Mask::setFrameStats( bool frameStats )
{
  printFrameStats = frameStats;
}
