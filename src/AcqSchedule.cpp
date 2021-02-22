/*
                             AcqSchedule.cpp

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
 * \file     AcqSchedule.cpp
 * \author  Yoan Audureau -- GEOPS-UPSUD
 * \version 1.0
 * \date    19/06/2014
 * \brief
 */

#include "AcqSchedule.h"

AcqSchedule::AcqSchedule(int H, int M, int S, int E, int G, int F, int N)
    : mH(H), mM(M), mS(S), mE(E), mG(G), mN(N), mF(F) {}

AcqSchedule::AcqSchedule() : mH(0), mM(0), mS(0), mE(0), mG(0), mN(0), mF(0) {}

AcqSchedule::~AcqSchedule(){};

