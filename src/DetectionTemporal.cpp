/*
					DetectionTemporal.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                       2014-2018 Chiara Marmo
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
*   Last modified:      12/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
 * \file    DetectionTemporal.cpp
 * \author  Yoan Audureau -- GEOPS-UPSUD
 * \version 1.0
 * \date    12/03/2018
 * \brief   Detection method by temporal movement.
 */

#include "DetectionTemporal.h"
#include <spdlog/spdlog.h>
using namespace cv;

DetectionTemporal::DetectionTemporal(detectionParam dtp, CamPixFmt fmt)
{
	mListColors.push_back(Scalar(0, 0, 139)); // DarkRed
	mListColors.push_back(Scalar(0, 0, 255)); // Red
	mListColors.push_back(Scalar(0, 100, 100)); // IndianRed
	mListColors.push_back(Scalar(92, 92, 205)); // Salmon
	mListColors.push_back(Scalar(0, 140, 255)); // DarkOrange
	mListColors.push_back(Scalar(30, 105, 210)); // Chocolate
	mListColors.push_back(Scalar(0, 255, 255)); // Yellow
	mListColors.push_back(Scalar(140, 230, 240)); // Khaki
	mListColors.push_back(Scalar(224, 255, 255)); // LightYellow
	mListColors.push_back(Scalar(211, 0, 148)); // DarkViolet
	mListColors.push_back(Scalar(147, 20, 255)); // DeepPink
	mListColors.push_back(Scalar(255, 0, 255)); // Magenta
	mListColors.push_back(Scalar(0, 100, 0)); // DarkGreen
	mListColors.push_back(Scalar(0, 128, 128)); // Olive
	mListColors.push_back(Scalar(0, 255, 0)); // Lime
	mListColors.push_back(Scalar(212, 255, 127)); // Aquamarine
	mListColors.push_back(Scalar(208, 224, 64)); // Turquoise
	mListColors.push_back(Scalar(205, 0, 0)); // Blue
	mListColors.push_back(Scalar(255, 191, 0)); // DeepSkyBlue
	mListColors.push_back(Scalar(255, 255, 0)); // Cyan

	mImgNum = 0;
	mDebugUpdateMask = false;
	mSubdivisionStatus = false;
	mDataSetCounter = 0;
	mRoiSize[0] = 10;
	mRoiSize[1] = 10;
	mdtp = dtp;

	if (dtp.ACQ_MASK_ENABLED) {
		mMask = imread(dtp.ACQ_MASK_PATH, IMREAD_GRAYSCALE);
	}

	// Create local mask to eliminate single white pixels.
	Mat maskTemp(3, 3, CV_8UC1, Scalar(255));
	maskTemp.at<uchar>(1, 1) = 0;
	maskTemp.copyTo(mLocalMask);

	mdtp.DET_DEBUG_PATH = mdtp.DET_DEBUG_PATH + "/";
	mDebugCurrentPath = mdtp.DET_DEBUG_PATH;

	// Create directories for debugging method.
	if (dtp.DET_DEBUG)
		createDebugDirectories(true);
}

DetectionTemporal::~DetectionTemporal()
{
}

void DetectionTemporal::resetDetection(bool loadNewDataSet)
{
	auto logger = spdlog::get("det_logger");
	// Clear list of files to send by mail.
	debugFiles.clear();
	mSubdivisionStatus = false;
	mPrevThresholdedMap.release();
	mPrevFrame.release();

	if (mdtp.DET_DEBUG && loadNewDataSet) {
		mDataSetCounter++;
		createDebugDirectories(false);
	}
}

void DetectionTemporal::createDebugDirectories(bool cleanDebugDirectory)
{
	mDebugCurrentPath = mdtp.DET_DEBUG_PATH + "debug_" + Conversion::intToString(mDataSetCounter) + "/";

	if (cleanDebugDirectory) {
		const ghc::filesystem::path p0 = ghc::filesystem::path(mdtp.DET_DEBUG_PATH);

		if (ghc::filesystem::exists(p0)) {
			ghc::filesystem::remove_all(p0);
		}
		else {
			ghc::filesystem::create_directories(p0);
		}
	}

	const ghc::filesystem::path p1 = ghc::filesystem::path(mDebugCurrentPath);

	if (!ghc::filesystem::exists(p1))
		ghc::filesystem::create_directories(p1);

	vector<string> debugSubDir;
	debugSubDir.push_back("original");
	debugSubDir.push_back("absolute_difference");
	debugSubDir.push_back("event_map_initial");
	debugSubDir.push_back("event_map_filtered");
	debugSubDir.push_back("absolute_difference_dilated");
	debugSubDir.push_back("neg_difference_thresholded");
	debugSubDir.push_back("pos_difference_thresholded");
	debugSubDir.push_back("neg_difference");
	debugSubDir.push_back("pos_difference");

	for (int i = 0; i < debugSubDir.size(); i++) {
		const ghc::filesystem::path path(mDebugCurrentPath + debugSubDir.at(i));

		if (!ghc::filesystem::exists(path)) {
			ghc::filesystem::create_directories(path);
		}
	}
}

vector<string> DetectionTemporal::getDebugFiles() { return debugFiles; }

std::shared_ptr<GlobalEvent> DetectionTemporal::runDetection(std::shared_ptr<Frame> c)
{
	auto logger = spdlog::get("det_logger");

	if (!mSubdivisionStatus) {
		mSubdivisionPos.clear();

		int h = c->mImg.rows;
		int w = c->mImg.cols;

		if (mdtp.DET_DOWNSAMPLE_ENABLED) {
			h /= 2;
			w /= 2;
		}

		ImgProcessing::subdivideFrame(mSubdivisionPos, 8, h, w);
		mSubdivisionStatus = true;

		if (mdtp.DET_DEBUG) {
			Mat s = Mat(h, w, CV_8UC1, Scalar(0));

			for (int i = 0; i < 8; i++) {
				line(s, Point(0, i * (h / 8)), Point(w - 1, i * (h / 8)),
					Scalar(255), 1);
				line(s, Point(i * (w / 8), 0), Point(i * (w / 8), h - 1),
					Scalar(255), 1);
			}

			SaveImg::saveBMP(s, mDebugCurrentPath + "subdivisions_map");
		}
	}
	else {
		//double tStep1 = 0;
		//double tStep2 = 0;
		//double tStep3 = 0;
		//double tStep4 = 0;
		//double tTotal = (double)getTickCount();

		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%% STEP 1 : FILETRING / THRESHOLDING %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

		//tStep1 = (double)getTickCount();
		Mat currImg;

		// ------------------------------
		//    Downsample current frame.
		// -------------------------------

		if (mdtp.DET_DOWNSAMPLE_ENABLED) {
			pyrDown(c->mImg, currImg, Size(c->mImg.cols / 2, c->mImg.rows / 2));
			if (mMask.data && (mMask.rows != currImg.rows || mMask.cols != currImg.cols)) {
				Mat tmp;
				mMask.copyTo(tmp);
				cv::resize(tmp, mMask, Size(c->mImg.cols / 2, c->mImg.rows / 2));
			}
		}
		else {
			c->mImg.copyTo(currImg);
		}

		if (!mMask.data) {
			mMask = Mat(currImg.rows, currImg.cols, CV_8UC1,Scalar(255));
		}

		// --------------------------------
		//      Check previous frame.
		// --------------------------------

		if (!mPrevFrame.data) {
			currImg.copyTo(mPrevFrame);
			return nullptr;
		}

		// --------------------------------
		//          Differences.
		// --------------------------------

		Mat absdiffImg, posDiffImg, negDiffImg;

		// Absolute difference.
		cv::absdiff(currImg, mPrevFrame, absdiffImg);

		// Positive difference.
		cv::subtract(currImg, mPrevFrame, posDiffImg, mMask);

		// Negative difference.
		cv::subtract(mPrevFrame, currImg, negDiffImg, mMask);

		// ---------------------------------
		//  Dilatate absolute difference.
		// ---------------------------------

		int dilation_size = 2;
		Mat element = getStructuringElement(
			MORPH_RECT, Size(2 * dilation_size + 1, 2 * dilation_size + 1),
			Point(dilation_size, dilation_size));
		cv::dilate(absdiffImg, absdiffImg, element);

		// ------------------------------------------------------------------------------
		//   Threshold absolute difference / positive difference / negative
		//   difference
		// ------------------------------------------------------------------------------

		Mat absDiffBinaryMap = ImgProcessing::thresholding( absdiffImg, mMask, 3, Thresh::MEAN);

		Scalar meanPosDiff, stddevPosDiff, meanNegDiff, stddevNegDiff;
		cv::meanStdDev(posDiffImg, meanPosDiff, stddevPosDiff, mMask);
		cv::meanStdDev(negDiffImg, meanNegDiff, stddevNegDiff, mMask);
		int posThreshold = stddevPosDiff[0] * 5 + 10;
		int negThreshold = stddevNegDiff[0] * 5 + 10;

		if (mdtp.DET_DEBUG) {
			Mat posBinaryMap = ImgProcessing::thresholding( posDiffImg, mMask, 5, Thresh::STDEV);
			Mat negBinaryMap = ImgProcessing::thresholding( negDiffImg, mMask, 5, Thresh::STDEV);

			SaveImg::saveBMP(Conversion::convertTo8UC1(currImg),
				mDebugCurrentPath + "/original/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(posBinaryMap,
				mDebugCurrentPath + "/pos_difference_thresholded/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(negBinaryMap,
				mDebugCurrentPath + "/neg_difference_thresholded/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(absDiffBinaryMap,
				mDebugCurrentPath + "/absolute_difference_thresholded/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(absdiffImg,
				mDebugCurrentPath + "/absolute_difference/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(Conversion::convertTo8UC1(posDiffImg),
				mDebugCurrentPath + "/pos_difference/frame_" + Conversion::intToString(c->mFrameNumber));
			SaveImg::saveBMP(Conversion::convertTo8UC1(negDiffImg),
				mDebugCurrentPath + "/neg_difference/frame_" + Conversion::intToString(c->mFrameNumber));
		}

		// Current frame is stored as the previous frame.
		currImg.copyTo(mPrevFrame);

		//tStep1 = (double)getTickCount() - tStep1;

		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% STEP 2 : FIND LOCAL EVENT %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

		// SUMMARY :
		// Loop binarized absolute difference image.
		// For each white pixel, define a Region of interest (ROI) of 10x10
		// centered in this pixel. Create a new Local Event initialized with
		// this first ROI or attach this ROI to an existing Local Event.
		// Loop the ROI in the binarized absolute difference image to store
		// position of white pixels. Loop the ROI in the positive difference
		// image to store positions of white pixels. Loop the ROI in the
		// negative difference image to store positions of white pixels.
		// Once the list of Local Event has been completed :
		// Analyze each local event in order to check that pixels can be
		// clearly split in two groups (negative, positive).

		list<std::shared_ptr<LocalEvent>> listLocalEvents;
		//tStep2 = (double)getTickCount();

		// Event map for the current frame.
		Mat eventMap = Mat(currImg.rows, currImg.cols, CV_8UC3, Scalar(0, 0, 0));

		// ----------------------------------
		//        Search local events.
		// ----------------------------------

		for (auto itR = mSubdivisionPos.begin(); itR != mSubdivisionPos.end(); ++itR) {
			// Extract subdivision from binary map.
			Mat subdivision = absDiffBinaryMap(
				Rect(itR->x, itR->y, absDiffBinaryMap.cols / 8,
					absDiffBinaryMap.rows / 8));

			// Check if there is white pixels.
			if (countNonZero(subdivision) > 0) {
				analyseRegion(subdivision, absDiffBinaryMap, eventMap,
					posDiffImg, posThreshold, negDiffImg,
					negThreshold, listLocalEvents, (*itR),
					mdtp.temporal.DET_LE_MAX, c->mFrameNumber, c->mDate);
			}
		}

		int i = 0;
		for (auto& ev : listLocalEvents) {
			ev->setLeIndex(i++);
		}

		if (mdtp.DET_DEBUG) {
			SaveImg::saveBMP(eventMap, mDebugCurrentPath + "/event_map_initial/frame_" + Conversion::intToString(c->mFrameNumber));
		}

		// ----------------------------------
		//         Link between LE.
		// ----------------------------------

		// Iterator list on localEvent list : localEvent contains a positive
		// or negative cluster.
		list<list<std::shared_ptr<LocalEvent>>::iterator> itLePos, itLeNeg;

		// Search pos and neg alone.
		for (auto itLE = listLocalEvents.begin(); itLE != listLocalEvents.end(); itLE++) {
			// Le has pos cluster but no neg cluster.
			if ((*itLE)->getPosClusterStatus() && !(*itLE)->getNegClusterStatus()) {
				itLePos.push_back(itLE);
			}
			else if (!(*itLE)->getPosClusterStatus() && (*itLE)->getNegClusterStatus()) {
				itLeNeg.push_back(itLE);
			}
		}

		float maxRadius = 50.f;
		// Try to link a positive cluster to a negative one.
		for (auto& leP : itLePos) {
			int nbPotentialNeg = 0;
			list<std::shared_ptr<LocalEvent>>::iterator itChoose;
			list<list<std::shared_ptr<LocalEvent>>::iterator>::iterator c;

			for (auto j = itLeNeg.begin(); j != itLeNeg.end(); j++) {
				Point A = (*leP)->getMassCenter();
				Point B = (**j)->getMassCenter();
				float dist = sqrt(pow((A.x - B.x), 2) + pow((A.y - B.y), 2));

				if (dist < maxRadius) {
					nbPotentialNeg++;
					itChoose = *j;
					c = j;
				}
			}

			if (nbPotentialNeg == 1) {
				(*leP)->mergeWithAnOtherLE(**itChoose);
				(*leP)->setMergedStatus(true);
				(*itChoose)->setMergedStatus(true);
				itLeNeg.erase(c);
			}
		}

		// Delete pos cluster not merged and negative cluster not merged.
		// Search pos and neg alone.
		for (auto itLE = listLocalEvents.begin(); itLE != listLocalEvents.end();) {
			// Le has pos cluster but no neg cluster.
			if ( // (itLE->getPosClusterStatus() &&
				// !itLE->getNegClusterStatus() &&
				// !itLE->getMergedStatus())||
				(!(*itLE)->getPosClusterStatus() && (*itLE)->getNegClusterStatus() && (*itLE)->getMergedStatus())) {
				itLE = listLocalEvents.erase(itLE);
			}
			else {
				++itLE;
			}
		}

		// -----------------------------------
		//            Circle TEST.
		// -----------------------------------
		for (auto itLE = listLocalEvents.begin(); itLE != listLocalEvents.end();) {
			if ((*itLE)->getPosClusterStatus() && (*itLE)->getNegClusterStatus()) {
				if ((*itLE)->localEventIsValid()) {
					++itLE;
				}
				else {
					itLE = listLocalEvents.erase(itLE);
				}
			}
			else {
				++itLE;
			}
		}

		if (mdtp.DET_DEBUG) {
			Mat eventMapFiltered = Mat(currImg.rows, currImg.cols, CV_8UC3, Scalar(0, 0, 0));

			for (auto& le : listLocalEvents) {
				Mat roiF(10, 10, CV_8UC3, le->getColor());

				for (auto& roi : le->mLeRoiList) {
					if (roi.x - 5 > 0 && roi.x + 5 < eventMapFiltered.cols && roi.y - 5 > 0 && roi.y + 5 < eventMapFiltered.rows) {
						roiF.copyTo(eventMapFiltered(Rect(roi.x - 5, roi.y - 5, 10, 10)));
					}
				}
			}

			SaveImg::saveBMP(eventMapFiltered,
				mDebugCurrentPath + "/event_map_filtered/frame_" + Conversion::intToString(c->mFrameNumber));
		}

		//tStep2 = (double)getTickCount() - tStep2;

		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%% STEP 3 : ATTACH LE TO GE OR CREATE NEW ONE %%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

		// SUMMARY :
		// Loop list of local events.
		// Create a new global event initialized with the current Local
		// event or attach it to an existing global event. If attached,
		// check the positive-negative couple of the global event.

		//tStep3 = (double)getTickCount();

		for (auto itLE = listLocalEvents.begin(); itLE != listLocalEvents.end();) {
			std::shared_ptr<GlobalEvent> pGESelected = nullptr;
			(*itLE)->setNumFrame(c->mFrameNumber);

			for (auto& ge : mListGlobalEvents) {
				Mat res = (*itLE)->getMap() & ge->getMapEvent();
				if (countNonZero(res) > 0) {
					// The current LE has found a possible global event.
					if (pGESelected) {
						// cout << "The current LE has found a possible global event."<< endl;
						// Choose the older global event.
						if (ge->getAge() > pGESelected->getAge()) {
							// cout << "Choose the older global event."<< endl;
							pGESelected = ge;
						}
					}
					else {
						// cout << "Keep same"<< endl;
						pGESelected = ge;
					}
				}
			}

			// Add current LE to an existing GE
			if (pGESelected) {
				// cout << "Add current LE to an existing GE ... "<< endl;
				// Add LE.
				pGESelected->addLE(*itLE);
				// cout << "Flag to indicate that a local event has been
				// added ... "<< endl;
				// Flag to indicate that a local event has been added.
				pGESelected->setNewLEStatus(true);
				// cout << "reset age of the last local event received by
				// the global event.... "<< endl;
				// reset age of the last local event received by the global
				// event.
				pGESelected->setAgeLastElem(0);
			}
			else {
				// The current LE has not been linked. It became a new GE.
				if (mListGlobalEvents.size() < mdtp.temporal.DET_GE_MAX) {
					// cout << "Selecting last available color ... "<< endl;
					Scalar geColor = Scalar(255, 255, 255); // availableGeColor.back();
					// cout << "Deleting last available color ... "<< endl;
					// availableGeColor.pop_back();
					// cout << "Creating new GE ... "<< endl;
					std::shared_ptr<GlobalEvent> newGE(std::make_shared<GlobalEvent>(c->mDate, c, currImg.rows, currImg.cols, geColor, &mdtp));
					// cout << "Adding current LE ... "<< endl;
					newGE->addLE(*itLE);
					// cout << "Pushing new LE to GE list  ... "<< endl;
					// Add the new globalEvent to the globalEvent's list
					mListGlobalEvents.push_back(newGE);
				}
			}

			itLE = listLocalEvents.erase(itLE); // Delete the current localEvent.
		}

		//tStep3 = (double)getTickCount() - tStep3;

		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%% STEP 4 : MANAGE LIST GLOBAL EVENT %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		/// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

		//tStep4 = (double)getTickCount(); // Count process time of step 4.
		bool saveSignal = false; // Returned signal to indicate to save a GE or not.

		// Loop global event list to check their characteristics.
		for (auto itGE = mListGlobalEvents.begin(); itGE != mListGlobalEvents.end();) {
			(*itGE)->setAge((*itGE)->getAge() + 1); // Increment age.
			// If the current global event has not received any new local
			// event.
			if (!(*itGE)->getNewLEStatus()) {
				// Increment its "Without any new local event age"
				(*itGE)->setAgeLastElem((*itGE)->getAgeLastElem() + 1);
			}
			else {
				//(*itGE)->setNumLastFrame(c->mFrameNumber);
				(*itGE)->AddFrame(c);
				(*itGE)->setNewLEStatus(false);
			}

			// CASE 1 : FINISHED EVENT.
			if ((*itGE)->getAgeLastElem() > 5) {
				// Linear profil ? Minimum duration respected ?
				if ((*itGE)->LEList.size() >= 5 && 
					(*itGE)->continuousGoodPos(4) && 
					(*itGE)->ratioFramesDist() && 
					(*itGE)->negPosClusterFilter() &&
                    (*itGE)->Speed() > mdtp.DET_SPEED) {
					mGeToSave = itGE;
					saveSignal = true;
					break;
				}
				else {
					itGE = mListGlobalEvents.erase(itGE); // Delete the event.
				}
			}
			// CASE 2 : NOT FINISHED EVENT.
			else {
				int nbsec = TimeDate::secBetweenTwoDates((*itGE)->getDate(), c->mDate);
				bool maxtime = false;
				if (nbsec > mdtp.DET_TIME_MAX)
					maxtime = true;

				if (maxtime) {
					logger->info("# GE deleted because max time reached - itGE->getDate() : {}",
						TimeDate::getYYYYMMDDThhmmss((*itGE)->getDate()));
					logger->info("- c.mDate : {}", TimeDate::getYYYYMMDDThhmmss(c->mDate));
					logger->info("- difftime in sec : {}", nbsec);
					logger->info("- maxtime in sec : {}", mdtp.DET_TIME_MAX);
					itGE = mListGlobalEvents.erase(itGE); // Delete the event.
					// Let the GE alive.
				}
				// Check some characteristics : Too long event ? not linear ?
				else if ((!(*itGE)->getLinearStatus() && !(*itGE)->continuousGoodPos(5))) {
					itGE = mListGlobalEvents.erase(itGE); // Delete the event.
				}
				else if ((!(*itGE)->getLinearStatus() && (*itGE)->continuousBadPos((int)(*itGE)->getAge() / 2))) {
					itGE = mListGlobalEvents.erase(itGE); // Delete the event.
				}
				else if (c->mFrameRemaining < 10 && c->mFrameRemaining != 0) {
					if ((*itGE)->LEList.size() >= 5 && 
						(*itGE)->continuousGoodPos(4) && 
						(*itGE)->ratioFramesDist() && 
						(*itGE)->negPosClusterFilter() &&
                        (*itGE)->Speed() > mdtp.DET_SPEED) {
						mGeToSave = itGE;
						saveSignal = true;
						break;
					}
					else {
						itGE = mListGlobalEvents.erase(itGE); // Delete the event.
					}
				}
				else {
					++itGE; // Do nothing to the current GE, check the following one.
				}
			}
		}

		//tStep4 = (double)getTickCount() - tStep4;
		//tTotal = (double)getTickCount() - tTotal;
		//double freq = getTickFrequency();
		//logger->debug("{},tt:{}ms,s1:{}ms,s2:{}ms,s3:{}ms,s4:{}ms", 
		//	c->mFrameNumber,
		//	tTotal * 1000 / freq,
		//	tStep1 * 1000 / freq,
		//	tStep2 * 1000 / freq,
		//	tStep3 * 1000 / freq,
		//	tStep4 * 1000 / freq);
		if (saveSignal) {
			auto ret = *mGeToSave;
			mListGlobalEvents.erase(mGeToSave);
			return ret;
		}
		else {
			return nullptr;
		}
	}

	return nullptr;
}

vector<Scalar> DetectionTemporal::getColorInEventMap(Mat& eventMap, Point roiCenter)
{
	// ROI in the eventMap.
	Mat roi;

	// ROI extraction from the eventmap.
	eventMap(Rect(roiCenter.x - mRoiSize[0] / 2, roiCenter.y - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
		.copyTo(roi);

	unsigned char* ptr = (unsigned char*)roi.data;
	int cn = roi.channels();
	vector<Scalar> listColor;
	bool exist = false;

	for (int i = 0; i < roi.rows; i++) {
		for (int j = 0; j < roi.cols; j++) {
			Scalar bgrPixel;
			bgrPixel.val[0] = ptr[i * roi.cols * cn + j * cn + 0]; // B
			bgrPixel.val[1] = ptr[i * roi.cols * cn + j * cn + 1]; // G
			bgrPixel.val[2] = ptr[i * roi.cols * cn + j * cn + 2]; // R

			if (bgrPixel.val[0] != 0 || bgrPixel.val[1] != 0 || bgrPixel.val[2] != 0) {
				for (int k = 0; k < listColor.size(); k++) {
					if (bgrPixel == listColor.at(k)) {
						exist = true;
						break;
					}
				}

				if (!exist)
					listColor.push_back(bgrPixel);

				exist = false;
			}
		}
	}

	return listColor;
}

void DetectionTemporal::colorRoiInBlack(Point p, int h, int w, Mat& region)
{
	int posX = p.x - w;
	int posY = p.y - h;

	if (p.x - w < 0) {
		w = p.x + w / 2;
		posX = 0;
	}
	else if (p.x + w / 2 > region.cols) {
		w = region.cols - p.x + w / 2;
	}

	if (p.y - h < 0) {
		h = p.y + h / 2;
		posY = 0;
	}
	else if (p.y + h / 2 > region.rows) {
		h = region.rows - p.y + h / 2;
	}

	// Color roi in black in the current region.
	Mat roiBlackRegion(h, w, CV_8UC1, Scalar(0));
	roiBlackRegion.copyTo(region(Rect(posX, posY, w, h)));
}

void DetectionTemporal::analyseRegion(
	Mat& subdivision, Mat& absDiffBinaryMap, Mat& eventMap, Mat& posDiff,
	int posDiffThreshold, Mat& negDiff, int negDiffThreshold,
	list<std::shared_ptr<LocalEvent>>& listLE,
	Point subdivisionPos, // Origin position of a region in frame (corner top left)
	int maxNbLE, int numFrame, TimeDate::Date cFrameDate)
{
	int situation = 0;
	int nbCreatedLE = 0;
	int nbRoiAttachedToLE = 0;
	int nbNoCreatedLE = 0;
	int nbROI = 0;
	int nbRoiNotAnalysed = 0;
	int roicounter = 0;

	unsigned char* ptr;
	auto logger = spdlog::get("det_logger");

	// Loop pixel's subdivision.
	for (int i = 0; i < subdivision.rows; i++) {
		ptr = subdivision.ptr<unsigned char>(i);

		for (int j = 0; j < subdivision.cols; j++) {
			// Pixel is white.
			if ((int)ptr[j] > 0) {
				// Check if we are not out of frame range when a ROI is defined
				// at the current pixel location.
				if ((subdivisionPos.y + i - mRoiSize[1] / 2 > 0) && 
					(subdivisionPos.y + i + mRoiSize[1] / 2 < absDiffBinaryMap.rows) && 
					(subdivisionPos.x + j - mRoiSize[0] / 2 > 0) && 
					(subdivisionPos.x + j + mRoiSize[0] / 2 < absDiffBinaryMap.cols)) {
					nbROI++;
					roicounter++;
					// Get colors in eventMap at the current ROI location.
					vector<Scalar> listColorInRoi = getColorInEventMap(eventMap, Point(subdivisionPos.x + j, subdivisionPos.y + i));

					if (listColorInRoi.size() == 0)
						situation = 0; // black color = create a new local event
					if (listColorInRoi.size() == 1)
						situation = 1; // one color = add the current roi to an existing local event
					if (listColorInRoi.size() > 1)
						situation = 2; // several colors = make a decision

					switch (situation) {
					case 0:
					{
						if (listLE.size() < maxNbLE) {
							// Create new localEvent object.
							std::shared_ptr<LocalEvent> newLocalEvent(std::make_shared<LocalEvent>(
								mListColors.at(listLE.size()),
								Point(subdivisionPos.x + j, subdivisionPos.y + i),
								absDiffBinaryMap.rows, absDiffBinaryMap.cols, mRoiSize));

							// Extract white pixels in ROI.
							vector<Point> whitePixAbsDiff, whitePixPosDiff, whitePixNegDiff;
							Mat roiAbsDiff, roiPosDiff, roiNegDiff;

							absDiffBinaryMap(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
								.copyTo(roiAbsDiff);
							posDiff(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
								.copyTo(roiPosDiff);
							negDiff(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
								.copyTo(roiNegDiff);

							if (roiPosDiff.type() == CV_16UC1 && roiNegDiff.type() == CV_16UC1) {
								unsigned char* ptrRoiAbsDiff;
								unsigned short* ptrRoiPosDiff;
								unsigned short* ptrRoiNegDiff;

								for (int a = 0; a < roiAbsDiff.rows; a++) {
									ptrRoiAbsDiff = roiAbsDiff.ptr<unsigned char>(a);
									ptrRoiPosDiff = roiPosDiff.ptr<unsigned short>(a);
									ptrRoiNegDiff = roiNegDiff.ptr<unsigned short>(a);

									for (int b = 0; b < roiAbsDiff.cols;
										b++) {
										if (ptrRoiAbsDiff[b] > 0)
											whitePixAbsDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										if (ptrRoiPosDiff[b] > posDiffThreshold)
											whitePixPosDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										if (ptrRoiNegDiff[b] > negDiffThreshold)
											whitePixNegDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
									}
								}

							}
							else if (roiPosDiff.type() == CV_8UC1 && roiNegDiff.type() == CV_8UC1) {
								unsigned char* ptrRoiAbsDiff;
								unsigned char* ptrRoiPosDiff;
								unsigned char* ptrRoiNegDiff;

								for (int a = 0; a < roiAbsDiff.rows; a++) {
									ptrRoiAbsDiff = roiAbsDiff.ptr<unsigned char>(a);
									ptrRoiPosDiff = roiPosDiff.ptr<unsigned char>(a);
									ptrRoiNegDiff = roiNegDiff.ptr<unsigned char>(a);

									for (int b = 0; b < roiAbsDiff.cols;
										b++) {
										if (ptrRoiAbsDiff[b] > 0)
											whitePixAbsDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										if (ptrRoiPosDiff[b] > posDiffThreshold)
											whitePixPosDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										if (ptrRoiNegDiff[b] > negDiffThreshold)
											whitePixNegDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
									}
								}
							}

							newLocalEvent->addAbs(whitePixAbsDiff);
							newLocalEvent->addPos(whitePixPosDiff);
							newLocalEvent->addNeg(whitePixNegDiff);

							// Update center of mass.
							newLocalEvent->computeMassCenter();

							// Save the frame number where the local event
							// has been created.
							newLocalEvent->setNumFrame(numFrame);
							// Save acquisition date of the frame.
							newLocalEvent->mFrameAcqDate = cFrameDate;
							// Add LE in the list of localEvent.
							listLE.push_back(newLocalEvent);
							// Update eventMap with the color of the new
							// localEvent.
							Mat roi(mRoiSize[1], mRoiSize[0], CV_8UC3,
								mListColors.at(listLE.size() - 1));
							roi.copyTo(eventMap(
								Rect(subdivisionPos.x + j - mRoiSize[0] / 2,
									subdivisionPos.y + i - mRoiSize[1] / 2,
									mRoiSize[0], mRoiSize[1])));
							// Color roi in black in the current region.
							colorRoiInBlack(Point(j, i), mRoiSize[1], mRoiSize[0], subdivision);

							colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], absDiffBinaryMap);
							colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], posDiff);
							colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], negDiff);

							nbCreatedLE++;
						}
						else {
							nbNoCreatedLE++;
						}
					}
					break;
					case 1:
					{
						int index = 0;
						for (auto it = listLE.begin(); it != listLE.end(); ++it) {
							// Try to find a local event which has the same
							// color.
							if ((*it)->getColor() == listColorInRoi.at(0)) {
								// Extract white pixels in ROI.
								vector<Point> whitePixAbsDiff, whitePixPosDiff, whitePixNegDiff;
								Mat roiAbsDiff, roiPosDiff, roiNegDiff;

								absDiffBinaryMap(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
									.copyTo(roiAbsDiff);
								posDiff(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
									.copyTo(roiPosDiff);
								negDiff(Rect(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1]))
									.copyTo(roiNegDiff);

								if (roiPosDiff.type() == CV_16UC1 && roiNegDiff.type() == CV_16UC1) {
									unsigned char* ptrRoiAbsDiff;
									unsigned short* ptrRoiPosDiff;
									unsigned short* ptrRoiNegDiff;

									for (int a = 0; a < roiAbsDiff.rows; a++) {
										ptrRoiAbsDiff = roiAbsDiff.ptr<unsigned char>(a);
										ptrRoiPosDiff = roiPosDiff.ptr<unsigned short>(a);
										ptrRoiNegDiff = roiNegDiff.ptr<unsigned short>(a);

										for (int b = 0; b < roiAbsDiff.cols; b++) {
											if (ptrRoiAbsDiff[b] > 0)
												whitePixAbsDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
											if (ptrRoiPosDiff[b] > posDiffThreshold)
												whitePixPosDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
											if (ptrRoiNegDiff[b] > negDiffThreshold)
												whitePixNegDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										}
									}
								}
								else if (roiPosDiff.type() == CV_8UC1 && roiNegDiff.type() == CV_8UC1) {
									unsigned char* ptrRoiAbsDiff;
									unsigned char* ptrRoiPosDiff;
									unsigned char* ptrRoiNegDiff;

									for (int a = 0; a < roiAbsDiff.rows; a++) {
										ptrRoiAbsDiff = roiAbsDiff.ptr<unsigned char>(a);
										ptrRoiPosDiff = roiPosDiff.ptr<unsigned char>(a);
										ptrRoiNegDiff = roiNegDiff.ptr<unsigned char>(a);

										for (int b = 0; b < roiAbsDiff.cols; b++) {
											if (ptrRoiAbsDiff[b] > 0)
												whitePixAbsDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
											if (ptrRoiPosDiff[b] > posDiffThreshold)
												whitePixPosDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
											if (ptrRoiNegDiff[b] > negDiffThreshold)
												whitePixNegDiff.emplace_back(subdivisionPos.x + j - mRoiSize[0] / 2 + b, subdivisionPos.y + i - mRoiSize[1] / 2 + a);
										}
									}
								}

								(*it)->addAbs(whitePixAbsDiff);
								(*it)->addPos(whitePixPosDiff);
								(*it)->addNeg(whitePixNegDiff);

								// Add the current roi.
								(*it)->mLeRoiList.emplace_back(subdivisionPos.x + j, subdivisionPos.y + i);
								// Set local event 's map
								(*it)->setMap(Point(subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2), mRoiSize[1], mRoiSize[0]);
								// Update center of mass
								(*it)->computeMassCenter();

								// Update eventMap with the color of the new
								// localEvent
								Mat roi(mRoiSize[1], mRoiSize[0], CV_8UC3, listColorInRoi.at(0));
								roi.copyTo(eventMap(Rect( subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1])));
								// Color roi in black in thresholded frame.
								Mat roiBlack(mRoiSize[1], mRoiSize[0], CV_8UC1, Scalar(0));
								roiBlack.copyTo(absDiffBinaryMap(Rect( subdivisionPos.x + j - mRoiSize[0] / 2, subdivisionPos.y + i - mRoiSize[1] / 2, mRoiSize[0], mRoiSize[1])));
								// Color roi in black in the current region.
								colorRoiInBlack(Point(j, i), mRoiSize[1], mRoiSize[0], subdivision);

								colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], absDiffBinaryMap);
								colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], posDiff);
								colorRoiInBlack(Point(subdivisionPos.x + j, subdivisionPos.y + i), mRoiSize[1], mRoiSize[0], negDiff);

								nbRoiAttachedToLE++;

								break;
							}

							index++;
						}
					}
					break;
					case 2:
					{
						nbRoiNotAnalysed++;
						/* vector<LocalEvent>::iterator it;
							 vector<LocalEvent>::iterator itLEbase;
							 it = listLE.begin();

							 vector<Scalar>::iterator it2;
							 it2 = listColorInRoi.begin();

							 bool LE = false;
							 bool colorFound = false;

							 while (it != listLE.end()){

								 // Check if the current LE have a color.
								 while (it2 != listColorInRoi.end()){

									 if(it->getColor() == (*it2)){

										colorFound = true;
										it2 = listColorInRoi.erase(it2);
										break;

									 }

									 ++it2;
								 }

								 if(colorFound){

									 if(!LE){

										 itLEbase = it;
										 LE = true;

										 (*it).LE_Roi.push_back(Point(areaPosition.x
							 + j, areaPosition.y + i));

										 Mat tempMat = (*it).getMap();
										 Mat
							 roiTemp(roiSize[1],roiSize[0],CV_8UC1,Scalar(255));
										 roiTemp.copyTo(tempMat(Rect(areaPosition.x
							 + j - roiSize[0]/2, areaPosition.y + i -
							 roiSize[1]/2, roiSize[0], roiSize[1])));
										 (*it).setMap(tempMat);

										 // Update center of mass
										 (*it).computeMassCenterWithRoi();

										 // Update eventMap with the color of
							 the new localEvent group Mat roi(roiSize[1],
							 roiSize[0], CV_8UC3, listColorInRoi.at(0));
										 roi.copyTo(eventMap(Rect(areaPosition.x
							 + j-roiSize[0]/2, areaPosition.y +
							 i-roiSize[1]/2,roiSize[0],roiSize[1])));

										 colorInBlack(j, i, areaPosX, areaPosY,
							 areaPosition, area, frame);

									 }else{

										 // Merge LE data

										 Mat temp = (*it).getMap();
										 Mat temp2 = (*itLEbase).getMap();
										 Mat temp3 = temp + temp2;
										 (*itLEbase).setMap(temp3);

										 (*itLEbase).LE_Roi.insert((*itLEbase).LE_Roi.end(),
							 (*it).LE_Roi.begin(), (*it).LE_Roi.end());

										 it = listLE.erase(it);

									 }

									 colorFound = false;

								 }else{

									 ++it;

								 }
							 }*/

					}
					break;
					}
				}
			}
		}
	}
}
