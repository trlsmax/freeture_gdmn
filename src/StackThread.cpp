/*
                            StackThread.cpp

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
* \file    StackThread.cpp
* \author  Yoan Audureau -- GEOPS-UPSUD
* \version 1.0
* \date    19/06/2014
* \brief   Stack frames.
*/

#include "StackThread.h"
using namespace cv;

StackThread::StackThread(bool *sS,
                         mutex *sS_m,
                         condition_variable *sS_c,
                         circular_buffer<Frame> *fb,
                         mutex *fb_m,
                         condition_variable *fb_c,
                         dataParam dp,
                         stackParam sp,
                         stationParam stp,
                         CamPixFmt pfmt,
                         fitskeysParam fkp)
{

    mThread = NULL;
    mustStop = false;
    frameBuffer = fb;
    frameBuffer_mutex = fb_m;
    frameBuffer_condition = fb_c;
    stackSignal = sS;
    stackSignal_mutex = sS_m;
    stackSignal_condition = sS_c;
    completeDataPath = "";
    isRunning = false;
    interruptionStatus = false;
    mdp = dp;
    mstp = stp;
    msp = sp;
    mfkp = fkp;
    mPixfmt = pfmt;

    printFrameStats = false;
}

StackThread::~StackThread(void)
{

    auto logger = spdlog::get("stack_logger");
    logger->info("Cleaning ressources and deleting StackThread...");

    if (mThread != NULL)
        delete mThread;

}

bool StackThread::startThread()
{

    auto logger = spdlog::get("stack_logger");
    logger->info("Creating StackThread...");
    mThread = new std::thread(&StackThread::run, this);
    mThread->detach();
    return true;
}

void StackThread::stopThread()
{

    // Signal the thread to stop (thread-safe)
    mustStopMutex.lock();
    mustStop = true;
    mustStopMutex.unlock();

    interruptThread();
    stackSignal_condition->notify_one();

    mThread->join();
    delete mThread;
    mThread = nullptr;
}

bool StackThread::interruptThread()
{

    auto logger = spdlog::get("stack_logger");
    interruptionStatusMutex.lock();
    interruptionStatus = true;
    logger->info("StackThread interruption.");
    interruptionStatusMutex.unlock();
    return true;

}

bool StackThread::buildStackDataDirectory(TimeDate::Date date)
{

    namespace fs = ghc::filesystem;
    auto logger = spdlog::get("stack_logger");
    string YYYYMMDD = TimeDate::getYYYYMMDD(date);
    //string root = mdp.DATA_PATH + mstp.STATION_NAME + "_" + YYYYMMDD +"/";
    string root = DataPaths::getSessionPath(mdp.DATA_PATH, date);


    //string subDir = "stacks/";
    // stop making sub-directories
    string subDir = "";
    string finalPath = root + subDir;

    completeDataPath = finalPath;

    if (YYYYMMDD == "00000000")
        return false;


    logger->info("Stacks data path : {}", completeDataPath);

    fs::path p(mdp.DATA_PATH);
    fs::path p1(root);
    fs::path p2(root + subDir);

    // If DATA_PATH exists
    if (fs::exists(p)) {
        // If DATA_PATH/STATION_YYYYMMDD/ exists
        if (fs::exists(p1)) {
            // If DATA_PATH/STATION_YYYYMMDD/stacks/ doesn't exists
            if (!fs::exists(p2)) {
                // If fail to create DATA_PATH/STATION_YYYYMMDD/stacks/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create stacks directory : {}", p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/stacks/
                } else {
                    logger->info("Success to create stacks directory : {}", p2.string());
                    return true;
                }
            }
            // If DATA_PATH/STATION_YYYYMMDD/ doesn't exists
        } else {
            // If fail to create DATA_PATH/STATION_YYYYMMDD/
            if (!fs::create_directory(p1)) {
                logger->error("Unable to create STATION_YYYYMMDD directory : {}", p1.string());
                return false;
                // If success to create DATA_PATH/STATION_YYYYMMDD/
            } else {
                logger->info("Success to create STATION_YYYYMMDD directory : {}", p1.string());
                // If fail to create DATA_PATH/STATION_YYYYMMDD/stacks/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create stacks directory : {}", p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/stacks/
                } else {
                    logger->info("Success to create stacks directory : {}", p2.string());
                    return true;
                }
            }
        }

        // If DATA_PATH doesn't exists
    } else {
        // If fail to create DATA_PATH
        if (!fs::create_directory(p)) {
            logger->error("Unable to create DATA_PATH directory : {}", p.string());
            return false;
            // If success to create DATA_PATH
        } else {
            logger->info("Success to create DATA_PATH directory : {}", p.string());
            // If fail to create DATA_PATH/STATION_YYYYMMDD/
            if (!fs::create_directory(p1)) {
                logger->error("Unable to create STATION_YYYYMMDD directory : {}", p1.string());
                return false;
                // If success to create DATA_PATH/STATION_YYYYMMDD/
            } else {
                logger->info("Success to create STATION_YYYYMMDD directory : {}", p1.string());
                // If fail to create DATA_PATH/STATION_YYYYMMDD/stacks/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create stacks directory : {}", p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/stacks/
                } else {
                    logger->info("Success to create stacks directory : {}", p2.string());
                    return true;
                }
            }
        }
    }

    return true;
}

bool StackThread::getRunStatus()
{

    return isRunning;

}

void StackThread::setFrameStats(bool frameStats)
{
    printFrameStats = frameStats;
}

void StackThread::run()
{

    bool stop = false;
    isRunning = true;
    auto logger = spdlog::get("stack_logger");

    logger->info("==============================================");
    logger->info("============== Start stack thread ============");
    logger->info("==============================================");

    try {
        do {
            try {
                // Thread is sleeping...
                std::this_thread::sleep_for(std::chrono::milliseconds(msp.STACK_INTERVAL * 1000));

                // Create a stack to accumulate n frames.
                Stack frameStack = Stack(mdp.FITS_COMPRESSION_METHOD, mfkp, mstp);

                // First reference date.
                time_point<system_clock, std::chrono::seconds> refTimeSec =
                        time_point_cast<std::chrono::seconds>(system_clock::now());

                long secTime = 0;
                do {
                    // Communication with AcqThread. Wait for a new frame.
                    std::unique_lock<std::mutex> lock(*stackSignal_mutex);
                    while (!(*stackSignal) && !interruptionStatus) stackSignal_condition->wait(lock);
                    *stackSignal = false;
                    lock.unlock();

                    double t = (double) getTickCount();

                    // Check interruption signal from AcqThread.
                    bool forceToSave = false;
                    interruptionStatusMutex.lock();
                    if (interruptionStatus) forceToSave = true;
                    interruptionStatusMutex.unlock();

                    if (!forceToSave) {

                        // Fetch last frame grabbed.
                        std::unique_lock<std::mutex> lock2(*frameBuffer_mutex);
                        if (frameBuffer->size() == 0) {
                            throw "SHARED CIRCULAR BUFFER SIZE = 0 -> STACK INTERRUPTION.";
                        }
                        Frame newFrame = frameBuffer->back();
                        lock2.unlock();

                        // Add the new frame to the stack.
                        frameStack.addFrame(newFrame);

                        t = (((double) getTickCount() - t) / getTickFrequency()) * 1000;
                        if (printFrameStats) {
                            spdlog::info("\033[15;0H [ TIME STACK ] : {} ms", t);
                        }
                        logger->info("[ TIME STACK ] : {} ms", t);
                    } else {
                        // Interruption is active.
                        logger->info("Interruption status : {}", forceToSave);

                        // Interruption operations terminated. Rest interruption signal.
                        interruptionStatusMutex.lock();
                        interruptionStatus = false;
                        interruptionStatusMutex.unlock();
                        break;
                    }

                    time_point<system_clock, std::chrono::seconds> nowTimeSec =
                            time_point_cast<std::chrono::seconds>(system_clock::now());

                    auto d = nowTimeSec - refTimeSec;
                    secTime = d.count();
                    if (printFrameStats) {
                        spdlog::info("\033[16;0H NEXT STACK : {}s", (int) (msp.STACK_TIME - secTime));
                    }
                } while (secTime <= msp.STACK_TIME);

                // Stack finished. Save it.
                if (frameStack.getNbFramesStacked() > 0) {
                    if (buildStackDataDirectory(frameStack.getDateFirstFrame())) {
                        if (!frameStack.saveStack(completeDataPath, msp.STACK_MTHD, msp.STACK_REDUCTION)) {
                            logger->error("Fail to save stack.");
                        }
                        logger->info("Stack saved : {}", completeDataPath);
                    } else {
                        logger->error("Fail to build stack directory. ");
                    }
                }
            } catch (std::exception &e) {
                logger->info("Stack thread INTERRUPTED");
                spdlog::info("Stack thread INTERRUPTED : {}", e.what());
            }

            // Get the "must stop" state (thread-safe)
            mustStopMutex.lock();
            stop = mustStop;
            mustStopMutex.unlock();
        } while (!stop);
    } catch (const char *msg) {
        logger->critical(msg);
    } catch (exception &e) {
        logger->critical(e.what());
    }

    isRunning = false;
    logger->info("StackThread ended.");
}

