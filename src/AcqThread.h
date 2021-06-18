/*
                                AcqThread.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau, Chiara Marmo -- GEOPS-UPSUD
*                       2020 Martin Cupak
*                             DFN - GFO - SSTC - Curtin university
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
*   Last modified:      27/05/2020
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
 * \file    AcqThread.h
 * \author  Yoan Audureau, Chiara Marmo -- GEOPS-UPSUD
 * \version 1.0
 * \date    03/10/2016
 * \brief   Acquisition thread.
 */

#ifndef ACQTHREAD_H
#define ACQTHREAD_H

#include <condition_variable>
#include <filesystem.hpp>
#include <mutex>
#include <thread>

#include "double_linked_list.h"
#include "DataPaths.h"
#include "DetThread.h"
#include "Device.h"
#include "ECamPixFmt.h"
#include "EImgFormat.h"
#include "Ephemeris.h"
#include "ExposureControl.h"
#include "ImgProcessing.h"
#include "SParam.h"
#include "config.h"

using namespace std;
using namespace ghc;

class AcqThread {
private:
    const string FITS_SUFFIX = "capture";

    bool mMustStop; // Signal to stop thread.
    mutex mMustStopMutex;
    thread* mThread; // Acquisition thread.
    bool mThreadTerminated; // Terminated status of the thread.
    Device* mDevice; // Device used for acquisition.
    int mDeviceID; // Index of the device to use.
    scheduleParam mNextAcq {}; // Next scheduled acquisition.
    int mNextAcqIndex;
    DetThread* pDetection; // Pointer on detection thread in order to stop it
        // or reset it when a regular capture occurs.
    ExposureControl* pExpCtrl; // Pointer on exposure time control object while
        // sunrise and sunset.
    string mOutputDataPath; // Dynamic location where to save data (regular
        // captures etc...).
    string mCurrentDate;
    int mStartSunriseTime {}; // In seconds.
    int mStopSunriseTime {}; // In seconds.
    int mStartSunsetTime {}; // In seconds.
    int mStopSunsetTime {}; // In seconds.
    int mCurrentTime {}; // In seconds.

    // Parameters from configuration file.
    stationParam mstp;
    detectionParam mdtp;
    cameraParam mcp;
    dataParam mdp;
    fitskeysParam mfkp;
    framesParam mfp;
    videoParam mvp;

    // Communication with the shared framebuffer.
    condition_variable* frameBuffer_condition;
    mutex* frameBuffer_mutex;
    CDoubleLinkedList<std::shared_ptr<Frame>>* frameBuffer;

    // Communication with StackThread.
    bool* detSignal;
    mutex* detSignal_mutex;
    condition_variable* detSignal_condition;

    bool printFrameStats;

public:
    AcqThread(CDoubleLinkedList<std::shared_ptr<Frame>>* fb, mutex* fb_m,
        condition_variable* fb_c, bool* dSignal, mutex* dSignal_m,
        condition_variable* dSignal_c, DetThread* detection,
        int cid, dataParam dp, stationParam stp, detectionParam dtp, cameraParam acq,
        framesParam fp, videoParam vp, fitskeysParam fkp);

    ~AcqThread(void);

    void run();

    void stopThread();

    bool startThread();

    // Return activity status.
    bool getThreadStatus() const;

    void setFrameStats(bool frameStats);

private:
    // Compute in seconds the sunrise start/stop times and the sunset start/stop
    // times.
    bool computeSunTimes();

    // Build the directory where the data will be saved.
    bool buildAcquisitionDirectory(TimeDate::Date date);

    // Analyse the scheduled acquisition list to find the next one according to
    // the current time.
    void selectNextAcquisitionSchedule(TimeDate::Date date);

    // Save a capture on disk.
    void saveImageCaptured(Frame& img, int imgNum, ImgFormat outputType,
        string imgPrefix);

    // Run a regular or scheduled acquisition.
    void runImageCapture(int imgNumber, int imgExposure, int imgGain,
        CamPixFmt imgFormat, ImgFormat imgOutput,
        string imgPrefix);

    // Prepare the device for a continuous acquisition.
    bool prepareAcquisitionOnDevice();
};

#endif
