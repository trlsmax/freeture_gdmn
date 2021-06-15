/*
                                TimeDate.h

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture

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
* \file    TimeDate.h
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    13/06/2014
* \brief   Time helpers.
*/

#pragma once

#include "Conversion.h"
#include "TimeDate.h"
#include <chrono>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <ctime>

using namespace std;
using namespace std::chrono;

class TimeDate {

public:
    struct Date {
        std::chrono::system_clock::time_point tp;
        int year;
        int month;
        int day;
        int hours;
        int minutes;
        double seconds;

        Date()
            : year(0)
            , month(0)
            , day(0)
            , hours(0)
            , minutes(0)
            , seconds(0)
        {
            Reset();
        }

        Date(std::chrono::system_clock::time_point _tp)
            : tp(_tp)
        {
            Reset(tp);
        }

        void Reset(std::chrono::system_clock::time_point _tp = system_clock::now())
        {
            tp = _tp;
			auto t = system_clock::to_time_t(tp);
            auto time = std::localtime(&t);
            year = time->tm_year + 1900;
            month = time->tm_mon + 1;
            day = time->tm_mday;
            hours = time->tm_hour;
            minutes = time->tm_min;
            seconds = time->tm_sec;
        }
    };

    /**
        * Get local date time in UT
        *
        * @param pt Boost ptime.
        * @param format "%Y:%m:%d:%H:%M:%S"
        * @return Date time in string.
        */
    static string localDateTime(system_clock::time_point pt, const string& format);

    /**
        * Get date in UT
        *
        * @return YYYY, MM, DD.
        */
    static string getCurrentDateYYYYMMDD();

    /**
        * Convert gregorian date to julian date.
        *
        * @param date Vector of date : YYYY, MM, DD, hh, mm, ss.
        * @return Julian date.
        */
    static double gregorianToJulian(Date date);

    /**
        * Get julian century from julian date.
        *
        * @param julianDate.
        * @return Julian century.
        */
    static double julianCentury(double julianDate);

    /**
        * Convert Hours:Minutes:seconds to decimal hours.
        *
        * @param H hours.
        * @param M minutes.
        * @param S seconds.
        * @return decimal hours.
        */
    static double hmsToHdecimal(int H, int M, int S);

    /**
        * Convert decimal hours to Hours:Minutes:seconds.
        *
        * @param val Decimal hours.
        * @return Vector of Hours:Minutes:seconds.
        */
    static vector<int> HdecimalToHMS(double val);

    /**
        * Get local sideral time.
        *
        * @param julianCentury
        * @param gregorianH
        * @param gregorianMin
        * @param gregorianS
        * @param longitude
        * @return Local sideral time.
        */
    static double localSideralTime_2(double julianCentury, int gregorianH, int gregorianMin, int gregorianS, double longitude);

    /**
        * Get local sideral time.
        *
        * @param julianDate
        * @param gregorianH
        * @param gregorianMin
        * @param gregorianS
        * @return Local sideral time.
        */
    static double localSideralTime_1(double JD0, int gregorianH, int gregorianMin, int gregorianS);

    /**
        * Split string to int values according to the ":" separator.
        *
        * @param str String to split. Its format is YYYY:MM:DD:HH:MM:SS
        * @return Vector of int values.
        */
    static vector<int> splitStringToInt(string str);

    /**
        * Get YYYYMMDD from date string.
        *
        * @param date Date with the following format : YYYY-MM-DDTHH:MM:SS.fffffffff
        * @return YYYYMMDD.
        */
    static string getYYYYMMDDfromDateString(string date);

    // output : YYYYMMDD
    static string getYYYYMMDD(Date date);

    /**
        * Get year, month, date, hours, minutes and seconds from date string.
        *
        * @param date Date with the following format : YYYY-MM-DDTHH:MM:SS.fffffffff
        * @return Vector of int values.
        */
    static vector<int> getIntVectorFromDateString(string date);

    /**
        * Get YYYYMMJJTHHMMSS date.
        *
        * @param date YYYYMMJJTHHMMSS.fffffffff
        * @return YYYYMMJJTHHMMSS.
        */
    static string getYYYYMMDDThhmmss(string date);

    /**
        * Get YYYYMMJJTHHMMSS date.
        *
        * @param date YYYYMMJJTHHMMSS.fffffffff
        * @return YYYYMMJJTHHMMSS.
        */
    static string getYYYYMMDDThhmmss(Date date);

    static string getYYYY_MM_DD(Date date);

    static string getYYYY_MM_DD_hhmmss(string date);

    static string getYYYY_MM_DD_hhmmss(Date date);

    static string getYYYY(Date date);

    static string getMM(Date date);

    static string getDD(Date date);

    static Date splitIsoExtendedDate(string date);

    static string getIsoExtendedFormatDate(Date date);

    /**
        * Get seconds between two dates
        */
    static int secBetweenTwoDates(Date d1, Date d2);
    static vector<string> split(const char* str, char c = ' ');
    static string IsoExtendedStringNow(void);
};

