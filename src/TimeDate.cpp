/*
                                TimeDate.cpp

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
*   Last modified:      20/07/2015
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    TimeDate.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    13/06/2014
* \brief   Manage time.
*/

#include "TimeDate.h"
#include <ctime>
#include <date/date.h>
#include <iomanip>
#include <string>

//http://rhubbarb.wordpress.com/2009/10/17/boost-datetime-locales-and-facets/
string TimeDate::localDateTime(system_clock::time_point pt, const string& format)
{
    auto t = system_clock::to_time_t(pt);
    std::stringstream tmp;
    tmp << std::put_time(std::localtime(&t), format.c_str());
    return tmp.str();
}

string TimeDate::getCurrentDateYYYYMMDD()
{
    return localDateTime(system_clock::now(), "%Y%m%d");
}

vector<string> TimeDate::split(const char* str, char c)
{
    vector<string> result;
    do {
        const char* begin = str;
        while (*str != c && *str)
            str++;
        result.emplace_back(begin, str);
    } while (0 != *str++);
    return result;
}

double TimeDate::gregorianToJulian(Date date)
{
    if (date.month == 1 || date.month == 2) {

        date.year = date.year - 1;
        date.month = date.month + 12;
    }

    double A, A1, B, C, D;
    double f0 = modf(date.year / 100, &A);
    double f1 = modf(A / 4, &A1);
    B = 2 - A + A1;
    double f2 = modf(365.25 * date.year, &C);
    double f3 = modf(30.6001 * (date.month + 1), &D);

    std::ostringstream strs;
    double val = B + C + D + date.day + 1720994.5;
    strs << val;
    std::string str = strs.str();
    //cout <<std::setprecision(5)<< std::fixed<< val<<endl;

    return B + C + D + date.day + 1720994.5;
}

double TimeDate::julianCentury(double julianDate)
{
    return ((julianDate - 2451545.0) / 36525);
}

double TimeDate::hmsToHdecimal(int H, int M, int S)
{
    double res = H;
    double entPartM;
    double fractPartM = modf(M / 60, &entPartM);
    res = res + fractPartM;

    double entPartS;
    double fractPartS = modf(S / 3600, &entPartS);
    res = res + fractPartS;

    return res;
}

//conversion longitude degrées en H M S
vector<int> TimeDate::HdecimalToHMS(double val)
{
    vector<int> res;

    double entPart_h;
    double fractPart_ms_dec = modf(val, &entPart_h);

    double entPart_m;
    double fractPart_s_dec = modf(fractPart_ms_dec * 60, &entPart_m);

    double entPart_s = fractPart_s_dec * 60;

    res.push_back(entPart_h);
    res.push_back(entPart_m);
    res.push_back(entPart_s);

    return res;
}

//http://aa.usno.navy.mil/faq/docs/GAST.php
double TimeDate::localSideralTime_1(double JD0, int gregorianH, int gregorianMin, int gregorianS)
{
    double JD = JD0;
    double H = gregorianH + gregorianMin / 60 + gregorianS / 3600;

    hmsToHdecimal(gregorianH, gregorianMin, gregorianS);

    double D = JD - 2451545.0;

    double D0 = JD0 - 2451545.0;

    double T = D / 36525;

    double GMST = 6.697374558 + 0.06570982441908 * D0 + 1.00273790935 * H + 0.000026 * T * T;

    // the Longitude of the ascending node of the Moon
    double omega = 125.04 - 0.052954 * D; // in degree

    // the Mean Longitude of the Sun
    double L = 280.04 - 0.98565 * D; // in degree

    //is the obliquity
    double epsilon = 23.4393 - 0.0000004 * D; //in degree

    // the nutation in longitude, is given in hours approximately by
    double deltaPhi = -0.000319 * sin(omega) - 0.000024 * sin(2 * L);

    //The equation of the equinoxes
    double eqeq = deltaPhi * cos(epsilon);

    double GAST = GMST + eqeq;

    return GAST;
}

//http://jean-paul.cornec.pagesperso-orange.fr/gmst0.htm
double TimeDate::localSideralTime_2(double julianCentury, int gregorianH, int gregorianMin, int gregorianS, double longitude)
{
    double T = julianCentury;

    //Temps sidéral de Greenwich à 0h TU
    double GTSM0_sec = 24110.54841 + (8640184.812866 * julianCentury) + (0.093104 * pow(julianCentury, 2)) - (pow(julianCentury, 3) * 0.0000062);
    //cout << "GTSM0_sec "<< GTSM0_sec<<endl;

    double entPart_h;
    double fractPart_ms_dec = modf(GTSM0_sec / 3600, &entPart_h);
    //cout << "fractPart_ms_dec "<< fractPart_ms_dec<<endl;

    double entPart_m;
    double fractPart_s = modf(fractPart_ms_dec * 60, &entPart_m);
    //cout << "fractPart_s "<< fractPart_s<<endl;

    vector<int> GTSM0_hms;
    GTSM0_hms.push_back((int)entPart_h % 24); // H
    GTSM0_hms.push_back((int)entPart_m); // M
    GTSM0_hms.push_back((int)std::floor(fractPart_s * 60 + 0.5)); // S

    //cout << "GTSM0_hms-> "<<GTSM0_hms.at(0)<<"h "<<GTSM0_hms.at(1)<< "m "<<GTSM0_hms.at(2)<< "s"<<endl;

    //conversion longitude degrées en H décimal
    // double lon = 2.1794397;
    double lon_Hdec = longitude / 15;

    //cout << "LON_dec-> "<<lon_Hdec<<endl;

    //conversion longitude degrées en H M S
    double entPart_lon_h;
    double fractPart_lon_ms_dec = modf(lon_Hdec, &entPart_lon_h);

    double entPart_lon_m;
    double fractPart_lon_s_dec = modf(fractPart_lon_ms_dec * 60, &entPart_lon_m);

    double entPart_lon_s = fractPart_lon_s_dec * 60;

    //cout << "LON_hms-> "<<entPart_lon_h<<"h "<<entPart_lon_m<< "m "<<entPart_lon_s<< "s"<<endl;

    //cout << "LON_sec-> "<<entPart_lon_h*3600+entPart_lon_m*60+entPart_lon_s<<endl;

    //Conversion de l'heure en temps sidéral écoulé
    double HTS = (gregorianH * 3600 + gregorianMin * 60 + gregorianS) * 1.00273790935;
    //cout << "HTS_sec-> "<<HTS<<endl;

    double GTSMLocal_sec = GTSM0_sec + entPart_lon_h * 3600 + entPart_lon_m * 60 + entPart_lon_s + HTS;
    //cout << "GTSMLocal_sec-> "<<GTSMLocal_sec<<endl;

    double entPart_gtsm_h;
    double fractPart_gtsm_ms_dec = modf(GTSMLocal_sec / 3600, &entPart_gtsm_h);
    //  cout << "fractPart_gtsm_ms_dec "<< fractPart_gtsm_ms_dec<<endl;

    double entPart_gtsm_m;
    double fractPart_gtsm_s = modf(fractPart_gtsm_ms_dec * 60, &entPart_gtsm_m);
    //  cout << "fractPart_gtsm_s "<< fractPart_gtsm_s<<endl;

    vector<double> GTSM0Local_hms;
    GTSM0Local_hms.push_back((double)((int)entPart_gtsm_h % 24)); // H
    GTSM0Local_hms.push_back(entPart_gtsm_m); // M
    GTSM0Local_hms.push_back(fractPart_gtsm_s * 60); // S

    // cout << "GTSMLocal_hms-> "<<GTSM0Local_hms.at(0)<<"h "<<GTSM0Local_hms.at(1)<< "m "<<fractPart_gtsm_s*60<< "s"<<endl;

    double sideralT = 0.0;
    sideralT = GTSM0Local_hms.at(0) * 15 + GTSM0Local_hms.at(1) * 0.250 + GTSM0Local_hms.at(2) * 0.00416667;

    //cout << "sideralT in degree-> "<<sideralT<<endl;

    return sideralT;
}

vector<int> TimeDate::splitStringToInt(string str)
{
    vector<string> tmp = split(str.c_str(), ':');
    vector<int> intOutput;
    for (size_t i = 0; i < tmp.size(); i++) {
        intOutput.push_back(atoi(tmp[i].c_str()));
    }
    return intOutput;
}

// Input date format : YYYY:MM:DD from YYYY-MM-DDTHH:MM:SS,fffffffff
// Output : vector<int> with YYYY, MM, DD, hh, mm, ss
vector<int> TimeDate::getIntVectorFromDateString(string date)
{
    vector<string> output1;
    vector<string> output2;
    vector<string> output3;
    vector<string> output4;
    vector<int> finalOuput;

    finalOuput.push_back(atoi(date.substr(0, 4).c_str()));
    finalOuput.push_back(atoi(date.substr(5, 2).c_str()));
    finalOuput.push_back(atoi(date.substr(8, 2).c_str()));
    finalOuput.push_back(atoi(date.substr(11, 2).c_str()));
    finalOuput.push_back(atoi(date.substr(14, 2).c_str()));
    finalOuput.push_back(atoi(date.substr(17, string::npos).c_str()));

    return finalOuput;
}

// Input date format : YYYY-MM-DDTHH:MM:SS,fffffffff
// Output : YYYY, MM, DD, hh, mm, ss.ss
TimeDate::Date TimeDate::splitIsoExtendedDate(string date)
{
    Date res;
    res.year = atoi(date.substr(0, 4).c_str());
    res.month = atoi(date.substr(5, 2).c_str());
    res.day = atoi(date.substr(8, 2).c_str());
    res.hours = atoi(date.substr(11, 2).c_str());
    res.minutes = atoi(date.substr(14, 2).c_str());
    res.seconds = stod(date.substr(17, string::npos).c_str());

    return res;
}

// Input : YYYY, MM, DD, hh, mm, ss.ss
// Output  YYYY-MM-DDTHH:MM:SS,fffffffff
string TimeDate::getIsoExtendedFormatDate(Date date)
{
    string isoExtendedDate = Conversion::intToString(date.year) + "-" + Conversion::numbering(2, date.month) + Conversion::intToString(date.month) + "-" + Conversion::numbering(2, date.day) + Conversion::intToString(date.day) + "T" + Conversion::numbering(2, date.hours) + Conversion::intToString(date.hours) + ":" + Conversion::numbering(2, date.minutes) + Conversion::intToString(date.minutes) + ":" + Conversion::numbering(2, (int)date.seconds) + Conversion::doubleToString(date.seconds);
    return isoExtendedDate;
}

// Input date format : YYYY:MM:DD from YYYY-MM-DDTHH:MM:SS,fffffffff
string TimeDate::getYYYYMMDDfromDateString(string date)
{
    vector<string> output2;

    output2.push_back(date.substr(0, 4));
    output2.push_back(date.substr(5, 2));
    output2.push_back(date.substr(7, 2));

    // Build YYYYMMDD string.
    string yyyymmdd = "";
    for (int i = 0; i < output2.size(); i++) {

        yyyymmdd += output2.at(i);
    }

    return yyyymmdd;
}

// output : YYYYMMDDTHHMMSS
string TimeDate::getYYYYMMDDThhmmss(Date date)
{
    std::tm t = std::tm();
    t.tm_year = date.year;
    t.tm_mon = date.month;
    t.tm_mday = date.day;
    t.tm_hour = date.hours;
    t.tm_min = date.minutes;
    t.tm_sec = date.seconds;

    stringstream ss;
    ss << std::put_time(&t, "%Y%m%dT%H%M%S");
    return ss.str();
}

// output : YYYYMMDD
string TimeDate::getYYYYMMDD(Date date)
{
    std::tm t = std::tm();
    t.tm_year = date.year;
    t.tm_mon = date.month;
    t.tm_mday = date.day;

    stringstream ss;
    ss << std::put_time(&t, "%Y%m%d");
    return ss.str();
}

// output : YYYY-MM-DD
string TimeDate::getYYYY_MM_DD(Date date)
{
    std::tm t = std::tm();
    t.tm_year = date.year;
    t.tm_mon = date.month;
    t.tm_mday = date.day;

    stringstream ss;
    ss << std::put_time(&t, "%Y-%m-%d");
    return ss.str();
}

// output : YYYY-MM-DD_HHMMSS
string TimeDate::getYYYY_MM_DD_hhmmss(Date date)
{
    std::tm t = std::tm();
    t.tm_year = date.year;
    t.tm_mon = date.month;
    t.tm_mday = date.day;
    t.tm_hour = date.hours;
    t.tm_min = date.minutes;
    t.tm_sec = date.seconds;

    stringstream ss;
    ss << std::put_time(&t, "%Y-%m-%d_%H%M%S");
    return ss.str();
}

// output : YYYY
string TimeDate::getYYYY(Date date)
{
    string res = Conversion::numbering(4, date.year) + Conversion::intToString(date.year);
    return res;
}

// output : MM
string TimeDate::getMM(Date date)
{
    string res = Conversion::numbering(2, date.month) + Conversion::intToString(date.month);
    return res;
}

// output : DD
string TimeDate::getDD(Date date)
{
    string res = Conversion::numbering(2, date.day) + Conversion::intToString(date.day);
    return res;
}

string TimeDate::getYYYY_MM_DD_hhmmss(string date)
{
    Date dateobj = TimeDate::splitIsoExtendedDate(date);
    return TimeDate::getYYYY_MM_DD_hhmmss(dateobj);
}

string TimeDate::getYYYYMMDDThhmmss(string date)
{
    string finalDate = "";

    finalDate += date.substr(0, 4);
    finalDate += date.substr(5, 2);
    finalDate += date.substr(8, 2);
    finalDate += date.substr(11, 2);
    finalDate += date.substr(14, 2);
    finalDate += date.substr(17, string::npos);

    return finalDate;
}

int TimeDate::secBetweenTwoDates(Date d1, Date d2)
{
    std::tm t1 = std::tm();
    t1.tm_year = d1.year;
    t1.tm_mon = d1.month;
    t1.tm_mday = d1.day;
    t1.tm_hour = d1.hours;
    t1.tm_min = d1.minutes;
    t1.tm_sec = d1.seconds;
    std::time_t tt1 = std::mktime(&t1);

    std::tm t2 = std::tm();
    t2.tm_year = d2.year;
    t2.tm_mon = d2.month;
    t2.tm_mday = d2.day;
    t2.tm_hour = d2.hours;
    t2.tm_min = d2.minutes;
    t2.tm_sec = d2.seconds;
    std::time_t tt2 = std::mktime(&t2);

    std::chrono::system_clock::time_point tp1 = std::chrono::system_clock::from_time_t(tt1);
    std::chrono::system_clock::time_point tp2 = std::chrono::system_clock::from_time_t(tt2);
    std::chrono::system_clock::duration d = tp2 - tp1;

    return d.count();
}

string TimeDate::IsoExtendedStringNow()
{
    auto now = chrono::system_clock::now();
    return date::format("%FT%T", date::floor<std::chrono::milliseconds>(now));
}
