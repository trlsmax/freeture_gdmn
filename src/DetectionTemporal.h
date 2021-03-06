/*
                            DetectionTemporal.h

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
 * \file    DetectionTemporal.h
 * \author  Yoan Audureau -- GEOPS-UPSUD
 * \version 1.0
 * \date    03/06/2014
 * \brief   Detection method by temporal analysis.
 */

#pragma once

#include <algorithm>
#include <filesystem.hpp>
#include <iterator>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <utility>
#include <vector>

#include "DataPaths.h"
#include "Detection.h"
#include "ECamPixFmt.h"
#include "ELogSeverityLevel.h"
#include "EParser.h"
#include "EStackMeth.h"
#include "Frame.h"
#include "GlobalEvent.h"
#include "ImgProcessing.h"
#include "LocalEvent.h"
#include "SaveImg.h"
#include "TimeDate.h"
#include "config.h"

using namespace ghc::filesystem;
using namespace std;

class DetectionTemporal : public Detection {
private:
    list<std::shared_ptr<GlobalEvent>> mListGlobalEvents;  // List of global events (Events spread on several frames).
    list<std::shared_ptr<GlobalEvent>>::iterator mGeToSave;  // Global event to save.
    vector<cv::Point> mSubdivisionPos;  // Position (origin in top left) of 64 subdivisions.
    vector<cv::Scalar> mListColors;  // One color per local event.
    cv::Mat mLocalMask;              // Mask used to remove isolated white pixels.
    bool mSubdivisionStatus;  // If subdivisions positions have been computed.
    cv::Mat mPrevThresholdedMap;
    int mRoiSize[2];
    int mImgNum;     // Current frame number.
    cv::Mat mPrevFrame;  // Previous frame.
    cv::Mat mMask;
    string mDebugCurrentPath;
    int mDataSetCounter;
    bool mDebugUpdateMask;
    vector<string> debugFiles;
    detectionParam mdtp;
    cv::VideoWriter mVideoDebugAutoMask;

public:
    DetectionTemporal(detectionParam dp, CamPixFmt fmt);

    ~DetectionTemporal();

    void initMethod(string cfgPath);

    std::shared_ptr<GlobalEvent> runDetection(std::shared_ptr<Frame> c);

    void resetDetection(bool loadNewDataSet);


    TimeDate::Date getEventDate() { return (*mGeToSave)->getDate(); };

    vector<string> getDebugFiles();

private:
    void createDebugDirectories(bool cleanDebugDirectory);

    int selectThreshold(cv::Mat i);

    vector<cv::Scalar> getColorInEventMap(cv::Mat &eventMap, cv::Point roiCenter);

    void colorRoiInBlack(cv::Point p, int h, int w, cv::Mat &region);

    void analyseRegion(cv::Mat &subdivision, cv::Mat &absDiffBinaryMap, cv::Mat &eventMap,
                       cv::Mat &posDiff, int posDiffThreshold, cv::Mat &negDiff,
                       int negDiffThreshold, list<std::shared_ptr<LocalEvent>> &listLE,
                       cv::Point subdivisionPos, int maxNbLE, int numFrame,
                       TimeDate::Date cFrameDate);
};

