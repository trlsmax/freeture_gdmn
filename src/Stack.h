/*
                                Stack.h

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
* \file    Stack.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    19/06/2014
* \brief
*/

#pragma once

#include "config.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <filesystem.hpp>
#include "ELogSeverityLevel.h"
#include "TimeDate.h"
#include "Frame.h"
#include "EStackMeth.h"
#include "SParam.h"

using namespace ghc::filesystem;
using namespace std;

class Stack {
    private :
        const string FITS_SUFFIX = "stack";
        cv::Mat             stack;
        int             curFrames;
        int             gainFirstFrame;
        int             expFirstFrame;
        TimeDate::Date  mDateFirstFrame;
        TimeDate::Date  mDateLastFrame;
        int             fps;
        CamPixFmt       format;
        bool            varExpTime;
        double          sumExpTime;

    public :

        /**
        * Constructor.
        *
        */
        Stack();

        /**
        * Destructor.
        *
        */
        ~Stack(void);

        /**
        * Add a frame to the stack.
        *
        * @param i Frame to add.
        */
        void addFrame(Frame &i);

        /**
        * Get Date of the first frame of the stack.
        *
        * @return Date.
        */
        TimeDate::Date getDateFirstFrame(){return mDateFirstFrame;};

        /**
        * Get number of frames in the stack.
        *
        * @return Number of frames.
        */
        int getNbFramesStacked(){return curFrames;};

        cv::Mat& getStack() {return stack;};
};
