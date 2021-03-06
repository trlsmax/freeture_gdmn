/*
                                main.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                       2018 Chiara Marmo
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
 * \file    main.cpp
 * \author  Yoan Audureau -- Chiara Marmo -- GEOPS-UPSUD
 * \version 1.2
 * \date    20/03/2018
 */

#include "config.h"

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>
#else
#ifdef LINUX

#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#endif
#endif

#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem.hpp>
#include <cstdio>
#include <cxxopts/cxxopts.hpp>
#include <iostream>
#include <memory>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>

#include "double_linked_list.h"
#include "AcqThread.h"
#include "CfgParam.h"
#include "Conversion.h"
#include "DetThread.h"
#include "ECamPixFmt.h"
#include "EImgBitDepth.h"
#include "EParser.h"
#include "Ephemeris.h"
#include "Frame.h"
#include "ImgProcessing.h"
#include "SaveImg.h"
#include "ResultSaver.h"

#define BOOST_NO_SCOPED_ENUMS
using namespace std;
using namespace cv;
using namespace ghc;

bool sigTermFlag = false;

const string FITS_SUFFIX = "test";

#ifdef LINUX

struct sigaction act;

void sigTermHandler(int signum, siginfo_t* info, void* ptr)
{
    auto logger = spdlog::get("mt_logger");
    logger->info("Received signal : {} from : {}", signum, (unsigned long)info->si_pid);
    spdlog::info("Received signal : {} from : {}", signum, (unsigned long)info->si_pid);
    sigTermFlag = true;
}

// http://cc.byexamples.com/2007/04/08/non-blocking-user-input-in-loop-without-ncurses/
int kbhit()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); // STDIN_FILENO is 0
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void nonblock(int state)
{
    struct termios ttystate;

    // get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);

    if (state == 1) {
        // turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        // minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    } else if (state == 0) {
        // turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }
    // set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

#endif

void init_log(string path)
{
    spdlog::default_logger()->set_pattern("%v");
    auto logger = spdlog::daily_logger_mt("mt_logger", path + "/main.txt", 0, 0);
    auto acq_logger = spdlog::daily_logger_mt("acq_logger", path + "/acq.txt", 0, 0);
    auto det_logger = spdlog::daily_logger_mt("det_logger", path + "/det.txt", 0, 0);
    auto stack_logger = spdlog::daily_logger_mt("stack_logger", path + "/stack.txt", 0, 0);
}

int main(int argc, const char** argv)
{
    cxxopts::Options options("FreeTure", "A brief description");
    options.add_options()
        ("help", "Print FreeTure help.")
        ("m,mode", "FreeTure modes :- MODE 1 : Check configuration file.- MODE 2 : Continuous acquisition.- MODE 3 : Meteor detection.- MODE 4 : Single acquisition.- MODE 5 : Clean logs.", cxxopts::value<int>())
        ("t,time", "Execution time (s) of meteor detection mode.", cxxopts::value<int>())
        ("x,startx", "Crop starting x (from left to right): only for aravis cameras yet.", cxxopts::value<int>())
        ("y,starty", "Crop starting y (from top to bottom): only for aravis cameras yet.", cxxopts::value<int>())
        ("w,width", "Image width.", cxxopts::value<int>())
        ("h,height", "Image height.", cxxopts::value<int>())
        ("c,cfg", "Configuration file's path.", cxxopts::value<std::string>()->default_value(string(CFG_PATH) + "configuration.cfg"))
        ("f,format", "Index of pixel format.", cxxopts::value<int>()->default_value("0"))
        ("bmp", "Save .bmp.")
        ("e,exposure", "Define exposure.", cxxopts::value<double>())
        ("v,version", "Print FreeTure version.")
        ("l,listdevices", "List connected devices.")
        ("a,listformats", "List device's available pixel formats.")
        ("d,id", "Camera to use. List devices to get IDs.", cxxopts::value<int>())
        ("n,filename", "Name to use when a single frame is captured.", cxxopts::value<string>()->default_value("snap"))
        ("s,sendbymail", "Send single capture by mail. Require -c option.")
        ("p,savepath", "Save path.", cxxopts::value<string>()->default_value("./"))
        ("framestats", "Print frame stats in various threads. Useful when running in foreground, disabled by default.");

    auto vm = options.parse(argc, argv);

    try {
        int mode = -1;
        int executionTime = 0;
        string configPath = string(CFG_PATH) + "configuration.cfg";
        string savePath = "./";
        int acqFormat = 0;
        int acqWidth = 0;
        int acqHeight = 0;
        int startx = 0;
        int starty = 0;
        int gain = 0;
        double exp = 0;
        string version = string(VERSION);
        int devID = 0;
        string fileName = "snap";
        bool listFormats = false;
        bool frameStats = false;

		if (vm.count("cfg"))
			configPath = vm["cfg"].as<string>();

		CfgParam cfg(configPath);
		init_log(cfg.getLogParam().LOG_PATH);
		auto logger = spdlog::get("mt_logger");
        ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        ///%%%%%%%%%%%%%%%%%%%%%%% PRINT FREETURE VERSION
        ///%%%%%%%%%%%%%%%%%%%%%%%%
        ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        if (vm.count("version")) {
            spdlog::info("Current version : {}", version);

            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%% PRINT FREETURE HELP
            ///%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        } else if (vm.count("help")) {
            spdlog::info(options.help());

            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%% LIST CONNECTED DEVICES
            ///%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        } else if (vm.count("listdevices")) {
            Device device;
            device.listDevices(true);

            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%% GET AVAILABLE PIXEL FORMATS
            ///%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        } else if (vm.count("listformats")) {
            if (vm.count("id"))
                devID = vm["id"].as<int>();

            auto* device = new Device();
            device->listDevices(false);

            if (!device->createCamera(devID, true)) {
                delete device;
            } else {
                device->getSupportedPixelFormats();
                delete device; // MC: TODO !@#$%^& some bug in destructor?
                // catched it to shout like: (process:11022):
                // GLib-GObject-CRITICAL **: g_object_unref: assertion
                // 'G_IS_OBJECT (object)' failed
            }

        } else if (vm.count("mode")) {
            mode = vm["mode"].as<int>();
            if (vm.count("time"))
                executionTime = vm["time"].as<int>();

            /// ------------------------------------------------------------------
            /// --------------------- LOAD FREETURE PARAMETERS
            /// -------------------
            /// ------------------------------------------------------------------

            switch (mode) {
            case 1:

                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%% MODE 1 : TEST/CHECK CONFIGURATION FILE
                ///%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

                {
                    spdlog::info("================================================");
                    spdlog::info("====== FREETURE - Test/Check configuration =====");
                    spdlog::info("================================================");

                    cfg.showErrors = true;
                    cfg.allParamAreCorrect();
                }

                break;

            case 2:

                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%% MODE 2 : CONTINUOUS ACQUISITION
                ///%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

                {
                    spdlog::info("================================================");
                    spdlog::info("========== FREETURE - Continuous mode ==========");
                    spdlog::info("================================================");

                    /// --------------------- Manage program options
                    /// -----------------------

                    // Acquisition format.
                    if (vm.count("format"))
                        acqFormat = vm["format"].as<int>();
                    // Cam id.
                    if (vm.count("id"))
                        devID = vm["id"].as<int>();
                    // Crop start x
                    if (vm.count("startx"))
                        startx = vm["startx"].as<int>();
                    // Crop start y
                    if (vm.count("starty"))
                        starty = vm["starty"].as<int>();
                    // Cam width size
                    if (vm.count("width"))
                        acqWidth = vm["width"].as<int>();
                    // Cam height size
                    if (vm.count("height"))
                        acqHeight = vm["height"].as<int>();
                    // Gain value.
                    if (vm.count("gain"))
                        gain = vm["gain"].as<int>();
                    // Exposure value.
                    if (vm.count("exposure"))
                        exp = vm["exposure"].as<double>();

                    // Path where to save files.
                    if (vm.count("savepath"))
                        savePath = vm["savepath"].as<string>();
                    // Filename.
                    if (vm.count("filename"))
                        fileName = vm["filename"].as<string>();

                    // default execution time is 1h in mode 2
                    executionTime = 3600;
                    if (vm.count("time"))
                        executionTime = vm["time"].as<int>();

                    EParser<CamPixFmt> fmt;
                    string fstring = fmt.getStringEnum(
                        static_cast<CamPixFmt>(acqFormat));
                    if (fstring.empty())
                        throw ">> Pixel format specified not found.";

                    spdlog::info("------------------------------------------------");
                    spdlog::info("CAM ID    : {}", devID);
                    spdlog::info("FORMAT    : {}", fstring);
                    spdlog::info("GAIN      : {}", gain);
                    spdlog::info("EXPOSURE  : {}", exp);
                    spdlog::info("WIDTH     : {}", acqWidth);
                    if (startx > 0 || starty > 0) {
                        spdlog::info("START X,Y : {},{}", startx, starty);
                    }
                    spdlog::info("HEIGHT    : {}", acqHeight);
                    spdlog::info("------------------------------------------------");

                    auto* device = new Device();
                    device->listDevices(false);
                    device->mFormat = static_cast<CamPixFmt>(acqFormat);
                    if (!device->createCamera(devID, true)) {
                        delete device;
                        throw "Fail to create device.";
                    }

                    if (acqWidth != 0 && acqHeight != 0)
                        device->setCameraSize(startx, starty, acqWidth, acqHeight);
                    else
                        device->setCameraSize();

                    if (!device->setCameraPixelFormat()) {
                        delete device;
                        throw "Fail to set format";
                    }
                    device->setCameraFPS(cfg.getCamParam().ACQ_FPS);
                    device->setCameraExposureTime(exp);
                    device->setCameraGain(gain);
                    device->initializeCamera();
                    device->startCamera();

                    /// ------------------------------------------------------------------
                    /// ----------------------- MANAGE FILE NAME
                    /// -------------------------
                    /// ------------------------------------------------------------------

                    int filenum = 0;
                    bool increment = false;
                    path p(savePath);

                    // Search previous captures in the directory.
                    for (directory_iterator file(p);
                         file != directory_iterator(); ++file) {
                        path curr(file->path());

                        if (is_regular_file(curr)) {
                            list<string> ch;
                            string fname = curr.filename().string();
                            Conversion::stringTok(ch, fname.c_str(), "-");

                            int i = 0;
                            int n = 0;

                            if (ch.front() == fileName && ch.size() == 2) {
                                list<string> ch_;
                                Conversion::stringTok(ch_, ch.back().c_str(), ".");

                                int nn = atoi(ch_.front().c_str());
                                if (nn >= filenum) {
                                    filenum = nn;
                                    increment = true;
                                }
                            }
                        }
                    }

                    if (increment)
                        filenum++;

                        /// ------------------------------------------------------------------
                        /// ---------------------- START CAPTURE of FRAMES
                        /// -------------------
                        /// ------------------------------------------------------------------

#ifdef LINUX
                    nonblock(1);
#endif

                    char hitKey;
                    int interruption = 0;

                    time_t t0, t1;
                    time(&t0);

                    while (!interruption) {
                        Frame frame;

                        auto tacq = (double)getTickCount();
                        if (device->runContinuousCapture(frame)) {
                            tacq = (((double)getTickCount() - tacq) / getTickFrequency()) * 1000;
                            spdlog::info(" >> [ TIME ACQ ] : {} ms", tacq);

                            /// ------------- DISPLAY CURRENT FRAME
                            /// --------------------

                            /// ------------- SAVE CURRENT FRAME TO FILE
                            /// ---------------

                            // Save the frame in BMP.
                            if (vm.count("bmp")) {
                                spdlog::info(">> Saving bmp file ...");

                                Mat temp1, newMat;
                                frame.mImg.copyTo(temp1);

                                if (frame.mFormat == MONO12) {
                                    newMat = ImgProcessing::correctGammaOnMono12(temp1, 2.2);
                                    Mat temp = Conversion::convertTo8UC1(newMat);
                                } else {
                                    newMat = ImgProcessing::correctGammaOnMono8(temp1, 2.2);
                                }

                                SaveImg::saveBMP(newMat, savePath + fileName + "-" + Conversion::intToString(filenum, 6));
                                spdlog::info(">> Bmp saved : {}{}-{}.bmp", savePath,
                                    fileName, Conversion::intToString(filenum, 6));
                            }

                            filenum++;
                        }

                        /// Stop freeture if escape is pressed.
#ifdef WINDOWS

                        // Sleep(1000);
                        // Exit if ESC is pressed.
                        if (GetAsyncKeyState(VK_ESCAPE) != 0)
                            interruption = 1;

#else
#ifdef LINUX

                        interruption = kbhit();
                        if (interruption != 0) {
                            hitKey = fgetc(stdin);
                            if (hitKey == 27)
                                interruption = 1;
                            else
                                interruption = 0;
                        }

                        time(&t1);
                        if ((int)(difftime(t1, t0)) > executionTime) {
                            interruption = 1;
                        }
#endif
#endif
                    }

#ifdef LINUX
                    nonblock(0);
#endif

                    device->stopCamera();
                    delete device;
                }
                break;
            case 3:
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%% MODE 3 : METEOR DETECTION
                ///%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                {
                    spdlog::info("================================================");
                    spdlog::info("======= FREETURE - Meteor detection mode =======");
                    spdlog::info("================================================");

                    logger->info("==============================================");
                    logger->info("====== FREETURE - Meteor detection mode ======");
                    logger->info("==============================================");

                    /// ------------------------------------------------------------------
                    /// --------------------- CHECK FREETURE PARAMETERS
                    /// -------------------
                    /// ------------------------------------------------------------------

                    cfg.showErrors = true;
                    if (!cfg.allParamAreCorrect()) {
                        logger->warn("No camera detected?");
                        throw "Configuration file is not correct. Fail to launch detection mode.";
                    }

                    /// ------------------------------------------------------------------
                    /// ------------------------- SHARED RESSOURCES
                    /// ----------------------
                    /// ------------------------------------------------------------------

                    // Circular buffer to store last n grabbed frames.
                    CDoubleLinkedList<std::shared_ptr<Frame>> frameBuffer;
                    mutex frameBuffer_m;
                    condition_variable frameBuffer_c;

                    bool signalDet = false;
                    mutex signalDet_m;
                    condition_variable signalDet_c;

                    mutex cfg_m;

                    /// ------------------------------------------------------------------
                    /// --------------------------- CREATE THREAD
                    /// ------------------------
                    /// ------------------------------------------------------------------

                    AcqThread* acqThread = NULL;
                    DetThread* detThread = NULL;
                    ResultSaver saver(&cfg.getStationParam(), &cfg.getDataParam());

                    try {
                        // Create detection thread.
                        if (cfg.getDetParam().DET_ENABLED) {
                            logger->info(
                                "Start to create detection thread.");

                            detThread = new DetThread(
                                &frameBuffer, &frameBuffer_m,
                                &frameBuffer_c, &signalDet, &signalDet_m,
                                &signalDet_c, cfg.getDetParam(),
                                cfg.getDataParam(), cfg.getMailParam(),
                                cfg.getStationParam(),
                                cfg.getFitskeysParam(),
                                cfg.getCamParam().ACQ_FORMAT, &saver);

                            detThread->setFrameStats(frameStats);
                            if (!detThread->startThread())
                                throw "Fail to start detection thread.";
                        }


                        // Create acquisition thread.
                        acqThread = new AcqThread(
                            &frameBuffer, &frameBuffer_m, &frameBuffer_c,
                            &signalDet, &signalDet_m, &signalDet_c,
                            detThread, cfg.getDeviceID(),
                            cfg.getDataParam(), cfg.getStationParam(), cfg.getDetParam(),
                            cfg.getCamParam(), cfg.getFramesParam(),
                            cfg.getVidParam(), cfg.getFitskeysParam());
                        acqThread->setFrameStats(frameStats);

                        if (!acqThread->startThread()) {
                            throw "Fail to start acquisition thread.";

                        } else {
                            logger->info("Success to start acquisition Thread.");
#ifdef LINUX
                            logger->info("This is the process : {}", (unsigned long)getpid());
                            memset(&act, 0, sizeof(act));
                            act.sa_sigaction = sigTermHandler;
                            act.sa_flags = SA_SIGINFO;
                            sigaction(SIGTERM, &act, NULL);
                            nonblock(1);

#endif
                            int cptTime = 0;
                            bool waitLogTime = true;
                            char hitKey;
                            int interruption = 0;

                            /// ------------------------------------------------------------------
                            /// ----------------------------- MAIN LOOP
                            /// --------------------------
                            /// ------------------------------------------------------------------
                            while (!sigTermFlag && !interruption) {
                                /// Stop freeture if escape is pressed.
#ifdef WINDOWS
                                Sleep(1000);
                                // Exit if ESC is pressed.
                                if (GetAsyncKeyState(VK_ESCAPE) != 0)
                                    interruption = 1;
#else
#ifdef LINUX
                                sleep(1);
                                interruption = kbhit();
                                if (interruption != 0) {
                                    hitKey = fgetc(stdin);
                                    if (hitKey == 27)
                                        interruption = 1;
                                    else
                                        interruption = 0;
                                }
#endif
#endif
                                /// Monitors logs.
                                // logSystem.monitorLog();

                                /// Stop freeture according time execution
                                /// option.
                                if (executionTime != 0) {
                                    if (cptTime > executionTime) {
                                        break;
                                    }

                                    cptTime++;
                                }

                                /// Stop freeture if one of the thread is
                                /// stopped.
                                if (acqThread->getThreadStatus()) {
                                    break;
                                }

                                if (detThread != NULL) {
                                    if (!detThread->getRunStatus()) {
                                        logger->critical(
                                            "DetThread not running. "
                                            "Stopping the process ...");
                                        break;
                                    }
                                }
                            }
#ifdef LINUX
                            nonblock(0);
#endif
                        }
                    } catch (exception& e) {
                        spdlog::critical(e.what());
                        logger->critical(e.what());

                    } catch (const char* msg) {
                        spdlog::critical(msg);
                        logger->critical(msg);
                    }

                    if (acqThread != NULL) {
                        if (detThread != NULL) {
                            detThread->stopThread();
                            delete detThread;
                            detThread = nullptr;
                        }

                        acqThread->stopThread();
                        delete acqThread;
                    }

                    if (detThread != NULL) {
                        detThread->stopThread();
                        delete detThread;
                    }
                }

                break;
            case 4:
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%% MODE 4 : SINGLE ACQUISITION
                ///%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                {
                    spdlog::info("================================================");
                    spdlog::info("======== FREETURE - Single acquisition =========");
                    spdlog::info("================================================");

                    /// ------------------------------------------------------------------
                    /// --------------------- MANAGE PROGRAM OPTIONS
                    /// ---------------------
                    /// ------------------------------------------------------------------

                    // Path where to save files.
                    if (vm.count("savepath"))
                        savePath = vm["savepath"].as<string>();
                    // Acquisition pixel format.
                    if (vm.count("format"))
                        acqFormat = vm["format"].as<int>();
                    // Crop start x
                    if (vm.count("startx"))
                        startx = vm["startx"].as<int>();
                    // Crop start y
                    if (vm.count("starty"))
                        starty = vm["starty"].as<int>();
                    // Cam width size.
                    if (vm.count("width"))
                        acqWidth = vm["width"].as<int>();
                    // Cam height size
                    if (vm.count("height"))
                        acqHeight = vm["height"].as<int>();
                    // Cam id.
                    if (vm.count("id"))
                        devID = vm["id"].as<int>();
                    // Gain value.
                    if (vm.count("gain"))
                        gain = vm["gain"].as<int>();
                    // Exposure value.
                    if (vm.count("exposure"))
                        exp = vm["exposure"].as<double>();
                    // Filename.
                    if (vm.count("filename"))
                        fileName = vm["filename"].as<string>();

                    EParser<CamPixFmt> fmt;
                    string fstring = fmt.getStringEnum(
                        static_cast<CamPixFmt>(acqFormat));
                    if (fstring.empty())
                        throw ">> Pixel format specified not found.";

                    spdlog::info("------------------------------------------------");
                    spdlog::info("CAM ID    : {}", devID);
                    spdlog::info("FORMAT    : {}", fstring);
                    spdlog::info("GAIN      : {}", gain);
                    spdlog::info("EXPOSURE  : {}", exp);
                    if (acqWidth > 0 && acqHeight > 0)
                        spdlog::info("SIZE      : {},{}", acqWidth, acqHeight);
                    if (startx > 0 || starty > 0)
                        spdlog::info("START X,Y : {},{}", startx, starty);
                    spdlog::info("SAVE PATH : {}", savePath);
                    spdlog::info("FILENAME  : {}", fileName);
                    spdlog::info("------------------------------------------------");

                    /// ------------------------------------------------------------------
                    /// ----------------------- MANAGE FILE NAME
                    /// -------------------------
                    /// ------------------------------------------------------------------

                    int filenum = 0;
                    bool increment = false;
                    path p(savePath);

                    // Search previous captures in the directory.
                    for (directory_iterator file(p);
                         file != directory_iterator(); ++file) {
                        path curr(file->path());

                        if (is_regular_file(curr)) {
                            list<string> ch;
                            string fname = curr.filename().string();
                            Conversion::stringTok(ch, fname.c_str(), "-");

                            int i = 0;
                            int n = 0;

                            if (ch.front() == fileName && ch.size() == 2) {
                                list<string> ch_;
                                Conversion::stringTok(ch_, ch.back().c_str(), ".");

                                int nn = atoi(ch_.front().c_str());
                                if (nn >= filenum) {
                                    filenum = nn;
                                    increment = true;
                                }
                            }
                        }
                    }

                    if (increment)
                        filenum++;

                    /// ------------------------------------------------------------------
                    /// ---------------------- RUN SINGLE CAPTURE
                    /// ------------------------
                    /// ------------------------------------------------------------------
                    Frame frame;
                    frame.mExposure = exp;
                    frame.mGain = gain;
                    frame.mFormat = static_cast<CamPixFmt>(acqFormat);
                    frame.mStartX = startx;
                    frame.mStartY = starty;
                    frame.mHeight = acqHeight;
                    frame.mWidth = acqWidth;

                    auto* device = new Device();
                    device->listDevices(false);

                    if (!device->createCamera(devID, false)) {
                        delete device;
                        throw ">> Fail to create device.";
                    }

                    if (!device->runSingleCapture(frame)) {
                        delete device;
                        throw ">> Single capture failed.";
                    }

                    delete device;

                    spdlog::info(">> Single capture succeed.");

                    /// ------------------------------------------------------------------
                    /// ------------------- SAVE / DISPLAY CAPTURE
                    /// -----------------------
                    /// ------------------------------------------------------------------

                    if (frame.mImg.data) {
                        // Save the frame in BMP.
                        if (vm.count("bmp")) {
                            spdlog::info(">> Saving bmp file ...");

                            Mat temp1, newMat;
                            frame.mImg.copyTo(temp1);

                            if (frame.mFormat == MONO12) {
                                newMat = ImgProcessing::correctGammaOnMono12(temp1, 2.2);
                                Mat temp = Conversion::convertTo8UC1(newMat);
                            } else {
                                newMat = ImgProcessing::correctGammaOnMono8(temp1, 2.2);
                            }

                            SaveImg::saveBMP(
                                newMat,
                                savePath + fileName + "-" + Conversion::intToString(filenum, 6));
                            spdlog::info(">> Bmp saved : {}{}-{}.bmp",
                                savePath,
                                fileName,
                                Conversion::intToString(filenum, 6));
                        }
                    }
                }

                break;
            case 5:

                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%% MODE 5 : CLEAN LOGS
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

                // Simply remove all log directory contents.

                {
                    filesystem::path p(cfg.getLogParam().LOG_PATH);

                    if (filesystem::exists(p)) {
                        filesystem::remove_all(p);
                        spdlog::info("Clean log completed.");
                    } else {
                        spdlog::info("Log directory not found.");
                    }
                }

                break;
            case 6: {
            }

            break;

            default: {
                spdlog::info("MODE {} is not available. Correct modes are : ", mode);
                spdlog::info("[1] Check configuration file.");
                spdlog::info("[2] Run continous acquisition.");
                spdlog::info("[3] Run meteor detection.");
                spdlog::info("[4] Run single capture.");
                spdlog::info("[5] Clean logs.");
                spdlog::info("Execute freeture command to see options.");
            }

            break;
            }

            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%% PRINT HELP
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            ///%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        } else {
            spdlog::info(options.help());
        }

    } catch (exception& e) {
        spdlog::info(">> Error : {}", e.what());

    } catch (const char* msg) {
        spdlog::info(">> Error : {}", msg);
    }

    // cxxopts::notify(vm);

    return 0;
}

