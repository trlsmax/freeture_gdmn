/*
                            DetectionTemplate.h

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
*   Last modified:      03/03/2015
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    DetectionTemplate.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    03/03/2015
*/

#pragma once

#include "config.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include "ELogSeverityLevel.h"
#include "TimeDate.h"
#include "Frame.h"
#include "EStackMeth.h"
#include "ECamPixFmt.h"
#include "GlobalEvent.h"
#include "LocalEvent.h"
#include "Detection.h"
#include "EParser.h"
#include "SaveImg.h"
#include <vector>
#include <utility>
#include <iterator>
#include <algorithm>
#include <filesystem.hpp>
#include "ImgProcessing.h"

using namespace ghc::filesystem;
using namespace std;

class DetectionTemplate : public Detection {

    private :
        int                 mImgNum;                // Current frame number.
        cv::Mat                 mPrevFrame;             // Previous frame.
        cv::Mat                 mMask;                  // Mask applied to frames.
        int                 mDataSetCounter;
        detectionParam      mdtp;


    public :

        DetectionTemplate(detectionParam dtp, CamPixFmt fmt);

        ~DetectionTemplate();

        void initMethod(string cfgPath);

        bool runDetection(Frame &c);

        void saveDetectionInfos(string p, int nbFramesAround);

        void resetDetection(bool loadNewDataSet);

        int getEventFirstFrameNb();

        TimeDate::Date getEventDate();

        int getEventLastFrameNb();

    private :

        void createDebugDirectories(bool cleanDebugDirectory);

};


