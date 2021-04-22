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

#include "opencv2/highgui/highgui.hpp"
#include <opencv2/imgproc/imgproc.hpp>
#include "ELogSeverityLevel.h"
#include "TimeDate.h"
#include "Fits2D.h"
#include "Fits.h"
#include "Frame.h"
#include "EStackMeth.h"
#include <filesystem.hpp>

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
        string          mFitsCompressionMethod;
        stationParam mstp;
        fitskeysParam mfkp;

    public :

        /**
        * Constructor.
        *
        */
        Stack(string fitsCompression, fitskeysParam fkp, stationParam stp);

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
        * Save stack.
        *
        * @param fitsHeader fits keywords.
        * @param path Location where to save the stack.
        * @param stackMthd Method to use to create stack.
        * @param stationName Name of the station.
        * @param stackReduction Enable stack reduction.
        * @return Success status.
        */
        bool saveStack(string path, StackMeth stackMthd, bool stackReduction);

        /**
        * Get number of frames in the stack.
        *
        * @return Number of frames.
        */
        int getNbFramesStacked(){return curFrames;};

        cv::Mat& getStack() {return stack;};

        /**
        * Reduce stack in float 32 bits to 8 or 16 bits.
        *
        * @param bzero (Physical_value = image_value * bscale + bzero)
        * @param bscale
        * @return Reduced image.
        */
        cv::Mat reductionByFactorDivision(float &bzero, float &bscale);

};
