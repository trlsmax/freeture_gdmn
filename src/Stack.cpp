/*
                                Stack.cpp

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
* \file    Stack.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    19/06/2014
* \brief
*/

#include "Stack.h"
#include <spdlog/spdlog.h>
using namespace cv;

Stack::Stack()
    : curFrames(0)
    , varExpTime(false)
    , sumExpTime(0.0)
    , gainFirstFrame(0)
    , expFirstFrame(0)
    , fps(0)
    , format(MONO8)
{
}

Stack::~Stack()
{}

void Stack::addFrame(Frame &i)
{
    try {
        if (TimeDate::getYYYYMMDD(i.mDate) != "00000000") {
            if (curFrames == 0) {
                stack = Mat::zeros(i.mImg.rows, i.mImg.cols, CV_8UC1);
                gainFirstFrame = i.mGain;
                expFirstFrame = i.mExposure;
                mDateFirstFrame = i.mDate;
                fps = i.mFps;
                format = i.mFormat;
            }

            if (expFirstFrame != i.mExposure)
                varExpTime = true;
            sumExpTime += i.mExposure;
            //cv::accumulate(curr, stack);
            Mat& curr = i.mImg;
			unsigned char *ptr = NULL;
			unsigned char *ptr2 = NULL;
			for (int i = 0; i < stack.rows; i++) {
				ptr = stack.ptr<unsigned char>(i);
				ptr2 = curr.ptr<unsigned char>(i);
				for (int j = 0; j < stack.cols; j++) {
                    if (ptr2[j] > ptr[j]) {
                        ptr[j] = ptr2[j];
                    }
				}
			}
            curFrames++;
            mDateLastFrame = i.mDate;
        }
    } catch (exception &e) {
        cout << e.what() << endl;
        auto logger = spdlog::get("stack_logger");
        logger->critical(e.what());
    }
}

