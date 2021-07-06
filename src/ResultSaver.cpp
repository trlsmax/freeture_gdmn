#include "ResultSaver.h"
#include <spdlog/spdlog.h>
#include "Stack.h"
#include "DataPaths.h"

ResultSaver::ResultSaver(stationParam* stp, dataParam* dp)
    : mstp(stp)
    , mdp(dp)
{
	running = true;
	thd = new std::thread(&ResultSaver::Run, this);
	thd->detach();
}

ResultSaver::~ResultSaver(void)
{
	Terminate();
}

void ResultSaver::Terminate(void)
{
	running = false;
	if (thd && thd->joinable()) {
		thd->join();
	}
	delete thd;
	thd = nullptr;
}

void ResultSaver::AddResult(std::shared_ptr<GlobalEvent> ge)
{
	std::unique_lock<std::mutex> lock(mtx);
	queue.push(ge);
	cv.notify_one();
}

void ResultSaver::Run()
{
	try {
		do {
            std::vector<std::shared_ptr<GlobalEvent>> ges;
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock);

			while (!queue.empty()) {
                ges.push_back(queue.back());
                queue.pop();
			}
			lock.unlock();

            for (std::shared_ptr<GlobalEvent>& ge : ges) {
                std::string eventPath = buildEventDataDirectory(ge->getDate());
                if (eventPath.empty()) {
                    continue;
                }
                eventPath += mstp->TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(ge->getDate());
                saveDetectionInfos(ge.get(), eventPath);
                saveEventData(ge.get(), eventPath);
            }

            ges.clear();
		} while (running);
	}
	catch (std::exception& e) {

	}
}

void ResultSaver::saveDetectionInfos(GlobalEvent* ge, string path)
{
    if (!ge) {
        return;
    }

	// Save ge map.
	if (ge->DetectionParam()->temporal.DET_SAVE_GEMAP) {
		SaveImg::saveBMP(ge->getMapEvent(), path + "GeMap");
		//debugFiles.push_back("GeMap.bmp");
	}

	// Save dir map.
	if (ge->DetectionParam()->temporal.DET_SAVE_DIRMAP) {
		SaveImg::saveBMP(ge->getDirMap(), path + "DirMap");
	}

	// Save positions.
	if (ge->DetectionParam()->temporal.DET_SAVE_POS) {
		std::ofstream posFile;
		// string posFilePath = p + "positions.txt";
		string posFilePath = path + "_positions.csv";

		posFile.open(posFilePath.c_str());

		// Number of the first frame associated to the event.
		int numFirstFrame = -1;

        auto frame = ge->Frames().front();
		// write csv header
		string line = "frame_id,datetime,x_fits,y_fits\n";
		posFile << line;

		for (auto itLe = ge->LEList.begin(); itLe != ge->LEList.end(); ++itLe) {
			if (numFirstFrame == -1)
				numFirstFrame = (*itLe)->getNumFrame();

            cv::Point pos = (*itLe)->getMassCenter();

			int positionY = 0;
			if (ge->DetectionParam()->DET_DOWNSAMPLE_ENABLED) {
				pos *= 2;
				positionY = frame->mImg.rows * 2 - pos.y;
			}
			else {
				positionY = frame->mImg.rows - pos.y;
			}

			// NUM_FRAME    POSITIONX     POSITIONY (inversed)
			// string line = Conversion::intToString(itLE->getNumFrame() -
			// numFirstFrame + nbFramesAround) + "               (" +
			// Conversion::intToString(pos.x)  + ";" +
			// Conversion::intToString(positionY) + ")                 " +
			// TimeDate::getIsoExtendedFormatDate(itLE->mFrameAcqDate)+ "\n";
			string line = Conversion::intToString((*itLe)->getNumFrame() - numFirstFrame + ge->FramesAround()) + 
				"," + TimeDate::getIsoExtendedFormatDate((*itLe)->mFrameAcqDate) + 
				"," + Conversion::intToString(pos.x) + 
				"," + Conversion::intToString(positionY) + "\n";
			posFile << line;
		}

		line = "Speed," + std::to_string(ge->Speed()) + ",pixel/s";
		posFile << line;

		posFile.close();
	}
}

bool ResultSaver::saveEventData(GlobalEvent* ge, std::string EventPath)
{
    if (!ge) {
        return false;
    }

	auto logger = spdlog::get("det_logger");

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
    cv::VideoWriter* video = NULL;

	if (ge->DetectionParam()->DET_SAVE_AVI) {
		// third parameter controls FPS. Might need to change that one (used to
		// be 5 fps).
		video = new cv::VideoWriter(
			EventPath + "_video.avi",
			cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30,
			cv::Size(static_cast<int>(ge->Frames().front()->mImg.cols),
				static_cast<int>(ge->Frames().front()->mImg.rows)),
			false);
	}

	// Init sum.
	Stack stack;

	// Loop framebuffer.
	for (auto frame : ge->Frames()) {
		if (ge->DetectionParam()->DET_SAVE_AVI) {
			if (video->isOpened()) {
				if (frame->mImg.type() != CV_8UC1) {
                    cv::Mat iv = Conversion::convertTo8UC1(frame->mImg);
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

	if (ge->DetectionParam()->DET_SAVE_AVI) {
		if (video != NULL)
			delete video;
	}

	// EVENT STACK WITH HISTOGRAM EQUALIZATION
	if (ge->DetectionParam()->DET_SAVE_SUM_WITH_HIST_EQUALIZATION) {
		SaveImg::saveJPEG(stack.getStack(), EventPath);
	}

	return true;
}

std::string ResultSaver::buildEventDataDirectory(TimeDate::Date eventDate)
{
	namespace fs = ghc::filesystem;
	auto logger = spdlog::get("det_logger");

	// eventDate is the date of the first frame attached to the event.
	// string YYYYMMDD = TimeDate::getYYYYMMDD(mEventDate);

	// Data location.
	fs::path p(mdp->DATA_PATH);

	// Create data directory for the current day.
	// string fp = mdp.DATA_PATH + mStationName + "_" + YYYYMMDD +"/";
	string fp = DataPaths::getSessionPath(mdp->DATA_PATH, eventDate);
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
	string fp2 = mstp->TELESCOP + "_" + TimeDate::getYYYY_MM_DD_hhmmss(eventDate) + "/";
	fs::path p2(fp + fp1 + fp2);
	bool ok = fs::create_directories(p2);

    if (ok) {
        return fp + fp1 + fp2;
    } else {
        return std::string();
    }
}
