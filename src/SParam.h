/*
                                SParam.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau
*                       2018 Chiara Marmo
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
*   Last modified:      20/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    SParam.h
* \author  Yoan Audureau -- Chiara Marmo -- GEOPS-UPSUD
* \version 1.2
* \date    20/03/2018
* \brief   FreeTure parameters
*/

#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include <stdlib.h>
#include "ECamPixFmt.h"
#include "ETimeMode.h"
#include "EImgFormat.h"
#include "EDetMeth.h"
#include "ELogSeverityLevel.h"
#include "EStackMeth.h"
#include "ESmtpSecurity.h"
#include <vector>
#include "EInputDeviceType.h"
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/imgproc/imgproc.hpp>
#include <toml.hpp>

using namespace std;

// ******************************************************
// ****************** MAIL PARAMETERS *******************
// ******************************************************

struct mailParam{
    bool            MAIL_DETECTION_ENABLED;
    string          MAIL_SMTP_SERVER;
    SmtpSecurity    MAIL_CONNECTION_TYPE;
    string          MAIL_SMTP_LOGIN;
    string          MAIL_SMTP_PASSWORD;
    vector<string>  MAIL_RECIPIENTS;

    void from_toml(const toml::value& v) {
        this->MAIL_DETECTION_ENABLED = toml::find_or<bool>(v, "MAIL_DETECTION_ENABLED", false);
        this->MAIL_SMTP_SERVER = toml::find_or<std::string>(v, "MAIL_SMTP_SERVER", "");
        this->MAIL_CONNECTION_TYPE = toml::find_or<SmtpSecurity>(v, "MAIL_CONNECTION_TYPE", SmtpSecurity::NO_SECURITY);
        this->MAIL_SMTP_LOGIN = toml::find_or<std::string>(v, "MAIL_SMTP_LOGIN", "");
        this->MAIL_SMTP_PASSWORD = toml::find_or<std::string>(v, "MAIL_SMTP_PASSWORD", "");
        this->MAIL_RECIPIENTS = toml::find_or<std::vector<std::string>>(v, "MAIL_RECIPIENTS", std::vector<std::string>());
    }

    toml::value into_toml() const {
        return toml::value{
            {"MAIL_DETECTION_ENABLED", this->MAIL_DETECTION_ENABLED},
            {"MAIL_SMTP_SERVER", this->MAIL_SMTP_SERVER},
            {"MAIL_CONNECTION_TYPE", this->MAIL_CONNECTION_TYPE},
            {"MAIL_SMTP_LOGIN", this->MAIL_SMTP_LOGIN},
            {"MAIL_SMTP_PASSWORD", this->MAIL_SMTP_PASSWORD},
            {"MAIL_RECIPIENTS", this->MAIL_RECIPIENTS}
        };
    }
};

// ******************************************************
// ******************* LOG PARAMETERS *******************
// ******************************************************

struct logParam {
    string              LOG_PATH;
    int                 LOG_ARCHIVE_DAY;
    int                 LOG_SIZE_LIMIT;
    LogSeverityLevel    LOG_SEVERITY;
};

// ******************************************************
// **************** OUTPUT DATA PARAMETERS **************
// ******************************************************

struct dataParam {
    string  DATA_PATH;
    bool    FITS_COMPRESSION;
    string  FITS_COMPRESSION_METHOD;
};

// ******************************************************
// **************** INPUT FRAMES PARAMETERS *************
// ******************************************************

struct framesParam {
    int INPUT_TIME_INTERVAL;
    vector<string> INPUT_FRAMES_DIRECTORY_PATH;

    void from_toml(const toml::value& v) {
        this->INPUT_FRAMES_DIRECTORY_PATH = toml::find_or<std::vector<std::string>>(v, "INPUT_FRAMES_DIRECTORY_PATH", std::vector<std::string>());
        this->INPUT_TIME_INTERVAL = toml::find_or<int>(v, "INPUT_TIME_INTERVAL", 0);
    }

    toml::value into_toml() const {
        return toml::value{
            {"MAIL_DETECTION_ENABLED", this->MAIL_DETECTION_ENABLED},
            {"MAIL_SMTP_SERVER", this->MAIL_SMTP_SERVER},
            {"MAIL_CONNECTION_TYPE", this->MAIL_CONNECTION_TYPE},
            {"MAIL_SMTP_LOGIN", this->MAIL_SMTP_LOGIN},
            {"MAIL_SMTP_PASSWORD", this->MAIL_SMTP_PASSWORD},
            {"MAIL_RECIPIENTS", this->MAIL_RECIPIENTS}
        };
    }
};

// ******************************************************
// **************** INPUT VIDEO PARAMETERS **************
// ******************************************************

struct videoParam {
    int INPUT_TIME_INTERVAL;
    vector<string> INPUT_VIDEO_PATH;
};

// ******************************************************
// ********* SCHEDULED ACQUISITION PARAMETERS ***********
// ******************************************************

struct scheduleParam {
    int hours;
    int min;
    int sec;
    int exp;
    int gain;
    int rep;
    CamPixFmt fmt;
};

// ******************************************************
// ************** INPUT CAMERA PARAMETERS ***************
// ******************************************************
struct scheduledCaptures {
	bool        ACQ_SCHEDULE_ENABLED;
	ImgFormat   ACQ_SCHEDULE_OUTPUT;
	vector<scheduleParam> ACQ_SCHEDULE;
};

struct ephemeris {
	bool    EPHEMERIS_ENABLED;
	double  SUN_HORIZON_1;
	double  SUN_HORIZON_2;
	vector<int>  SUNRISE_TIME;
	vector<int>  SUNSET_TIME;
	int     SUNSET_DURATION;
	int     SUNRISE_DURATION;
};

struct regularParam {
	int interval;
	int exp;
	int gain;
	int rep;
	CamPixFmt fmt;
};

struct regularCaptures {
	bool        ACQ_REGULAR_ENABLED;
	TimeMode    ACQ_REGULAR_MODE;
	string      ACQ_REGULAR_PRFX;
	ImgFormat   ACQ_REGULAR_OUTPUT;
	regularParam ACQ_REGULAR_CFG;
};

struct detectionMethod1 {
	bool    DET_SAVE_GEMAP;
	bool    DET_SAVE_DIRMAP;
	bool    DET_SAVE_POS;
	int     DET_LE_MAX;
	int     DET_GE_MAX;
	//bool    DET_SAVE_GE_INFOS;
};

// ******************************************************
// **************** DETECTION PARAMETERS ****************
// ******************************************************

struct detectionParam {
    int         ACQ_BUFFER_SIZE;
    bool        ACQ_MASK_ENABLED;
    string      ACQ_MASK_PATH;
    cv::Mat         MASK;
    bool        DET_ENABLED;
    TimeMode    DET_MODE;
    bool        DET_DEBUG;
    string      DET_DEBUG_PATH;
    int         DET_TIME_AROUND;
    int         DET_TIME_MAX;
    DetMeth     DET_METHOD;
    bool        DET_SAVE_FITS3D;
    bool        DET_SAVE_FITS2D;
    bool        DET_SAVE_SUM;
    bool        DET_SUM_REDUCTION;
    StackMeth   DET_SUM_MTHD;
    bool        DET_SAVE_SUM_WITH_HIST_EQUALIZATION;
    bool        DET_SAVE_AVI;
    bool        DET_UPDATE_MASK;
    int         DET_UPDATE_MASK_FREQUENCY;
    bool        DET_DEBUG_UPDATE_MASK;
    bool        DET_DOWNSAMPLE_ENABLED;

    detectionMethod1 temporal;

};

struct acqParam {
    double      ACQ_FPS;
    CamPixFmt   ACQ_FORMAT;
    bool        ACQ_RES_CUSTOM_SIZE;
    bool        SHIFT_BITS;
    int         ACQ_NIGHT_EXPOSURE;
    int         ACQ_NIGHT_GAIN;
    int         ACQ_DAY_EXPOSURE;
    int         ACQ_DAY_GAIN;
    int         ACQ_STARTX;
    int         ACQ_STARTY;
    int         ACQ_HEIGHT;
    int         ACQ_WIDTH;
    int         EXPOSURE_CONTROL_FREQUENCY;
    bool        EXPOSURE_CONTROL_SAVE_IMAGE;
    bool        EXPOSURE_CONTROL_SAVE_INFOS;
    framesParam     framesInput;
    videoParam      vidInput;
    cameraParam     camInput;
    regularCaptures regcap;
    scheduledCaptures schcap;
};

// ******************************************************
// ******************* STACK PARAMETERS *****************
// ******************************************************

struct stackParam{
    bool        STACK_ENABLED;
    TimeMode    STACK_MODE;
    int         STACK_TIME;
    int         STACK_INTERVAL;
    StackMeth   STACK_MTHD;
    bool        STACK_REDUCTION;
};

// ******************************************************
// ***************** FITS KEYS PARAMETERS ***************
// ******************************************************

struct fitskeysParam{
    string FILTER;
    double K1;
    double K2;
    string COMMENT;
    double CD1_1;
    double CD1_2;
    double CD2_1;
    double CD2_2;
    double XPIXEL;
    double YPIXEL;
};


struct cameraParam{
    pair<pair<int, bool>,string> DEVICE_ID; // Pair : <value, status>
    ephemeris ephem;
    acqParam acq;
    detectionParam det;
    stackParam stack;
    fitskeysParam fits;
};


// ******************************************************
// ****************** STATION PARAMETERS ****************
// ******************************************************

 struct stationParam {
    string STATION_NAME;
    string TELESCOP;
    string OBSERVER;
    string INSTRUME;
    string CAMERA;
    double FOCAL;
    double APERTURE;
    double SITELONG;
    double SITELAT;
    double SITEELEV;
    string GPS_LOCK;
};

// ******************************************************
// ****************** FREETURE PARAMETERS ***************
// ******************************************************

struct parameters {
    std::vector<cameraParam> cameras;
    dataParam       data;
    logParam        log;
    stationParam    station;
    mailParam       mail;
};

