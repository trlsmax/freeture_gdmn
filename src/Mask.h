/*
                                    Mask.h

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
*   Last modified:      26/11/2014
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    Mask.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    26/11/2014
*/

#pragma once
#include <chrono>
#include <circular_buffer/circular_buffer.hpp>
#include "ImgProcessing.h"
#include "SaveImg.h"
#include "Conversion.h"
#include "ECamPixFmt.h"
#include <filesystem.hpp>

using namespace std;
using namespace cb;
using namespace ghc::filesystem;
using namespace std::chrono;

class Mask {

    public :

        cv::Mat mCurrentMask;

    private :

        cv::Mat mOriginalMask;
        int mUpdateInterval;
        bool mUpdateMask;
        bool mMaskToCreate;
        time_point<system_clock, std::chrono::seconds> refTimeSec;
        bool updateStatus;
        int saturatedValue;
        circular_buffer<cv::Mat> satMap;

	bool printFrameStats;

    public :

        Mask(int timeInterval, bool customMask, string customMaskPath, bool downsampleMask, CamPixFmt format, bool updateMask);

        bool applyMask(cv::Mat &currFrame);

        void resetMask();

	void setFrameStats( bool frameStats );
};


