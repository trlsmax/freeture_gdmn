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
#include <spdlog/spdlog.h>
using namespace cv;

DetThread::DetThread(circular_buffer<Frame>* fb, mutex* fb_m,
	condition_variable* fb_c, bool* dSignal,
	mutex* dSignal_m,
	condition_variable* dSignal_c, detectionParam dtp,
	dataParam dp, mailParam mp, stationParam sp,
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

	mFitsHeader.loadKeys(fkp, sp);
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
	// Flag to indicate that an event must be complete with more frames.
	bool eventToComplete = false;
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
					Frame lastFrame;
					std::unique_lock<std::mutex> lock2(*frameBuffer_mutex);
					if (frameBuffer->size() > 2)
						lastFrame = frameBuffer->back();
					lock2.unlock();

					double t = (double)getTickCount();
					if (lastFrame.mImg.data) {
						mFormat = lastFrame.mFormat;
						// Run detection process.
						if (pDetMthd->runDetection(lastFrame) && !eventToComplete) {
							// Event detected.
							logger->info(
								"Event detected ! Waiting frames to complete "
								"the event...");
							eventToComplete = true;

							// Get a reference date.
							refTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());

							mNbDetection++;
						}

						// Wait frames to complete the detection.
						if (eventToComplete) {
							time_point<system_clock, std::chrono::seconds> nowTimeSec = time_point_cast<std::chrono::seconds>(system_clock::now());

							auto d = nowTimeSec - refTimeSec;
							long secTime = d.count();
							if (secTime > mdtp.DET_TIME_AROUND) {
								logger->info("Event completed.");
								// Build event directory.
								mEventDate = pDetMthd->getEventDate();
								logger->info("Building event directory...");

								if (buildEventDataDirectory())
									logger->info("Success to build event directory.");
								else
									logger->error("Fail to build event directory.");

								// Save event.
								logger->info("Saving event...");
								string eventBase = mstp.TELESCOP + "_detection_" + TimeDate::getYYYY_MM_DD_hhmmss(mEventDate);
								pDetMthd->saveDetectionInfos(mEventPath + eventBase, mNbFramesAround);
								std::unique_lock<std::mutex> lock(*frameBuffer_mutex);
								if (!saveEventData(
									pDetMthd->getEventFirstFrameNb(),
									pDetMthd->getEventLastFrameNb()))
									logger->critical("Error saving event data.");
								else
									logger->info("Success to save event !");

								lock.unlock();

								// Reset detection.
								logger->info("Reset detection process.");
								pDetMthd->resetDetection(false);
								eventToComplete = false;
								mNbFramesAround = 0;
							}

							mNbFramesAround++;
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
		spdlog::critical(msg);
		logger->critical(msg);
	}
	catch (exception& e) {
		spdlog::critical("An error occured. See log for details.");
		spdlog::critical(e.what());
		logger->critical(e.what());
	}

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

bool DetThread::saveEventData(int firstEvPosInFB, int lastEvPosInFB)
{
	namespace fs = ghc::filesystem;
	auto logger = spdlog::get("det_logger");

	// List of data path to attach to the mail notification.
	vector<string> mailAttachments;

	string eventBase = mstp.TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(mEventDate);

	// Number of the first frame to save. It depends of how many frames we want
	// to keep before the event.
	int numFirstFrameToSave = firstEvPosInFB - mNbFramesAround;

	// Number of the last frame to save. It depends of how many frames we want
	// to keep after the event.
	int numLastFrameToSave = lastEvPosInFB + mNbFramesAround;

	// If the number of the first frame to save for the event is not in the
	// framebuffer. The first frame to save become the first frame available in
	// the framebuffer.
	if (frameBuffer->front().mFrameNumber > numFirstFrameToSave)
		numFirstFrameToSave = frameBuffer->front().mFrameNumber;

	// Check the number of the last frame to save.
	if (frameBuffer->back().mFrameNumber < numLastFrameToSave)
		numLastFrameToSave = frameBuffer->back().mFrameNumber;

	// Total frames to save.
	int nbTotalFramesToSave = numLastFrameToSave - numFirstFrameToSave;

	// Count number of digit on nbTotalFramesToSave.
	int n = nbTotalFramesToSave;
	int nbDigitOnNbTotalFramesToSave = 0;

	while (n != 0) {
		n /= 10;
		++nbDigitOnNbTotalFramesToSave;
	}

	logger->info("> First frame to save  : {}", numFirstFrameToSave);
	logger->info("> Lst frame to save    : {}", numLastFrameToSave);
	logger->info("> First event frame    : {}", firstEvPosInFB);
	logger->info("> Last event frame     : {}", lastEvPosInFB);
	logger->info("> Frames before        : {}", mNbFramesAround);
	logger->info("> Frames after         : {}", mNbFramesAround);
	logger->info("> Total frames to save : {}", nbTotalFramesToSave);
	logger->info("> Total digit          : {}", nbDigitOnNbTotalFramesToSave);

	TimeDate::Date dateFirstFrame;

	int c = 0;

	// Init video avi
	VideoWriter* video = NULL;

	if (mdtp.DET_SAVE_AVI) {
		// third parameter controls FPS. Might need to change that one (used to
		// be 5 fps).
		video = new VideoWriter(
			mEventPath + eventBase + "_video.avi",
			VideoWriter::fourcc('M', 'J', 'P', 'G'), 30,
			Size(static_cast<int>(frameBuffer->front().mImg.cols),
				static_cast<int>(frameBuffer->front().mImg.rows)),
			false);
	}

	// Init fits 3D.
	Fits3D fits3d(logger);

	if (mdtp.DET_SAVE_FITS3D) {
		fits3d = Fits3D(mFormat, frameBuffer->front().mImg.rows,
			frameBuffer->front().mImg.cols,
			(numLastFrameToSave - numFirstFrameToSave + 1),
			mEventPath + "fits3D");
		fits3d.kDATE = TimeDate::IsoExtendedStringNow();

		// Name of the fits file.
		fits3d.kFILENAME = mEventPath + eventBase + "_fitscube.fit";
	}

	// Init sum.
	Stack stack = Stack(mdp.FITS_COMPRESSION_METHOD, mfkp, mstp);

	// Exposure time sum.
	double sumExpTime = 0.0;
	double firstExpTime = 0.0;
	bool varExpTime = false;

	// Loop framebuffer.
	circular_buffer<Frame>::iterator it;
	for (it = frameBuffer->begin(); it != frameBuffer->end(); ++it) {
		// Get infos about the first frame of the event for fits 3D.
		if ((*it).mFrameNumber == numFirstFrameToSave && mdtp.DET_SAVE_FITS3D) {
			fits3d.kDATEOBS = TimeDate::getIsoExtendedFormatDate((*it).mDate);
			// Gain.
			fits3d.kGAINDB = (*it).mGain;
			// Saturation.
			fits3d.kSATURATE = (*it).mSaturatedValue;
			// FPS.
			fits3d.kCD3_3 = (*it).mFps;
			// CRVAL1 : sideral time.
			double julianDate = TimeDate::gregorianToJulian((*it).mDate);
			double julianCentury = TimeDate::julianCentury(julianDate);
			double sideralT = TimeDate::localSideralTime_2(
				julianCentury, (*it).mDate.hours, (*it).mDate.minutes,
				(int)(*it).mDate.seconds, mFitsHeader.kSITELONG);
			fits3d.kCRVAL1 = sideralT;
			// Projection and reference system
			fits3d.kCTYPE1 = "RA---ARC";
			fits3d.kCTYPE2 = "DEC--ARC";
			// Equinox
			fits3d.kEQUINOX = 2000.0;
			firstExpTime = (*it).mExposure;
			dateFirstFrame = (*it).mDate;
		}

		// Get infos about the last frame of the event record for fits 3D.
		if ((*it).mFrameNumber == numLastFrameToSave && mdtp.DET_SAVE_FITS3D) {
			spdlog::info("DATE first : {} H {} M {} S", dateFirstFrame.hours,
				dateFirstFrame.minutes, dateFirstFrame.seconds);
			spdlog::info("DATE last  : {} H {} M {} S", (*it).mDate.hours,
				(*it).mDate.minutes, (*it).mDate.seconds);
			fits3d.kELAPTIME = ((*it).mDate.hours * 3600 + (*it).mDate.minutes * 60 + (*it).mDate.seconds) - (dateFirstFrame.hours * 3600 + dateFirstFrame.minutes * 60 + dateFirstFrame.seconds);
		}

		// If the current frame read from the framebuffer has to be saved.
		if ((*it).mFrameNumber >= numFirstFrameToSave && (*it).mFrameNumber < numLastFrameToSave) {
			// Save fits2D.
			if (mdtp.DET_SAVE_FITS2D) {
				string fits2DPath = mEventPath + eventBase + "_rawframes/";
				string fits2DName = "frame_" + Conversion::numbering(nbDigitOnNbTotalFramesToSave, c) + Conversion::intToString(c);
				logger->info(">> Saving fits2D : {}", fits2DName);

				Fits2D newFits(fits2DPath, logger);
				newFits.loadKeys(mfkp, mstp);
				// Frame's acquisition date.
				newFits.kDATEOBS = TimeDate::getIsoExtendedFormatDate((*it).mDate);
				// Fits file creation date.
				// YYYYMMDDTHHMMSS,fffffffff where T is the date-time separator
				newFits.kDATE = TimeDate::IsoExtendedStringNow();
				// Name of the fits file.
				newFits.kFILENAME = fits2DName;
				// Exposure time.
				newFits.kONTIME = (*it).mExposure / 1000000.0;
				// Gain.
				newFits.kGAINDB = (*it).mGain;
				// Saturation.
				newFits.kSATURATE = (*it).mSaturatedValue;
				// FPS.
				newFits.kCD3_3 = (*it).mFps;
				// CRVAL1 : sideral time.
				double julianDate = TimeDate::gregorianToJulian((*it).mDate);
				double julianCentury = TimeDate::julianCentury(julianDate);
				double sideralT = TimeDate::localSideralTime_2(
					julianCentury, (*it).mDate.hours, (*it).mDate.minutes,
					(*it).mDate.seconds, mFitsHeader.kSITELONG);
				newFits.kCRVAL1 = sideralT;
				newFits.kEXPOSURE = (*it).mExposure / 1000000.0;
				// Projection and reference system
				newFits.kCTYPE1 = "RA---ARC";
				newFits.kCTYPE2 = "DEC--ARC";
				// Equinox
				newFits.kEQUINOX = 2000.0;

				if (!fs::exists(fs::path(fits2DPath))) {
					if (fs::create_directories(fs::path(fits2DPath)))
						logger->info("Success to create directory : {}", fits2DPath);
				}

				switch (mFormat) {
				case MONO12: {
					newFits.writeFits((*it).mImg, S16, fits2DName,
						mdp.FITS_COMPRESSION_METHOD, "");
				} break;
				default:

				{
					newFits.writeFits((*it).mImg, UC8, fits2DName,
						mdp.FITS_COMPRESSION_METHOD, "");
				}
				}
			}

			if (mdtp.DET_SAVE_AVI) {
				if (video->isOpened()) {
					if (it->mImg.type() != CV_8UC1) {
						Mat iv = Conversion::convertTo8UC1((*it).mImg);
						video->write(iv);
					}
					else {
						video->write(it->mImg);
					}
				}
			}

			// Add a frame to fits cube.
			if (mdtp.DET_SAVE_FITS3D) {
				if (firstExpTime != (*it).mExposure)
					varExpTime = true;

				sumExpTime += (*it).mExposure;
				fits3d.addImageToFits3D((*it).mImg);
			}

			// Add frame to the event's stack.
			if (mdtp.DET_SAVE_SUM && (*it).mFrameNumber >= firstEvPosInFB && (*it).mFrameNumber <= lastEvPosInFB) {
				stack.addFrame((*it));
			}

			c++;
		}
	}

	if (mdtp.DET_SAVE_AVI) {
		if (video != NULL)
			delete video;
	}

	// ********************************* SAVE EVENT IN FITS CUBE
	// ***********************************

	if (mdtp.DET_SAVE_FITS3D) {
		// Exposure time of a single frame.
		if (varExpTime)
			fits3d.kEXPOSURE = 999999;
		else {
			it = frameBuffer->begin();
			fits3d.kEXPOSURE = (*it).mExposure / 1000000.0;
		}

		// Exposure time sum of frames in the fits cube.
		fits3d.kONTIME = sumExpTime / 1000000.0;
		fits3d.writeFits3D();
	}

	// ********************************* SAVE EVENT STACK IN FITS
	// **********************************
	//if (mdtp.DET_SAVE_SUM) {
	//    stack.saveStack(mEventPath, mdtp.DET_SUM_MTHD, mdtp.DET_SUM_REDUCTION);
	//}

	// ************************** EVENT STACK WITH HISTOGRAM EQUALIZATION
	// ***************************

	if (mdtp.DET_SAVE_SUM_WITH_HIST_EQUALIZATION) {
#if 0
		Mat s, s1, eqHist;
		float bzero = 0.0;
		float bscale = 1.0;
		s = stack.reductionByFactorDivision(bzero, bscale);
		cout << "mFormat : " << mFormat << endl;
		if (mFormat != MONO8)
			Conversion::convertTo8UC1(s).copyTo(s);

		equalizeHist(s, eqHist);
		// SaveImg::saveJPEG(eqHist,mEventPath+mStationName+"_"+TimeDate::getYYYYMMDDThhmmss(mEventDate)+"_UT");
		SaveImg::saveJPEG(eqHist, mEventPath + eventBase);
#endif
		SaveImg::saveJPEG(stack.getStack(), mEventPath + eventBase);
	}

#if 0
	// *********************************** SEND MAIL NOTIFICATION ***********************************
	BOOST_LOG_SEV(logger, notification) << "Prepare mail..." << mmp.MAIL_DETECTION_ENABLED;
	if (mmp.MAIL_DETECTION_ENABLED) {

		BOOST_LOG_SEV(logger, notification) << "Sending mail...";

		for (int i = 0; i < pDetMthd->getDebugFiles().size(); i++) {

			if (boost::filesystem::exists(mEventPath + pDetMthd->getDebugFiles().at(i))) {

				BOOST_LOG_SEV(logger, notification) << "Send : " << mEventPath << pDetMthd->getDebugFiles().at(i);
				mailAttachments.push_back(mEventPath + pDetMthd->getDebugFiles().at(i));

			}
		}

		if (mdtp.DET_SAVE_SUM_WITH_HIST_EQUALIZATION && boost::filesystem::exists(mEventPath + mStationName + "_" + TimeDate::getYYYYMMDDThhmmss(mEventDate) + "_UT.jpg")) {

			BOOST_LOG_SEV(logger, notification) << "Send : " << mEventPath << mStationName << "_" << TimeDate::getYYYYMMDDThhmmss(mEventDate) << "_UT.jpg";
			mailAttachments.push_back(mEventPath + mStationName + "_" + TimeDate::getYYYYMMDDThhmmss(mEventDate) + "_UT.jpg");

		}

		SMTPClient::sendMail(mmp.MAIL_SMTP_SERVER,
			mmp.MAIL_SMTP_LOGIN,
			mmp.MAIL_SMTP_PASSWORD,
			"freeture@" + mStationName + ".fr",
			mmp.MAIL_RECIPIENTS,
			mStationName + "-" + TimeDate::getYYYYMMDDThhmmss(mEventDate),
			mStationName + "\n" + mEventPath,
			mailAttachments,
			mmp.MAIL_CONNECTION_TYPE);

}
#endif

	return true;
}

void DetThread::setFrameStats(bool frameStats)
{
	printFrameStats = frameStats;
	pDetMthd->setMaskFrameStats(frameStats);
}
