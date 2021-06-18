/*
                        DetectionTemplate.cpp

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
* \file    DetectionTemplate.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    03/03/2015
*/

#include "DetectionTemplate.h"
using namespace cv;

DetectionTemplate::DetectionTemplate(detectionParam dtp, CamPixFmt fmt):mImgNum(0), mDataSetCounter(0) {

    mdtp = dtp;

}

DetectionTemplate::~DetectionTemplate() {
}

void DetectionTemplate::createDebugDirectories(bool cleanDebugDirectory) {

}

bool DetectionTemplate::runDetection(Frame &c) {

    Mat currImg;

    if(mdtp.DET_DOWNSAMPLE_ENABLED)
        pyrDown(c.mImg, currImg, Size(c.mImg.cols / 2, c.mImg.rows / 2));
    else
        c.mImg.copyTo(currImg);

    // --------------------------------
    //          OPERATIONS
    // --------------------------------

    // --------------------------------
    //      Check previous frame.
    // --------------------------------

    if(!mPrevFrame.data) {

        cout << "PrevFrame has no data ! " << endl;
        currImg.copyTo(mPrevFrame);
        return false;

    }

    Mat absdiffImg;
    cv::absdiff(currImg, mPrevFrame, absdiffImg);

    // ---------------------------------
    //  Dilatation absolute difference.
    // ---------------------------------

    int dilation_size = 2;
    Mat element = getStructuringElement(MORPH_RECT, Size(2*dilation_size + 1, 2*dilation_size+1), Point(dilation_size, dilation_size));
    cv::dilate(absdiffImg, absdiffImg, element);

    // ----------------------------------
    //   Threshold absolute difference.
    // ----------------------------------

    Mat absDiffBinaryMap = Mat(currImg.rows,currImg.cols, CV_8UC1,Scalar(0));
    Scalar meanAbsDiff, stddevAbsDiff;
    cv::meanStdDev(absdiffImg, meanAbsDiff, stddevAbsDiff, mMask);
    int absDiffThreshold = meanAbsDiff[0] * 3;

    if(absdiffImg.type() == CV_16UC1) {

        unsigned short * ptrAbsDiff;
        unsigned char * ptrMap;

        for(int i = 0; i < absdiffImg.rows; i++) {

            ptrAbsDiff = absdiffImg.ptr<unsigned short>(i);
            ptrMap = absDiffBinaryMap.ptr<unsigned char>(i);

            for(int j = 0; j < absdiffImg.cols; j++){

                if(ptrAbsDiff[j] > absDiffThreshold) {
                    ptrMap[j] = 255;
                }
            }
        }

    }

    currImg.copyTo(mPrevFrame);

    // No detection : return false
    return false;

}

void DetectionTemplate::saveDetectionInfos(string p, int nbFramesAround) {


}

void DetectionTemplate::resetDetection(bool loadNewDataSet) {


}


int DetectionTemplate::getEventFirstFrameNb() {

    return 0;

}

TimeDate::Date DetectionTemplate::getEventDate() {

    TimeDate::Date d;
    return d;

}

int DetectionTemplate::getEventLastFrameNb() {

    return 0;

}
