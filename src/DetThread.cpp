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
	fitskeysParam fkp, CamPixFmt pfmt)
	:

	pDetMthd(nullptr)
	, mForceToReset(false)
	, mMustStop(false)
	, mIsRunning(false)
	, mNbDetection(0)
	, mWaitFramesToCompleteEvent(false)
	, mCurrentDataSetLocation("")
	, mNbWaitFrames(0)
	, mInterruptionStatus(false)
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
	std::list<std::tuple<std::shared_ptr<GlobalEvent>, time_point<system_clock, std::chrono::seconds>, CDoubleLinkedList<std::shared_ptr<Frame>>::Iterator>> listGlobalEvent;
	// Flag to indicate that an event must be complete with more frames.
	// Reference date to count time to complete an event.
	time_point<system_clock, std::chrono::seconds> refTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());

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
                                listGlobalEvent.emplace_back(ret, time_point_cast<std::chrono::seconds>(system_clock::now()), lastFrame);
                                //logger->info( "Event detected ! Waiting frames to complete the event...");
                                spdlog::info( "Event detected ! Waiting frames to complete the event...");
                                mNbDetection++;
                            }

                            if (!listGlobalEvent.empty()) {
                                time_point<system_clock, std::chrono::seconds> nowTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());
                                for (auto itr = listGlobalEvent.begin(); itr != listGlobalEvent.end();) {
                                    std::get<0>(*itr)->AddFrame(lastFrame, false);
                                    if ((nowTimeSec - std::get<1>(*itr)).count() > mdtp.DET_TIME_AROUND) {
                                        //logger->info("Event completed.");
                                        spdlog::info("Event completed.");
                                        // Build event directory.
                                        mEventDate = std::get<0>(*itr)->getDate();

                                        if (buildEventDataDirectory())
                                            logger->info("Success to build event directory.");
                                        else
                                            logger->error("Fail to build event directory.");

                                        // Save event.
                                        logger->info("Saving event...");
                                        string eventBase = mstp.TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(mEventDate);
                                        std::unique_lock<std::mutex> lock3(*frameBuffer_mutex);
                                        std::get<0>(*itr)->GetFramesBeforeEvent(std::get<2>(*itr));
                                        lock3.unlock();
                                        pDetMthd->saveDetectionInfos(std::get<0>(*itr).get(), mEventPath + eventBase);
                                        if (!saveEventData(std::get<0>(*itr).get()))
                                            spdlog::info("Error saving event data.");
                                        else
                                            spdlog::info("Success to save event !");
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

bool DetThread::buildEventDataDirectory()
{
	namespace fs = ghc::filesystem;
	auto logger = spdlog::get("det_logger");

	// eventDate is the date of the first frame attached to the event.
	// string YYYYMMDD = TimeDate::getYYYYMMDD(mEventDate);

	// Data location.
	fs::path p(mdp.DATA_PATH);

	// Create data directory for the current day.
	// string fp = mdp.DATA_PATH + mStationName + "_" + YYYYMMDD +"/";
	string fp = DataPaths::getSessionPath(mdp.DATA_PATH, mEventDate);
	fs::path p0(fp);

	// Events directory.
	// string fp1 = "events/";
	string fp1 = "";
	fs::path p1(fp + fp1);

	// Current event directory with the format : STATION_AAAAMMDDThhmmss_UT
	// string fp2 = mStationName + "_" +
	// TimeDate::getYYYYMMDDThhmmss(mEventDate) + "_UT/"; string fp2 =
	// mStationName + "_" + "detection" + "_" +
	// TimeDate::getYYYY_MM_DD_hhmmss(mEventDate);
	string fp2 = mstp.TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(mEventDate) + "/";
	fs::path p2(fp + fp1 + fp2);
	fs::create_directories(p2);

	// Final path used by an other function to save event data.
	mEventPath = fp + fp1 + fp2;

	// Check if data path specified in the configuration file exists.
	if (fs::exists(p)) {
		// Check DataLocation/STATION_AAMMDD/
		if (fs::exists(p0)) {
			// Check DataLocation/STATION_AAMMDD/events/
			if (fs::exists(p1)) {
				// Check
				// DataLocation/STATION_AAMMDD/events/STATION_AAAAMMDDThhmmss_UT/
				if (!fs::exists(p2)) {
					// Create
					// DataLocation/STATION_AAMMDD/events/STATION_AAAAMMDDThhmmss_UT/
					if (!fs::create_directories(p2)) {
						logger->error("Fail to create : {}", p2.c_str());
						return false;
					}
					else {
						logger->info("Success to create : {}", p2.c_str());
						return true;
					}
				}
			}
			else {
				// Create DataLocation/STATION_AAMMDD/events/
				if (!fs::create_directories(p1)) {
					logger->error("Fail to create : {}", p1.c_str());
					return false;
				}
				else {
					// Create
					// DataLocation/STATION_AAMMDD/events/STATION_AAAAMMDDThhmmss_UT/
					if (!fs::create_directories(p2)) {
						logger->error("Fail to create : {}", p2.c_str());
						return false;
					}
					else {
						logger->info("Success to create : {}", p2.c_str());
						return true;
					}
				}
			}
		}
		else {
			// Create DataLocation/STATION_AAMMDD/
			if (!fs::create_directories(p0)) {
				logger->error("Fail to create : {}", p0.c_str());
				return false;
			}
			else {
				// Create DataLocation/STATION_AAMMDD/events/
				if (!fs::create_directories(p1)) {
					logger->error("Fail to create : {}", p1.c_str());
					return false;
				}
				else {
					// Create
					// DataLocation/STATION_AAMMDD/events/STATION_AAAAMMDDThhmmss_UT/
					if (!fs::create_directories(p2)) {
						logger->error("Fail to create : {}", p2.c_str());
						return false;
					}
					else {
						logger->info("Success to create : {}", p2.c_str());
						return true;
					}
				}
			}
		}
	}
	else {
		// Create DataLocation/
		if (!fs::create_directories(p)) {
			logger->error("Fail to create : {}", p.c_str());
			return false;
		}
		else {
			// Create DataLocation/STATION_AAMMDD/
			if (!fs::create_directories(p0)) {
				logger->error("Fail to create : {}", p0.c_str());
				return false;
			}
			else {
				// Create DataLocation/STATION_AAMMDD/events/
				if (!fs::create_directories(p1)) {
					logger->error("Fail to create : {}", p1.c_str());
					return false;
				}
				else {
					// Create
					// DataLocation/STATION_AAMMDD/events/STATION_AAAAMMDDThhmmss_UT/
					if (!fs::create_directories(p2)) {
						logger->error("Fail to create : {}", p2.c_str());
						return false;
					}
					else {
						logger->info("Success to create : {}", p1.c_str());
						return true;
					}
				}
			}
		}
	}

	return true;
}

bool DetThread::saveEventData(GlobalEvent* ge)
{
	auto logger = spdlog::get("det_logger");

	string eventBase = mstp.TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(mEventDate);

	// Count number of digit on nbTotalFramesToSave.
	int n = ge->Frames().size();
	int nbDigitOnNbTotalFramesToSave = 0;

	while (n != 0) {
		n /= 10;
		++nbDigitOnNbTotalFramesToSave;
	}

    spdlog::info("> First frame to save  : {}", ge->Frames().front()->mFrameNumber);
	spdlog::info("> Lst frame to save    : {}", ge->Frames().back()->mFrameNumber);
	spdlog::info("> First event frame    : {}", ge->FirstEventFrameNbr());
	spdlog::info("> Last event frame     : {}", ge->LastEventFrameNbr());
	spdlog::info("> Frames before        : {}", ge->FramesAround());
	spdlog::info("> Frames after         : {}", ge->FramesAround());
	spdlog::info("> Total frames to save : {}", ge->Frames().size());
	spdlog::info("> Total digit          : {}", nbDigitOnNbTotalFramesToSave);

	// Init video avi
	VideoWriter* video = NULL;

	if (mdtp.DET_SAVE_AVI) {
		// third parameter controls FPS. Might need to change that one (used to
		// be 5 fps).
		video = new VideoWriter(
			mEventPath + eventBase + "_video.avi",
			VideoWriter::fourcc('M', 'J', 'P', 'G'), 30,
			Size(static_cast<int>(ge->Frames().front()->mImg.cols),
				static_cast<int>(ge->Frames().front()->mImg.rows)),
			false);
	}

	// Init sum.
	Stack stack = Stack(mdp.FITS_COMPRESSION_METHOD, mfkp, mstp);

	// Loop framebuffer.
	for (auto frame : ge->Frames()) {
		if (mdtp.DET_SAVE_AVI) {
			if (video->isOpened()) {
				if (frame->mImg.type() != CV_8UC1) {
					Mat iv = Conversion::convertTo8UC1(frame->mImg);
					video->write(iv);
				}
				else {
					video->write(frame->mImg);
				}
			}
		}

		// Add frame to the event's stack.
		stack.addFrame(*frame);
	}

	if (mdtp.DET_SAVE_AVI) {
		if (video != NULL)
			delete video;
	}

	// EVENT STACK WITH HISTOGRAM EQUALIZATION
	if (mdtp.DET_SAVE_SUM_WITH_HIST_EQUALIZATION) {
		SaveImg::saveJPEG(stack.getStack(), mEventPath + eventBase);
	}

	return true;
}

void DetThread::setFrameStats(bool frameStats)
{
	printFrameStats = frameStats;
	pDetMthd->setMaskFrameStats(frameStats);
}
