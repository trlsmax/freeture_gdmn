/*
                                StackThread.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau -- GEOPS-UPSUD
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
* \file    StackThread.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    19/06/2014
* \brief   Stack frames.
*/

#pragma once

#include "config.h"

#include <iostream>
#include <assert.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <spdlog/spdlog.h>
#include <filesystem.hpp>
#include <circular_buffer/circular_buffer.hpp>

#include "EStackMeth.h"
#include "Stack.h"
#include "Fits.h"
#include "Fits2D.h"
#include "TimeDate.h"
#include "EParser.h"
#include "SParam.h"
#include "DataPaths.h"

using namespace ghc;
using namespace cb;
using namespace std;

class StackThread {

    private :
        
        const string FITS_SUFFIX = "stack";

        thread   *mThread;
        bool            mustStop;
        mutex    mustStopMutex;
        bool            isRunning;
        bool            interruptionStatus;
        mutex    interruptionStatusMutex;

        condition_variable       *frameBuffer_condition;
        mutex                    *frameBuffer_mutex;
        circular_buffer<Frame>   *frameBuffer;
        bool                            *stackSignal;
        mutex                    *stackSignal_mutex;
        condition_variable       *stackSignal_condition;

        stationParam    mstp;
        fitskeysParam   mfkp;
        dataParam       mdp;
        stackParam      msp;
        CamPixFmt       mPixfmt;

        string completeDataPath;

	bool printFrameStats;
	
    public :

        StackThread(    bool                            *sS,
                        mutex                    *sS_m,
                        condition_variable       *sS_c,
                        circular_buffer<Frame>   *fb,
                        mutex                    *fb_m,
                        condition_variable       *fb_c,
                        dataParam       dp,
                        stackParam      sp,
                        stationParam    stp,
                        CamPixFmt       pfmt,
                        fitskeysParam   fkp);

        ~StackThread(void);

        bool startThread();

        void stopThread();

        void run();

        bool getRunStatus();

        bool interruptThread();
	
	void setFrameStats( bool frameStats );

    private :

        bool buildStackDataDirectory(TimeDate::Date date);

};
