/*
                                DetThread.h

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
* \file    DetThread.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    03/06/2014
* \brief   Detection thread.
*/

#pragma once

#include "config.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <filesystem.hpp>
#include <iterator>

#include "double_linked_list.h"
#include "DataPaths.h"
#include "Detection.h"
#include "DetectionTemplate.h"
#include "DetectionTemporal.h"
#include "EDetMeth.h"
#include "ESmtpSecurity.h"
#include "EStackMeth.h"
#include "SParam.h"
#include "Stack.h"
#include "TimeDate.h"

using namespace ghc;
using namespace std;

class DetThread {

private:
    const string FITS_SUFFIX = "event";

    thread* pThread; // Pointer on detection thread.
    Detection* pDetMthd; // Pointer on detection method.
    bool mMustStop;
    mutex mMustStopMutex;
    string mStationName; // Name of the station              (parameter from configuration file).
    CamPixFmt mFormat; // Acquisition bit depth            (parameter from configuration file).
    bool mIsRunning; // Detection thread running status.
    bool mWaitFramesToCompleteEvent;
    int mNbWaitFrames;
    string mCfgPath;
    string mEventPath; // Path of the last detected event.
    TimeDate::Date mEventDate; // Date of the last detected event.
    int mNbDetection; // Number of detection.
    bool mInterruptionStatus;
    mutex mInterruptionStatusMutex;
    CDoubleLinkedList<std::shared_ptr<Frame>>* frameBuffer;
    mutex* frameBuffer_mutex;
    condition_variable* frameBuffer_condition;
    bool* detSignal;
    mutex* detSignal_mutex;
    condition_variable* detSignal_condition;
    string mCurrentDataSetLocation;
    vector<pair<string, int>> mDetectionResults;
    bool mForceToReset;
    detectionParam mdtp;
    dataParam mdp;
    mailParam mmp;
    fitskeysParam mfkp;
    stationParam mstp;
    int mNbFramesAround; // Number of frames to keep around an event.

    bool printFrameStats;

public:
    DetThread(CDoubleLinkedList<std::shared_ptr<Frame>>* fb,
        mutex* fb_m,
        condition_variable* fb_c,
        bool* dSignal,
        mutex* dSignal_m,
        condition_variable* dSignal_c,
        detectionParam dtp,
        dataParam dp,
        mailParam mp,
        stationParam sp,
        fitskeysParam fkp,
        CamPixFmt pfmt);

    ~DetThread();

    void run();

    bool startThread();

    void stopThread();

    bool buildEventDataDirectory();

    /**
        * Save an event in the directory "events".
        *
        * @param firstEvPosInFB First frame's number of the event.
        * @param lastEvPosInFB Last frame's number of the event.
        * @return Success to save an event.
        */
    bool saveEventData(GlobalEvent* ge);

    /**
        * Run status of detection thread.
        *
        * @return Is running or not.
        */
    bool getRunStatus();

    /**
        * Get detection method used by detection thread.
        *
        * @return Detection method.
        */
    Detection* getDetMethod();

    /**
        * Interrupt detection thread.
        *
        */
    void interruptThread();

    void updateDetectionReport()
    {
        if (!mCurrentDataSetLocation.empty()) {
            mDetectionResults.emplace_back(mCurrentDataSetLocation, mNbDetection);
            mNbDetection = 0;
        }
    };

    void setCurrentDataSet(string location)
    {
        mCurrentDataSetLocation = std::move(location);
    };

    void setFrameStats(bool frameStats);
};

