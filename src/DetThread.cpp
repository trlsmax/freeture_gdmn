/*
								DetThread.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                       2014-2018 Chiara Marmo
*                            GEOPS-UPSUD
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
*   Last modified:      12/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
 * \file    DetThread.cpp
 * \author  Yoan Audureau -- GEOPS-UPSUD
 * \version 1.0
 * \date    12/03/2018
 * \brief   Detection thread.
 */

#include "DetThread.h"
#include <list>
#include <spdlog/spdlog.h>
#include <opencv2/opencv.hpp>
using namespace cv;

DetThread::DetThread(CDoubleLinkedList<std::shared_ptr<Frame>>* fb, mutex* fb_m, condition_variable* fb_c, 
	bool* dSignal, mutex* dSignal_m, condition_variable* dSignal_c, 
	detectionParam dtp, dataParam dp, mailParam mp, stationParam sp,
	fitskeysParam fkp, CamPixFmt pfmt, ResultSaver* rs)
	: pDetMthd(nullptr)
	, mForceToReset(false)
	, mMustStop(false)
	, mIsRunning(false)
	, mNbDetection(0)
	, mWaitFramesToCompleteEvent(false)
	, mCurrentDataSetLocation("")
	, mNbWaitFrames(0)
	, mInterruptionStatus(false)
    , m_ResultSaver(rs)
{
	frameBuffer = fb;
	frameBuffer_mutex = fb_m;
	frameBuffer_condition = fb_c;
	detSignal = dSignal;
	detSignal_mutex = dSignal_m;
	detSignal_condition = dSignal_c;
	pThread = nullptr;
	mFormat = pfmt;
	mStationName = sp.STATION_NAME;
	mdp = dp;
	mdtp = dtp;
	mmp = mp;
	mfkp = fkp;
	mstp = sp;
	mNbFramesAround = 0;

	printFrameStats = false;

	switch (dtp.DET_METHOD) {
	case TEMPORAL_MTHD: {
		pDetMthd = new DetectionTemporal(dtp, pfmt);
	} break;
	case TEMPLATE_MTHD: {
		pDetMthd = new DetectionTemplate(dtp, pfmt);
	} break;
	}
}

DetThread::~DetThread()
{
	auto logger = spdlog::get("det_logger");
	if (pDetMthd != nullptr) {
		logger->info("Remove pDetMthd instance.");
		delete pDetMthd;
	}

	if (pThread != nullptr) {
		logger->info("Remove detThread instance.");
		delete pThread;
	}
}

bool DetThread::startThread()
{
	auto logger = spdlog::get("det_logger");
	logger->info("Creating detThread...");
	pThread = new thread(&DetThread::run, this);
	pThread->detach();
	return true;
}

void DetThread::stopThread()
{
	auto logger = spdlog::get("det_logger");
	logger->info("Stopping detThread...");

	// Signal the thread to stop (thread-safe)
	mMustStopMutex.lock();
	mMustStop = true;
	mMustStopMutex.unlock();

	interruptThread();
	detSignal_condition->notify_one();
	// Wait for the thread to finish.
	pThread->join();

	delete pThread;
	pThread = nullptr;
}

Detection* DetThread::getDetMethod() { return pDetMthd; }

void DetThread::interruptThread()
{
	mInterruptionStatusMutex.lock();
	mInterruptionStatus = true;
	mInterruptionStatusMutex.unlock();
}

void DetThread::run()
{
	auto logger = spdlog::get("det_logger");
	bool stopThread = false;
	mIsRunning = true;
	bool eventToComplete = false;
	auto currentFrame = frameBuffer->end();
	std::list<std::tuple<std::shared_ptr<GlobalEvent>, 
              time_point<system_clock, std::chrono::seconds>, 
              CDoubleLinkedList<std::shared_ptr<Frame>>::Iterator>> listGlobalEvent;
#define GLOBAL_EVENT    0
#define EVENT_TIME      1
#define EVENT_FRAME     2
	// Flag to indicate that an event must be complete with more frames.
	// Reference date to count time to complete an event.
	time_point<system_clock, std::chrono::seconds> refTimeSec = 
        time_point_cast<std::chrono::seconds>(system_clock::now());

	logger->info("==============================================");
	logger->info("=========== Start detection thread ===========");
	logger->info("==============================================");

	/// Thread loop.
	try {
		do {
			try {
				/// Wait new frame from AcqThread.
				std::unique_lock<std::mutex> lock(*detSignal_mutex);
				while (!(*detSignal) && !mInterruptionStatus)
					detSignal_condition->wait(lock);
				*detSignal = false;
				lock.unlock();

				// Check interruption signal from AcqThread.
				mForceToReset = false;
				mInterruptionStatusMutex.lock();
				if (mInterruptionStatus) {
					logger->info("Interruption status : {}", mInterruptionStatus);
					logger->info("-> reset forced on detection method.");
					mForceToReset = true;
				}
				mInterruptionStatusMutex.unlock();

				if (!mForceToReset) {
					// Fetch the last grabbed frame.
                    std::list<std::shared_ptr<Frame>> frames;
					std::unique_lock<std::mutex> lock2(*frameBuffer_mutex);
					if (!frameBuffer->empty()) {
						if (currentFrame == frameBuffer->end()) {
							currentFrame = frameBuffer->begin();
						}
						while (currentFrame != frameBuffer->end()) {
							if (currentFrame.node()->valid) {
								frames.push_back(currentFrame.Value());
								currentFrame.node()->valid = false;
							}
							if (currentFrame.node()->pNext == nullptr) {
								break;
							} else {
								++currentFrame;
							}
						}
					}
					lock2.unlock();

                    for (auto& lastFrame : frames) {
                        double t = (double)getTickCount();
                        if (lastFrame && lastFrame->mImg.data) {
                            mFormat = lastFrame->mFormat;
                            // Run detection process.
                            std::shared_ptr<GlobalEvent> ret = pDetMthd->runDetection(lastFrame);
                            if (ret) {
                                listGlobalEvent.emplace_back(ret, time_point_cast<std::chrono::seconds>(system_clock::now()), currentFrame);
                                //logger->info( "Event detected ! Waiting frames to complete the event...");
                                spdlog::info( "Event detected ! Waiting frames to complete the event...");
                                mNbDetection++;
                            }

                            if (!listGlobalEvent.empty()) {
                                time_point<system_clock, std::chrono::seconds> nowTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());
                                for (auto itr = listGlobalEvent.begin(); itr != listGlobalEvent.end();) {
                                    std::get<GLOBAL_EVENT>(*itr)->AddFrame(lastFrame, false);
                                    if ((nowTimeSec - std::get<EVENT_TIME>(*itr)).count() > mdtp.DET_TIME_AROUND) {
                                        //logger->info("Event completed.");
                                        spdlog::info("Event completed.");

                                        std::unique_lock<std::mutex> lock3(*frameBuffer_mutex);
                                        std::get<GLOBAL_EVENT>(*itr)->GetFramesBeforeEvent(std::get<EVENT_FRAME>(*itr));
                                        lock3.unlock();
                                        if (m_ResultSaver) {
                                            m_ResultSaver->AddResult(std::get<0>(*itr));
                                        }
                                        itr = listGlobalEvent.erase(itr);

                                        // Reset detection.
                                        logger->info("Reset detection process.");
                                        pDetMthd->resetDetection(false);
                                    }
                                    else {
                                        itr++;
                                    }
                                }
                            }
                        }

                        t = (((double)getTickCount() - t) / getTickFrequency()) * 1000;
                        // cout <<"\033[11;0H" << " [ TIME DET ] : " <<
                        // std::setprecision(3) << std::fixed << t << " ms " << endl;
                        if (printFrameStats) {
                            spdlog::debug(" [ TIME DET ] : {} ms", t);
                        }
                        logger->debug(" [ TIME DET ] : {} ms", t);
                    }
                    frames.clear();
				}
				else {
					// reset method
					if (pDetMthd != NULL)
						pDetMthd->resetDetection(false);

					eventToComplete = false;
					mNbWaitFrames = 0;

					mInterruptionStatusMutex.lock();
					mInterruptionStatus = false;
					mInterruptionStatusMutex.unlock();
				}
			}
			catch (std::exception& e) {
				listGlobalEvent.clear();
                logger->critical(e.what());
			}

			mMustStopMutex.lock();
			stopThread = mMustStop;
			mMustStopMutex.unlock();
		} while (!stopThread);

		if (mDetectionResults.empty()) {
			spdlog::info("-----------------------------------------------");
			spdlog::info("------------->> DETECTED EVENTS : {}", mNbDetection);
			spdlog::info("-----------------------------------------------");
		}
		else {
			// Create Report for videos and frames in input.
			std::ofstream report;
			// string reportPath = mdp.DATA_PATH + "detections_report.txt";
			string reportPath = DataPaths::getSessionPath(
				mdp.DATA_PATH,
				TimeDate::splitIsoExtendedDate(TimeDate::IsoExtendedStringNow()))
				+ "detections_report.txt";

			report.open(reportPath.c_str());
			spdlog::info("--------------- DETECTION REPORT --------------");
			for (int i = 0; i < mDetectionResults.size(); i++) {
				report << mDetectionResults.at(i).first << "------> "
					<< mDetectionResults.at(i).second << "\n";
				spdlog::info("- DATASET {}:{} {}", i,
					mDetectionResults.at(i).second,
					(mDetectionResults.at(i).second > 1) ? " events" : " event");
			}

			spdlog::info("-----------------------------------------------");
			report.close();
		}
	}
	catch (const char* msg) {
		listGlobalEvent.clear();
		spdlog::critical(msg);
		logger->critical(msg);
	}
	catch (exception& e) {
		listGlobalEvent.clear();
		spdlog::critical("An error occured. See log for details.");
		spdlog::critical(e.what());
		logger->critical(e.what());
	}

	listGlobalEvent.clear();
	mIsRunning = false;
	logger->info("DetThread ended.");
}

bool DetThread::getRunStatus() { return mIsRunning; }

void DetThread::setFrameStats(bool frameStats)
{
	printFrameStats = frameStats;
	pDetMthd->setMaskFrameStats(frameStats);
}
