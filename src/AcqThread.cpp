/*
                            AcqThread.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau, Chiara Marmo
*                               GEOPS-UPSUD-CNRS
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
 * \file    AcqThread.cpp
 * \author  Yoan Audureau, Chiara Marmo -- GEOPS-UPSUD
 * \version 1.0
 * \date    21/01/2015
 * \brief   Acquisition thread.
 */

#include "AcqThread.h"
#include <chrono>
#include <spdlog/spdlog.h>
using namespace std::chrono;
using namespace cv;

AcqThread::AcqThread(circular_buffer<Frame>* fb,
    mutex* fb_m,
    condition_variable* fb_c,
    bool* sSignal,
    mutex* sSignal_m,
    condition_variable* sSignal_c,
    bool* dSignal,
    mutex* dSignal_m,
    condition_variable* dSignal_c,
    DetThread* detection,
    StackThread* stack,
    int cid,
    dataParam dp,
    stackParam sp,
    stationParam stp,
    detectionParam dtp,
    cameraParam acq,
    framesParam fp,
    videoParam vp,
    fitskeysParam fkp)
{
    frameBuffer = fb;
    frameBuffer_mutex = fb_m;
    frameBuffer_condition = fb_c;
    stackSignal = sSignal;
    stackSignal_mutex = sSignal_m;
    stackSignal_condition = sSignal_c;
    detSignal = dSignal;
    detSignal_mutex = dSignal_m;
    detSignal_condition = dSignal_c;
    pDetection = detection;
    pStack = stack;
    mThread = NULL;
    mMustStop = false;
    mDevice = NULL;
    mThreadTerminated = false;
    mNextAcqIndex = 0;
    pExpCtrl = NULL;
    mDeviceID = cid;
    mdp = dp;
    msp = sp;
    mstp = stp;
    mdtp = dtp;
    mcp = acq;
    mvp = vp;
    mfp = fp;

    printFrameStats = false;
}

AcqThread::~AcqThread(void)
{
    if (mDevice != NULL)
        delete mDevice;

    if (mThread != NULL)
        delete mThread;

    if (pExpCtrl != NULL)
        delete pExpCtrl;
}

void AcqThread::stopThread()
{
    if (mThread != nullptr) {
        mMustStopMutex.lock();
        mMustStop = true;
        mMustStopMutex.unlock();
        mThread->join();
        delete mThread;
        mThread = nullptr;
    }
}

bool AcqThread::startThread()
{
    // Create a device.
    mDevice = new Device(mcp, mfp, mvp, mDeviceID);

    // Search available devices.
    mDevice->listDevices(false);

    // CREATE CAMERA
    if (!mDevice->createCamera())
        return false;

    // Prepare continuous acquisition.
    if (!prepareAcquisitionOnDevice())
        return false;

    // Create acquisition thread.
    mThread = new thread(&AcqThread::run, this);
    mThread->detach();

    return true;
}

bool AcqThread::getThreadStatus() const
{
    return mThreadTerminated;
}

void AcqThread::setFrameStats(bool frameStats)
{
    printFrameStats = frameStats;
}

void AcqThread::run()
{
    bool stop = false;
    auto logger = spdlog::get("acq_logger");

    logger->info("==============================================");
    logger->info("========== START ACQUISITION THREAD ==========");
    logger->info("==============================================");

    try {
        // Search next acquisition according to the current time.
        selectNextAcquisitionSchedule(
            TimeDate::splitIsoExtendedDate(TimeDate::IsoExtendedStringNow()));

        // Exposure adjustment variables.
        bool exposureControlStatus = false;
        bool exposureControlActive = false;
        bool cleanStatus = false;

        // If exposure can be set on the input device.
        if (mDevice->getExposureStatus()) {
            pExpCtrl = new ExposureControl(mcp.EXPOSURE_CONTROL_FREQUENCY,
                mcp.EXPOSURE_CONTROL_SAVE_IMAGE,
                mcp.EXPOSURE_CONTROL_SAVE_INFOS,
                mdp.DATA_PATH, mstp.STATION_NAME);
            pExpCtrl->setFrameStats(this->printFrameStats);
        }

        TimeMode previousTimeMode = NONE;

        /// Acquisition process.
        do {
            // Location of a video or frames if input type is FRAMES or VIDEO.
            string location = "";

            // Load videos file or frames directory if input type is FRAMES or
            // VIDEO
            if (!mDevice->loadNextCameraDataSet(location))
                break;

            if (pDetection != nullptr)
                pDetection->setCurrentDataSet(location);

            // Reference time to compute interval between regular captures.
            time_point<system_clock, std::chrono::seconds> refTimeSec =
                    time_point_cast<std::chrono::seconds>(system_clock::now());

            do {
                // Container for the grabbed image.
                Frame newFrame;

                // Time counter of grabbing a frame.
                auto tacq = (double)getTickCount();

                // Grab a frame.
                if (mDevice->runContinuousCapture(newFrame)) {
                    logger->info("============= FRAME {} =============",
                        newFrame.mFrameNumber);
                    if (printFrameStats) {
                        spdlog::info("============= FRAME {} =============",
                            newFrame.mFrameNumber);
                    }

                    // If camera type in input is FRAMES or VIDEO.
                    if (mDevice->mVideoFramesInput) {
                        // Push the new frame in the framebuffer.
                        std::unique_lock<std::mutex> lock(*frameBuffer_mutex);
                        frameBuffer->push_back(newFrame);
                        lock.unlock();

                        // Notify detection thread.
                        if (pDetection != NULL) {
                            std::unique_lock<std::mutex> lock2(
                                *detSignal_mutex);
                            *detSignal = true;
                            detSignal_condition->notify_one();
                            lock2.unlock();
                        }

                        // Slow down the time in order to give more time to the
                        // detection process.
                        int twait = 100;
                        if (mvp.INPUT_TIME_INTERVAL == 0 && mfp.INPUT_TIME_INTERVAL > 0)
                            twait = mfp.INPUT_TIME_INTERVAL;
                        else if (mvp.INPUT_TIME_INTERVAL > 0 && mfp.INPUT_TIME_INTERVAL == 0)
                            twait = mvp.INPUT_TIME_INTERVAL;
#ifdef WINDOWS
                        Sleep(twait);
#else
#ifdef LINUX
                        usleep(twait * 1000);
#endif
#endif

                    } else {
                        // Get current time in seconds.
                        int currentTimeInSec = newFrame.mDate.hours * 3600 + newFrame.mDate.minutes * 60 + (int)newFrame.mDate.seconds;

                        // Detect day or night.
                        TimeMode currentTimeMode = NONE;

                        if ((currentTimeInSec > mStopSunsetTime) || (currentTimeInSec < mStartSunriseTime)) {
                            currentTimeMode = NIGHT;
                        } else if ((currentTimeInSec > mStartSunriseTime) && (currentTimeInSec < mStopSunsetTime)) {
                            currentTimeMode = DAY;
                        }

                        // If exposure control is not active, the new frame can
                        // be shared with others threads.
                        if (!exposureControlStatus) {
                            // Push the new frame in the framebuffer.
                            std::unique_lock<std::mutex> lock(
                                *frameBuffer_mutex);
                            frameBuffer->push_back(newFrame);
                            lock.unlock();

                            // Notify detection thread.
                            if (pDetection != nullptr) {
                                if (previousTimeMode != currentTimeMode && mdtp.DET_MODE != DAYNIGHT) {
                                    logger->info("TimeMode has changed ! ");
                                    std::unique_lock<std::mutex> lock2(
                                        *detSignal_mutex);
                                    *detSignal = false;
                                    lock2.unlock();
                                    spdlog::info(
                                        "Send interruption signal to detection "
                                        "process ");
                                    pDetection->interruptThread();
                                } else if (mdtp.DET_MODE == currentTimeMode || mdtp.DET_MODE == DAYNIGHT) {
                                    std::unique_lock<std::mutex> lock2(
                                        *detSignal_mutex);
                                    *detSignal = true;
                                    detSignal_condition->notify_one();
                                    lock2.unlock();
                                }
                            }

                            // Notify stack thread.
                            if (pStack != nullptr) {
                                // TimeMode has changed.
                                if (previousTimeMode != currentTimeMode && msp.STACK_MODE != DAYNIGHT) {
                                    logger->info("TimeMode has changed ! ");
                                    std::unique_lock<std::mutex> lock2(
                                        *stackSignal_mutex);
                                    *stackSignal = false;
                                    lock2.unlock();

                                    // Force interruption.
                                    spdlog::info(
                                        "Send interruption signal to stack ");
                                    pStack->interruptThread();
                                } else if (msp.STACK_MODE == currentTimeMode || msp.STACK_MODE == DAYNIGHT) {
                                    std::unique_lock<std::mutex> lock2(
                                        *stackSignal_mutex);
                                    *stackSignal = true;
                                    stackSignal_condition->notify_one();
                                    lock2.unlock();
                                }
                            }
                            cleanStatus = false;
                        } else {
                            // Exposure control is active, the new frame can't
                            // be shared with others threads.
                            if (!cleanStatus) {
                                // If stack process exists.
                                if (pStack != NULL) {
                                    std::unique_lock<std::mutex> lock(
                                        *stackSignal_mutex);
                                    *stackSignal = false;
                                    lock.unlock();

                                    // Force interruption.
                                    spdlog::info(
                                        "Send interruption signal to stack ");
                                    pStack->interruptThread();
                                }

                                // If detection process exists
                                if (pDetection != NULL) {
                                    std::unique_lock<std::mutex> lock(
                                        *detSignal_mutex);
                                    *detSignal = false;
                                    lock.unlock();
                                    spdlog::info(
                                        "Sending interruption signal to "
                                        "detection process... ");
                                    pDetection->interruptThread();
                                }

                                // Reset framebuffer.
                                spdlog::info("Cleaning frameBuffer...");
                                std::unique_lock<std::mutex> lock(
                                    *frameBuffer_mutex);
                                frameBuffer->clear();
                                lock.unlock();

                                cleanStatus = true;
                            }
                        }

                        previousTimeMode = currentTimeMode;

                        // Adjust exposure time.
                        if (pExpCtrl != NULL && exposureControlActive)
                            exposureControlStatus = pExpCtrl->controlExposureTime(
                                mDevice, newFrame.mImg, newFrame.mDate,
                                mdtp.MASK, mDevice->mMinExposureTime,
                                mcp.ACQ_FPS);

                        // Get current date YYYYMMDD.
                        string currentFrameDate = TimeDate::getYYYYMMDD(newFrame.mDate);

                        // If the date has changed, sun ephemeris must be
                        // updated.
                        if (currentFrameDate != mCurrentDate) {
                            logger->info(
                                "Date has changed. Former Date is {} . New "
                                "Date is {}",
                                mCurrentDate, currentFrameDate);
                            computeSunTimes();
                        }

                        // Acquisition at regular time interval is enabled.
                        if (mcp.regcap.ACQ_REGULAR_ENABLED && !mDevice->mVideoFramesInput) {
                            time_point<system_clock, std::chrono::seconds> nowTimeSec =
                                    time_point_cast<std::chrono::seconds>(system_clock::now());

                            auto d = nowTimeSec - refTimeSec;
                            long secTime = d.count();
                            if (printFrameStats) {
                                spdlog::info(
                                    "\033[2;0H NEXT REGCAP : {} s",
                                    (int)(mcp.regcap.ACQ_REGULAR_CFG.interval - secTime));
                            }

                            // Check it's time to run a regular capture.
                            if (secTime >= mcp.regcap.ACQ_REGULAR_CFG.interval) {
                                // Current time is after the sunset stop and
                                // before the sunrise start = NIGHT
                                if ((currentTimeMode == NIGHT) && (mcp.regcap.ACQ_REGULAR_MODE == NIGHT || mcp.regcap.ACQ_REGULAR_MODE == DAYNIGHT)) {
                                    logger->info("Run regular acquisition.");

                                    // MC: this is calibration image
                                    runImageCapture(
                                        mcp.regcap.ACQ_REGULAR_CFG.rep,
                                        mcp.regcap.ACQ_REGULAR_CFG.exp,
                                        mcp.regcap.ACQ_REGULAR_CFG.gain,
                                        mcp.regcap.ACQ_REGULAR_CFG.fmt,
                                        mcp.regcap.ACQ_REGULAR_OUTPUT,
                                        mcp.regcap.ACQ_REGULAR_PRFX);

                                    // Current time is between sunrise start and
                                    // sunset stop = DAY
                                } else if (currentTimeMode == DAY && (mcp.regcap.ACQ_REGULAR_MODE == DAY || mcp.regcap.ACQ_REGULAR_MODE == DAYNIGHT)) {
                                    logger->info("Run regular acquisition.");
                                    saveImageCaptured(
                                        newFrame, 0,
                                        mcp.regcap.ACQ_REGULAR_OUTPUT,
                                        mcp.regcap.ACQ_REGULAR_PRFX);
                                }

                                // Reset reference time in case a long exposure
                                // has been done.
                                refTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());
                            }
                        }

                        // Acquisiton at scheduled time is enabled.
                        if (!mcp.schcap.ACQ_SCHEDULE.empty() && mcp.schcap.ACQ_SCHEDULE_ENABLED && !mDevice->mVideoFramesInput) {
                            int next = (mNextAcq.hours * 3600 + mNextAcq.min * 60 + mNextAcq.sec) - (newFrame.mDate.hours * 3600 + newFrame.mDate.minutes * 60 + newFrame.mDate.seconds);

                            if (next < 0) {
                                next = (24 * 3600) - (newFrame.mDate.hours * 3600 + newFrame.mDate.minutes * 60 + newFrame.mDate.seconds) + (mNextAcq.hours * 3600 + mNextAcq.min * 60 + mNextAcq.sec);
                                spdlog::info("next : {}", next);
                            }

                            vector<int> tsch = TimeDate::HdecimalToHMS(next / 3600.0);

                            if (printFrameStats) {
                                spdlog::info(
                                    "\033[3;0H NEXT SCHCAP : {}h {}m {}s",
                                    tsch.at(0), tsch.at(1), tsch.at(2));
                            }

                            // It's time to run scheduled acquisition.
                            if (mNextAcq.hours == newFrame.mDate.hours && mNextAcq.min == newFrame.mDate.minutes && (int)newFrame.mDate.seconds == mNextAcq.sec) {
                                CamPixFmt format;
                                format = mNextAcq.fmt;

                                runImageCapture(
                                    mNextAcq.rep, mNextAcq.exp, mNextAcq.gain,
                                    format, mcp.schcap.ACQ_SCHEDULE_OUTPUT, "");

                                // Update mNextAcq
                                selectNextAcquisitionSchedule(newFrame.mDate);

                            } else {
                                // The current time has elapsed.
                                if (newFrame.mDate.hours > mNextAcq.hours) {
                                    selectNextAcquisitionSchedule(
                                        newFrame.mDate);

                                } else if (newFrame.mDate.hours == mNextAcq.hours) {
                                    if (newFrame.mDate.minutes > mNextAcq.min) {
                                        selectNextAcquisitionSchedule(
                                            newFrame.mDate);

                                    } else if (newFrame.mDate.minutes == mNextAcq.min) {
                                        if (newFrame.mDate.seconds > mNextAcq.sec) {
                                            selectNextAcquisitionSchedule(
                                                newFrame.mDate);
                                        }
                                    }
                                }
                            }
                        }

                        // Check sunrise and sunset time.
                        if ((((currentTimeInSec > mStartSunriseTime && currentTimeInSec < mStopSunriseTime) || (currentTimeInSec > mStartSunsetTime && currentTimeInSec < mStopSunsetTime))) && !mDevice->mVideoFramesInput) {
                            exposureControlActive = true;

                        } else {
                            // Print time before sunrise.
                            if (currentTimeInSec < mStartSunriseTime || currentTimeInSec > mStopSunsetTime) {
                                vector<int> nextSunrise;
                                if (currentTimeInSec < mStartSunriseTime)
                                    nextSunrise = TimeDate::HdecimalToHMS(
                                        (mStartSunriseTime - currentTimeInSec) / 3600.0);
                                if (currentTimeInSec > mStopSunsetTime)
                                    nextSunrise = TimeDate::HdecimalToHMS(
                                        ((24 * 3600 - currentTimeInSec) + mStartSunriseTime) / 3600.0);

                                if (printFrameStats) {
                                    spdlog::info(
                                        "\033[4;0H NEXT SUNRISE : {}h {}m {}s",
                                        nextSunrise.at(0), nextSunrise.at(1),
                                        nextSunrise.at(2));
                                }
                            }

                            // Print time before sunset.
                            if (currentTimeInSec > mStopSunriseTime && currentTimeInSec < mStartSunsetTime) {
                                vector<int> nextSunset;
                                nextSunset = TimeDate::HdecimalToHMS(
                                    (mStartSunsetTime - currentTimeInSec) / 3600.0);
                                if (printFrameStats) {
                                    spdlog::info(
                                        "\033[5;0H NEXT SUNSET : {}h {}m {}s",
                                        nextSunset.at(0), nextSunset.at(1),
                                        nextSunset.at(2));
                                }
                            }

                            // Reset exposure time when sunrise or sunset is
                            // finished.
                            if (exposureControlActive) {
                                // In DAYTIME : Apply minimum available exposure
                                // time.
                                if ((currentTimeInSec >= mStopSunriseTime && currentTimeInSec < mStartSunsetTime)) {
                                    logger->info("Apply day exposure time : {}",
                                        mDevice->getDayExposureTime());
                                    mDevice->setCameraDayExposureTime();
                                    logger->info("Apply day exposure time : {}",
                                        mDevice->getDayGain());
                                    mDevice->setCameraDayGain();

                                    // In NIGHTTIME : Apply maximum available
                                    // exposure time.
                                } else if ((currentTimeInSec >= mStopSunsetTime) || (currentTimeInSec < mStartSunriseTime)) {
                                    logger->info(
                                        "Apply night exposure time. {}",
                                        mDevice->getNightExposureTime());
                                    mDevice->setCameraNightExposureTime();
                                    logger->info(
                                        "Apply night exposure time. {}",
                                        mDevice->getNightGain());
                                    mDevice->setCameraNightGain();
                                }
                            }

                            exposureControlActive = false;
                            exposureControlStatus = false;
                        }
                    }
                }

                tacq = (((double)getTickCount() - tacq) / getTickFrequency()) * 1000;
                if (printFrameStats) {
                    spdlog::info("\033[6;0H [ TIME ACQ ] : {} ms ~cFPS({})",
                        tacq, (1.0 / (tacq / 1000.0)));
                }
                logger->info(" [ TIME ACQ ] : {}", tacq);
                mMustStopMutex.lock();
                stop = mMustStop;
                mMustStopMutex.unlock();
            } while (!stop && !mDevice->getCameraStatus());

            // Reset detection process to prepare the analyse of a new data set.
            if (pDetection != nullptr) {
                pDetection->getDetMethod()->resetDetection(true);
                pDetection->getDetMethod()->resetMask();
                pDetection->updateDetectionReport();
                if (!pDetection->getRunStatus())
                    break;
            }

            // Clear framebuffer.
            std::unique_lock<std::mutex> lock(*frameBuffer_mutex);
            frameBuffer->clear();
            lock.unlock();
        } while (mDevice->getCameraDataSetStatus() && !stop);
#if 0
    } catch (const boost::thread_interrupted&) {
        spdlog::info("AcqThread ended.");
        logger->info("AcqThread ended.");
#endif
    } catch (exception& e) {
        spdlog::critical("An exception occured : {}", e.what());
        logger->critical("An exception occured : {}", e.what());
    } catch (const char* msg) {
        spdlog::info(msg);
    }

    mDevice->stopCamera();
    mThreadTerminated = true;
    spdlog::info("Acquisition Thread TERMINATED.");
    logger->info("Acquisition Thread TERMINATED");
}

void AcqThread::selectNextAcquisitionSchedule(TimeDate::Date date)
{
    if (!mcp.schcap.ACQ_SCHEDULE.empty()) {
        // Search next acquisition
        for (int i = 0; i < mcp.schcap.ACQ_SCHEDULE.size(); i++) {
            if (date.hours < mcp.schcap.ACQ_SCHEDULE.at(i).hours) {
                mNextAcqIndex = i;
                break;

            } else if (date.hours == mcp.schcap.ACQ_SCHEDULE.at(i).hours) {
                if (date.minutes < mcp.schcap.ACQ_SCHEDULE.at(i).min) {
                    mNextAcqIndex = i;
                    break;

                } else if (date.minutes == mcp.schcap.ACQ_SCHEDULE.at(i).min) {
                    if (date.seconds < mcp.schcap.ACQ_SCHEDULE.at(i).sec) {
                        mNextAcqIndex = i;
                        break;
                    }
                }
            }
        }

        mNextAcq = mcp.schcap.ACQ_SCHEDULE.at(mNextAcqIndex);
    }
}

bool AcqThread::buildAcquisitionDirectory(TimeDate::Date date)
{
    namespace fs = ghc::filesystem;
    auto logger = spdlog::get("acq_logger");
    // string root = mdp.DATA_PATH + mstp.STATION_NAME + "_" + YYYYMMDD +"/";
    string root = DataPaths::getSessionPath(mdp.DATA_PATH, date);

    // string subDir = "captures/";
    string subDir;
    string finalPath = root + subDir;

    mOutputDataPath = finalPath;
    logger->info("CompleteDataPath : {}", mOutputDataPath);

    fs::path p(mdp.DATA_PATH);
    fs::path p1(root);
    fs::path p2(root + subDir);

    // If DATA_PATH exists
    if (fs::exists(p)) {
        // If DATA_PATH/STATI ON_YYYYMMDD/ exists
        if (fs::exists(p1)) {
            // If DATA_PATH/STATION_YYYYMMDD/captures/ doesn't exists
            if (!fs::exists(p2)) {
                // If fail to create DATA_PATH/STATION_YYYYMMDD/captures/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create captures directory : {}",
                        p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/captures/
                } else {
                    logger->info("Success to create captures directory : {}",
                        p2.string());
                    return true;
                }
            }
            // If DATA_PATH/STATION_YYYYMMDD/ doesn't exists
        } else {
            // If fail to create DATA_PATH/STATION_YYYYMMDD/
            if (!fs::create_directory(p1)) {
                logger->error(
                    "Unable to create STATION_YYYYMMDD directory : {}",
                    p1.string());
                return false;
                // If success to create DATA_PATH/STATION_YYYYMMDD/
            } else {
                logger->info(
                    "Success to create STATION_YYYYMMDD directory : {}",
                    p1.string());
                // If fail to create DATA_PATH/STATION_YYYYMMDD/stack/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create captures directory : {}",
                        p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/stack/
                } else {
                    logger->info("Success to create captures directory : {}",
                        p2.string());
                    return true;
                }
            }
        }

        // If DATA_PATH doesn't exists
    } else {
        // If fail to create DATA_PATH
        if (!fs::create_directory(p)) {
            logger->error("Unable to create DATA_PATH directory : {}",
                p.string());
            return false;
            // If success to create DATA_PATH
        } else {
            logger->info("Success to create DATA_PATH directory : {}",
                p.string());
            // If fail to create DATA_PATH/STATION_YYYYMMDD/
            if (!fs::create_directory(p1)) {
                logger->error(
                    "Unable to create STATION_YYYYMMDD directory : {}",
                    p1.string());
                return false;
                // If success to create DATA_PATH/STATION_YYYYMMDD/
            } else {
                logger->info(
                    "Success to create STATION_YYYYMMDD directory : {}",
                    p1.string());
                // If fail to create DATA_PATH/STATION_YYYYMMDD/captures/
                if (!fs::create_directory(p2)) {
                    logger->critical("Unable to create captures directory : {}",
                        p2.string());
                    return false;
                    // If success to create DATA_PATH/STATION_YYYYMMDD/captures/
                } else {
                    logger->info("Success to create captures directory : {}",
                        p2.string());
                    return true;
                }
            }
        }
    }

    return true;
}

void AcqThread::runImageCapture(int imgNumber,
    int imgExposure,
    int imgGain,
    CamPixFmt imgFormat,
    ImgFormat imgOutput,
    string imgPrefix)
{
    auto logger = spdlog::get("acq_logger");
    // Stop camera
    mDevice->stopCamera();

    // Stop stack process.
    if (pStack != NULL) {
        std::unique_lock<std::mutex> lock(*stackSignal_mutex);
        *stackSignal = false;
        lock.unlock();

        // Force interruption.
        logger->info("Send reset signal to stack. ");
        pStack->interruptThread();
    }

    // Stop detection process.
    if (pDetection != nullptr) {
        std::unique_lock<std::mutex> lock(*detSignal_mutex);
        *detSignal = false;
        lock.unlock();
        logger->info("Send reset signal to detection process. ");
        pDetection->interruptThread();
    }

    // Reset framebuffer.
    logger->info("Cleaning frameBuffer...");
    std::unique_lock<std::mutex> lock(*frameBuffer_mutex);
    frameBuffer->clear();
    lock.unlock();

    for (int i = 0; i < imgNumber; i++) {
        logger->info("Prepare capture n° {}", i);

        // Configuration for single capture.
        Frame frame;
        logger->info("Exposure time : {}", imgExposure);
        frame.mExposure = imgExposure;
        logger->info("Gain : {}", imgGain);
        frame.mGain = imgGain;
        EParser<CamPixFmt> format;
        logger->info("Format : {}", format.getStringEnum(imgFormat));
        frame.mFormat = imgFormat;

        if (mcp.ACQ_RES_CUSTOM_SIZE) {
            frame.mStartX = mcp.ACQ_STARTX;
            frame.mStartY = mcp.ACQ_STARTY;
            frame.mHeight = mcp.ACQ_HEIGHT;
            frame.mWidth = mcp.ACQ_WIDTH;
        }

        // Run single capture.
        // MC: this looks like the calibration image
        logger->info("Run single capture.");
        if (mDevice->runSingleCapture(frame)) {
            logger->info("Single capture succeed !");
            spdlog::info("Single capture succeed !");
            saveImageCaptured(frame, i, imgOutput, imgPrefix);
        } else {
            logger->error("Single capture failed !");
        }
    }

#ifdef WINDOWS
    Sleep(1000);
#else
#ifdef LINUX
    // MC: 1s + imgExposure may be long wait. consider other trigger mode
    //     so that we do net have to wait that long after each calib image.
    sleep(1 + (imgExposure / 1000000));
#endif
#endif

    logger->info("Restarting camera in continuous mode...");

    // RECREATE CAMERA
    if (!mDevice->recreateCamera())
        throw "Fail to restart camera.";
    prepareAcquisitionOnDevice();
}

void AcqThread::saveImageCaptured(Frame& img,
    int imgNum,
    ImgFormat outputType,
    string imgPrefix)
{
    auto logger = spdlog::get("acq_logger");
    if (img.mImg.data) {
        // string  YYYYMMDD = TimeDate::getYYYYMMDD(img.mDate);

        if (buildAcquisitionDirectory(img.mDate)) {
            // string fileName = imgPrefix + "_" +
            // TimeDate::getYYYYMMDDThhmmss(img.mDate) + "_UT-" +
            // Conversion::intToString(imgNum);

            string dateFileName = TimeDate::getYYYY_MM_DD_hhmmss(img.mDate);
            string fileName = mstp.TELESCOP + "_" + dateFileName + "_" + mstp.INSTRUME + "_calibration"; // + imObsType

            switch (outputType) {
            case JPEG: {
                switch (img.mFormat) {
                case MONO12: {
                    Mat temp;
                    img.mImg.copyTo(temp);
                    Mat newMat = ImgProcessing::correctGammaOnMono12(temp, 2.2);
                    Mat newMat2 = Conversion::convertTo8UC1(newMat);
                    SaveImg::saveJPEG(newMat2,
                        mOutputDataPath + fileName);
                } break;
                default: {
                    Mat temp;
                    img.mImg.copyTo(temp);
                    Mat newMat = ImgProcessing::correctGammaOnMono8(temp, 2.2);
                    SaveImg::saveJPEG(newMat,
                        mOutputDataPath + fileName);
                }
                }
            }

            break;

            case FITS: {
                Fits2D newFits(mOutputDataPath, logger);
                newFits.loadKeys(mfkp, mstp);
                newFits.kGAINDB = img.mGain;
                newFits.kEXPOSURE = img.mExposure / 1000000.0;
                newFits.kONTIME = img.mExposure / 1000000.0;
                newFits.kELAPTIME = img.mExposure / 1000000.0;
                newFits.kDATEOBS = TimeDate::getIsoExtendedFormatDate(img.mDate);

                double debObsInSeconds = img.mDate.hours * 3600 + img.mDate.minutes * 60 + img.mDate.seconds;
                double julianDate = TimeDate::gregorianToJulian(img.mDate);
                double julianCentury = TimeDate::julianCentury(julianDate);

                newFits.kCRVAL1 = TimeDate::localSideralTime_2(
                    julianCentury, img.mDate.hours, img.mDate.minutes,
                    (int)img.mDate.seconds, mstp.SITELONG);
                newFits.kCTYPE1 = "RA---ARC";
                newFits.kCTYPE2 = "DEC--ARC";
                newFits.kEQUINOX = 2000.0;

                switch (img.mFormat) {
                case MONO12: {
                    // Convert unsigned short type image in short type
                    // image.
                    Mat newMat = Mat(img.mImg.rows, img.mImg.cols,
                        CV_16SC1, Scalar(0));

                    // Set bzero and bscale for print unsigned short
                    // value in soft visualization.
                    newFits.kBZERO = 32768;
                    newFits.kBSCALE = 1;

                    unsigned short* ptr = nullptr;
                    short* ptr2 = nullptr;

                    for (int i = 0; i < img.mImg.rows; i++) {
                        ptr = img.mImg.ptr<unsigned short>(i);
                        ptr2 = newMat.ptr<short>(i);

                        for (int j = 0; j < img.mImg.cols; j++) {
                            if (ptr[j] - 32768 > 32767) {
                                ptr2[j] = 32767;
                            } else {
                                ptr2[j] = ptr[j] - 32768;
                            }
                        }
                    }

                    // Create FITS image with BITPIX = SHORT_IMG
                    // (16-bits signed integers), pixel with TSHORT
                    // (signed short)
                    if (newFits.writeFits(newMat, S16, fileName, "",
                            FITS_SUFFIX))
                        spdlog::info(">> Fits saved in : {}{}",
                            mOutputDataPath, fileName);
                }

                break;

                default: {
                    if (newFits.writeFits(img.mImg, UC8, fileName, "",
                            FITS_SUFFIX))
                        spdlog::info(">> Fits saved in : {}{}",
                            mOutputDataPath, fileName);
                }
                }

            }

            break;
            }
        }
    }
}

bool AcqThread::computeSunTimes()
{
    int sunriseStartH = 0, sunriseStartM = 0, sunriseStopH = 0,
        sunriseStopM = 0, sunsetStartH = 0, sunsetStartM = 0, sunsetStopH = 0,
        sunsetStopM = 0;

    string date = TimeDate::IsoExtendedStringNow();
    vector<int> intDate = TimeDate::getIntVectorFromDateString(date);

    string month = Conversion::intToString(intDate.at(1));
    if (month.size() == 1)
        month = "0" + month;
    string day = Conversion::intToString(intDate.at(2));
    if (day.size() == 1)
        day = "0" + day;
    mCurrentDate = Conversion::intToString(intDate.at(0)) + month + day;
    mCurrentTime = intDate.at(3) * 3600 + intDate.at(4) * 60 + intDate.at(5);

    if (printFrameStats) {
        spdlog::info("\033[7;0H LOCAL DATE : {}", mCurrentDate);
    }

    if (mcp.ephem.EPHEMERIS_ENABLED) {
        Ephemeris ephem1 = Ephemeris(mCurrentDate, mcp.ephem.SUN_HORIZON_1,
            mstp.SITELONG, mstp.SITELAT);

        if (!ephem1.computeEphemeris(sunriseStartH, sunriseStartM, sunsetStopH,
                sunsetStopM)) {
            return false;
        }

        Ephemeris ephem2 = Ephemeris(mCurrentDate, mcp.ephem.SUN_HORIZON_2,
            mstp.SITELONG, mstp.SITELAT);

        if (!ephem2.computeEphemeris(sunriseStopH, sunriseStopM, sunsetStartH,
                sunsetStartM)) {
            return false;
        }

    } else {
        sunriseStartH = mcp.ephem.SUNRISE_TIME.at(0);
        sunriseStartM = mcp.ephem.SUNRISE_TIME.at(1);

        double intpart1 = 0;
        double fractpart1 = modf((double)mcp.ephem.SUNRISE_DURATION / 3600.0, &intpart1);

        if (intpart1 != 0) {
            if (sunriseStartH + intpart1 < 24) {
                sunriseStopH = sunriseStartH + intpart1;

            } else {
                sunriseStopH = sunriseStartH + intpart1 - 24;
            }

        } else {
            sunriseStopH = sunriseStartH;
        }

        double intpart2 = 0;
        double fractpart2 = modf(fractpart1 * 60, &intpart2);

        if (sunriseStartM + intpart2 < 60) {
            sunriseStopM = sunriseStartM + intpart2;

        } else {
            if (sunriseStopH + 1 < 24) {
                sunriseStopH += 1;

            } else {
                sunriseStopH = sunriseStopH + 1 - 24;
            }

            sunriseStopM = intpart2;
        }

        sunsetStartH = mcp.ephem.SUNSET_TIME.at(0);
        sunsetStartM = mcp.ephem.SUNSET_TIME.at(1);

        double intpart3 = 0;
        double fractpart3 = modf((double)mcp.ephem.SUNSET_DURATION / 3600.0, &intpart3);

        if (intpart3 != 0) {
            if (sunsetStartH + intpart3 < 24) {
                sunsetStopH = sunsetStartH + intpart3;

            } else {
                sunsetStopH = sunsetStartH + intpart3 - 24;
            }

        } else {
            sunsetStopH = sunsetStartH;
        }

        double intpart4 = 0;
        double fractpart4 = modf(fractpart3 * 60, &intpart4);

        if (sunsetStartM + intpart4 < 60) {
            sunsetStopM = sunsetStartM + intpart4;

        } else {
            if (sunsetStopH + 1 < 24) {
                sunsetStopH += 1;

            } else {
                sunsetStopH = sunsetStopH + 1 - 24;
            }

            sunsetStopM = intpart4;
        }
    }

    if (printFrameStats) {
        spdlog::info("\033[8;0H SUNRISE : {}H{} - {}H{}", sunriseStartH,
            sunriseStartM, sunriseStopH, sunriseStopM);
        spdlog::info("\033[9;0H SUNSET  : {}H{} - {}H{}", sunsetStartH,
            sunsetStartM, sunsetStopH, sunsetStopM);
    }

    mStartSunriseTime = sunriseStartH * 3600 + sunriseStartM * 60;
    mStopSunriseTime = sunriseStopH * 3600 + sunriseStopM * 60;
    mStartSunsetTime = sunsetStartH * 3600 + sunsetStartM * 60;
    mStopSunsetTime = sunsetStopH * 3600 + sunsetStopM * 60;

    return true;
}

bool AcqThread::prepareAcquisitionOnDevice()
{
    auto logger = spdlog::get("acq_logger");
    // SET SIZE
    if (!mDevice->setCameraSize())
        return false;

    // SET FORMAT
    if (!mDevice->setCameraPixelFormat())
        return false;

    // LOAD GET BOUNDS
    mDevice->getCameraExposureBounds();
    mDevice->getCameraGainBounds();

    // Get Sunrise start/stop, Sunset start/stop. ---
    computeSunTimes();

    // CHECK SUNRISE AND SUNSET TIMES.

    if ((mCurrentTime > mStopSunsetTime) || (mCurrentTime < mStartSunriseTime)) {
        logger->info("DAYTIME         :  NO");
        logger->info("AUTO EXPOSURE   :  NO");
        logger->info("EXPOSURE TIME   :  {}", mDevice->getNightExposureTime());
        logger->info("GAIN            :  {}", mDevice->getNightGain());
        if (!mDevice->setCameraNightExposureTime())
            return false;
        if (!mDevice->setCameraNightGain())
            return false;
    } else if ((mCurrentTime > mStopSunriseTime && mCurrentTime < mStartSunsetTime)) {
        logger->info("DAYTIME         :  YES");
        logger->info("AUTO EXPOSURE   :  NO");
        logger->info("EXPOSURE TIME   :  {}", mDevice->getDayExposureTime());
        logger->info("GAIN            :  {}", mDevice->getDayGain());
        if (!mDevice->setCameraDayExposureTime())
            return false;
        if (!mDevice->setCameraDayGain())
            return false;
    } else {
        logger->info("DAYTIME         :  NO");
        logger->info("AUTO EXPOSURE   :  YES");
        logger->info("EXPOSURE TIME   :  Minimum ({}){}",
            mDevice->mMinExposureTime,
            mDevice->getNightExposureTime());
        logger->info("GAIN            :  Minimum ({})", mDevice->mMinGain);

        if (!mDevice->setCameraExposureTime(mDevice->mMinExposureTime))
            return false;

        if (!mDevice->setCameraGain(mDevice->mMinGain))
            return false;
    }

    // INIT CAMERA.
    if (!mDevice->initializeCamera())
        return false;

    // SET FPS.
    if (!mDevice->setCameraFPS())
        return false;

    // START CAMERA.
    if (!mDevice->startCamera())
        return false;

    return true;
}
