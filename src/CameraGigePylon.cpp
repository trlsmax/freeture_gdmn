/*
                            CameraGigePylon.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2015 Yoan Audureau
*                       2018 Chiara Marmo
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
*   Last modified:      19/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraGigePylon.cpp
* \author  Yoan Audureau -- Chiara Marmo GEOPS-UPSUD
* \version 1.2
* \date    19/03/2018
* \brief   Use Pylon library to pilot GigE Cameras.
*/

#include "CameraGigePylon.h"
#include <spdlog/spdlog.h>

#ifdef USE_PYLON

CameraGigePylon::CameraGigePylon(){

    pCamera = NULL;
    pStreamGrabber = NULL;
    connectionStatus = false;
    mFrameCounter = 0;
    mExposureAvailable = true;
    mGainAvailable = true;
    mInputDeviceType = CAMERA;

    // Enumerate GigE cameras
    pTlFactory = &CTlFactory::GetInstance();
    pTl = pTlFactory->CreateTl(CBaslerGigECamera ::DeviceClass());
    pTl->EnumerateDevices(devices);
}

vector<pair<int,string>> CameraGigePylon::getCamerasList() {
    vector<pair<int,string>> camerasList;

    try {
        int id = 0;
        if(!devices.empty()) {
            DeviceInfoList_t::const_iterator it;
            for(it = devices.begin(); it != devices.end(); ++it ) {
                if(!devices.empty()){
                    if(devices[id].GetFullName().find_first_of("Basler")==0||devices[id].GetFullName().find_first_of("Prosilica")==0) {
                        pair<int,string> c;
                        c.first = id;
                        c.second = "NAME[" + devices[id].GetModelName() + "] S/N[" + devices[id].GetSerialNumber() + "] SDK[PYLON]";
                        camerasList.push_back(c);
                    }
                }
                id++;
            }
        }
    }catch (GenICam::GenericException &e){
        auto logger = spdlog::get("acq_logger");
        logger->error("An exception occured : {}", e.GetDescription());
        spdlog::error("An exception occured : {}", e.GetDescription());
    }

    return camerasList;
}

CameraGigePylon::~CameraGigePylon(void){
    if(pStreamGrabber != NULL){
        delete pStreamGrabber;
    }

    if(pCamera != NULL) {
        if(pCamera->IsOpen()) pCamera->Close();
        delete pCamera;
    }

    if(pTlFactory != NULL)
        pTlFactory->ReleaseTl(pTl);
}

bool CameraGigePylon::listCameras() {
    try {
        spdlog::info("------------ GIGE CAMERAS WITH PYLON -----------");
        int id = 0;
        DeviceInfoList_t::const_iterator it;

        for(it = devices.begin(); it != devices.end(); ++it ) {
            if(!devices.empty()){
                if(devices[id].GetFullName().find_first_of("Basler")==0||devices[id].GetFullName().find_first_of("Prosilica")==0) {
                    spdlog::info("-> ID[{}] NAME[{}] S/N[{}]", id, devices[id].GetModelName().c_str(), devices[id].GetSerialNumber().c_str());
                }
            }
            id++;
        }

        spdlog::info("------------------------------------------------");
    }catch (GenICam::GenericException &e){
        auto logger = spdlog::get("acq_logger");
        logger->error("An exception occured : {}", e.GetDescription());
        spdlog::error("An exception occured : {}", e.GetDescription());
        return false;
    }

    return true;
}

bool CameraGigePylon::createDevice(int id){
    try {
        if(!devices.empty()) {
            // Create a camera object
            if(id >= 0 && id < devices.size()){
                pCamera = new CBaslerGigECamera( pTl->CreateDevice((devices[id]) ));
            }else {
                return false;
            }

            // Open the camera object
            pCamera->Open();
            if(pCamera->IsOpen()) {
                auto logger = spdlog::get("acq_logger");
                logger->info("Success to open the device.");
            }

            return true;
        }

    }catch (GenICam::GenericException &e){
        spdlog::info(e.GetDescription());
        return false;
    }

    return false;
}

bool CameraGigePylon::getDeviceNameById(int id, string &device) {
    if(!devices.empty()) {
        spdlog::info(" Camera (ID:{}) detected ", id);
        spdlog::info(" Name :        {}", devices[id].GetModelName().c_str());
        return true;
    }

    return false;
}

bool CameraGigePylon::grabInitialization(){
    if(pCamera){
        if(pCamera->IsOpen()){
            try{
                //Disable acquisition start trigger if available
                {
                    GenApi::IEnumEntry* acquisitionStart = pCamera->TriggerSelector.GetEntry( TriggerSelector_AcquisitionStart);
                    if ( acquisitionStart && GenApi::IsAvailable( acquisitionStart)){
                        pCamera->TriggerSelector.SetValue( TriggerSelector_AcquisitionStart);
                        pCamera->TriggerMode.SetValue( TriggerMode_Off);
                    }
                }

                //Disable frame start trigger if available
                {
                    GenApi::IEnumEntry* frameStart = pCamera->TriggerSelector.GetEntry( TriggerSelector_FrameStart);
                    if ( frameStart && GenApi::IsAvailable( frameStart)){
                        pCamera->TriggerSelector.SetValue( TriggerSelector_FrameStart);
                        pCamera->TriggerMode.SetValue( TriggerMode_Off);
                    }
                }

                //Set acquisition mode
                pCamera->AcquisitionMode.SetValue(AcquisitionMode_Continuous);

                //Set exposure settings
                pCamera->ExposureMode.SetValue(ExposureMode_Timed);
                if (!pStreamGrabber){
                    pStreamGrabber = new (CBaslerGigECamera::StreamGrabber_t)(pCamera->GetStreamGrabber(0));
                }

                pStreamGrabber->Open();
                // Get the image buffer size
                const size_t ImageSize = (size_t)(pCamera->PayloadSize.GetValue());

                // We won't use image buffers greater than ImageSize
                pStreamGrabber->MaxBufferSize.SetValue(ImageSize);

                // We won't queue more than nbBuffers image buffers at a time
                pStreamGrabber->MaxNumBuffer.SetValue(nbBuffers);
                pStreamGrabber->PrepareGrab();

                for (int i = 0; i < nbBuffers; ++i){
                    //ppBuffers[i] = new unsigned char[ImageSize];
                    if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono8){
                        ppBuffersUC[i] = new uint8_t[ImageSize];
                        handles[i] = pStreamGrabber->RegisterBuffer(ppBuffersUC[i], ImageSize);
                    }

                    if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono12){
                        ppBuffersUS[i] = new uint16_t[ImageSize];
                        handles[i] = pStreamGrabber->RegisterBuffer(ppBuffersUS[i], ImageSize);
                    }

                    pStreamGrabber->QueueBuffer(handles[i], NULL);
                }

                return true;
            }catch (GenICam::GenericException &e){
                auto logger = spdlog::get("acq_logger");
                // Error handling.
                logger->error("An exception occurred. {}", e.GetDescription());
                spdlog::error("An exception occurred. {}", e.GetDescription());
                return false;
            }
        }
    }

    return false;
}

void CameraGigePylon::getAvailablePixelFormats() {
    vector<string> pixfmt;
    if(pCamera != NULL) {
        if(pCamera->IsOpen()) {
            INodeMap *nodemap = pCamera->GetNodeMap();
            // Access the PixelFormat enumeration type node.
            CEnumerationPtr pixelFormat( nodemap->GetNode( "PixelFormat"));
            // Check if the pixel format Mono8 is available.
            if(IsAvailable(pixelFormat->GetEntryByName( "Mono8")))
                pixfmt.push_back("MONO8");

            // Check if the pixel format Mono12 is available.
            if(IsAvailable(pixelFormat->GetEntryByName( "Mono12")))
                pixfmt.push_back("MONO12");

            spdlog::info(">> Available pixel formats :");
            EParser<CamPixFmt> fmt;
            for( int i = 0; i != pixfmt.size(); i++ ) {
                if(fmt.isEnumValue(pixfmt.at(i))) {
                    spdlog::info("- {} available --> ID : {}", pixfmt.at(i), fmt.parseEnum(pixfmt.at(i)));
                }
            }
        }
    }
}

void CameraGigePylon::grabCleanse(){
    if(pCamera){
        if(pCamera->IsOpen()){
            auto logger = spdlog::get("acq_logger");
            try{
                // Flush the input queue, grabbing may have failed
                logger->info("Flush the input queue.");
                if(pStreamGrabber != NULL) {
                    pStreamGrabber->CancelGrab();
                    // Consume all items from the output queue
                    GrabResult Result;
                        while (pStreamGrabber->GetWaitObject().Wait(0)){
                            pStreamGrabber->RetrieveResult(Result);
                            //if (Result.Status() == Canceled)
                                //logger->info("Got canceled buffer.");
                        }

                        // Deregister and free buffers
                        for(int i = 0; i < nbBuffers; ++i){
                            pStreamGrabber->DeregisterBuffer(handles[i]);
                            //logger->info("Deregister and free buffer n° ")<< i ;
                            if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono8){
                                delete [] ppBuffersUC[i];
                            }else if (pCamera->PixelFormat.GetValue() == PixelFormat_Mono12){
                                delete [] ppBuffersUS[i];
                            }
                        }

                    // Free all resources used for grabbing
                    pStreamGrabber->FinishGrab();
                    pStreamGrabber->Close();

                    if(pStreamGrabber != NULL){
                        delete pStreamGrabber;
                        pStreamGrabber = NULL;
                    }

                    if(pCamera != NULL) {
                        pCamera->Close();
                        delete pCamera;
                        pCamera = NULL;
                    }

                    if(pTlFactory != NULL)
                        pTlFactory->ReleaseTl(pTl);
                        pTlFactory = NULL;
                    }
            }catch (GenICam::GenericException &e){
                // Error handling.
                logger->error("An exception occurred. {}", e.GetDescription());
                spdlog::error("An exception occurred. {}", e.GetDescription());
            }
        }
    }
}

bool CameraGigePylon::acqStart(){
    if(pCamera!=NULL) {
        pCamera->AcquisitionStart.Execute();
        return true;
    }

    return false;
}

void CameraGigePylon::acqStop(){
    pCamera->AcquisitionStop.Execute();
}

bool CameraGigePylon::grabImage(Frame &newFrame){
    bool res = true;
    auto logger = spdlog::get("acq_logger");

    if(pStreamGrabber->GetWaitObject().Wait(3000)){
        // Get an item from the grabber's output queue
        if(!pStreamGrabber->RetrieveResult(result)){
            logger->error("Fail to retrieve an item from the output queue.");
            res = false;
        }

        CGrabResultPtr ptrGrabResult;
        if(result.Succeeded()){
            Mat newImg;
            if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono8){
                newImg = Mat(pCamera->Height.GetValue(), pCamera->Width.GetValue(), CV_8UC1, Scalar(0));
            }else if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono12){
                newImg = Mat(pCamera->Height.GetValue(), pCamera->Width.GetValue(), CV_16UC1, Scalar(0));
            }

            memcpy(newImg.ptr(), result.Buffer(), pCamera->PayloadSize.GetValue());
            newFrame = Frame(   newImg,
                                pCamera->GainRaw.GetValue(),
                                (double)pCamera->ExposureTimeAbs.GetValue(),
                                TimeDate::IsoExtendedStringNow());

            newFrame.mFps = pCamera->AcquisitionFrameRateAbs.GetValue();
            newFrame.mFrameNumber = mFrameCounter;
            mFrameCounter++;

            if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono8){
                newFrame.mFormat = MONO8;
                newFrame.mSaturatedValue = 255;
            }else if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono12){
                newFrame.mFormat = MONO12;
                newFrame.mSaturatedValue = 4095;
            }
        }else{
            logger->error("Fail to grab a frame : {}", result.GetErrorDescription());
            res = false;
        }

        // Requeue the buffer
        pStreamGrabber->QueueBuffer( result.Handle(), result.Context() );
    }else{
        logger->error("Fail to grab a frame (timeout) : {}", result.GetErrorDescription());
        res = false;
    }

    return res;
}

bool CameraGigePylon::setSize(int x, int y, int width, int height, bool customSize) {
    if(pCamera) {
        auto logger = spdlog::get("acq_logger");
        try{
            if (pCamera->IsAttached() && pCamera->IsOpen()){
                if(customSize) {
                    logger->info("Set custom size to : {}x{}", width, height);
                    pCamera->Width.SetValue(width);
                    pCamera->Height.SetValue(height);
                // Default is maximum size
                }else {
                    logger->info("Set size to : {}x{}", pCamera->Width.GetMax(), pCamera->Height.GetMax());
                    pCamera->Width.SetValue(pCamera->Width.GetMax());
                    pCamera->Height.SetValue(pCamera->Height.GetMax());
                }

                return true;
            }else{
                logger->error("Can't access size image. Camera not opened or not attached.");
            }
        }catch (GenICam::GenericException &e){
            // Error handling
            logger->error("An exception occurred. {}", e.GetDescription());
        }
    }

    return false;
}

bool CameraGigePylon::grabSingleImage(Frame &frame, int camID){
    auto logger = spdlog::get("acq_logger");
    try {
        auto logger = spdlog::get("acq_logger");
        // Enumerate GigE cameras
        pTlFactory = &CTlFactory::GetInstance();
        pTl = pTlFactory->CreateTl(CBaslerGigECamera ::DeviceClass());

        if (((camID + 1 ) > pTl->EnumerateDevices(devices)) || camID < 0){
            throw "Camera ID not correct. Can't be found.";
        }else {
            spdlog::info(">> Camera (ID:{}) fouind. ", camID);
        }

        // Create an instant camera object with the correct id camera device.
        CInstantCamera camera( CTlFactory::GetInstance().CreateDevice(devices[camID].GetFullName()));

        INodeMap& nodemap = camera.GetNodeMap();
        // Open the camera for accessing the parameters.
        camera.Open();

        CIntegerPtr width(nodemap.GetNode("Width"));
        CIntegerPtr height(nodemap.GetNode("Height"));
        if(frame.mWidth > 0 && frame.mHeight > 0) {
            width->SetValue(frame.mWidth);
            height->SetValue(frame.mHeight);
        }else{
            // Set width and height to the maximum sensor's size.
            width->SetValue(width->GetMax());
            height->SetValue(height->GetMax());
        }

        // Set pixel format.
        // Access the PixelFormat enumeration type node.
        CEnumerationPtr pixelFormat(nodemap.GetNode("PixelFormat"));

        if(frame.mFormat == MONO8) {
            if(IsAvailable(pixelFormat->GetEntryByName("Mono8"))){
                pixelFormat->FromString("Mono8");
        }else{
            spdlog::info(">> Fail to set pixel format to MONO_8");
            return false;
        }

        }else if(frame.mFormat == MONO12){
            if(IsAvailable(pixelFormat->GetEntryByName("Mono12"))){
                pixelFormat->FromString("Mono12");
            }else{
                spdlog::info(">> Fail to set pixel format to MONO_12");
                return false;
            }
        }else{
            spdlog::info(">> No depth specified for the frame container.");
            return false;
        }

        CEnumerationPtr exposureAuto( nodemap.GetNode( "ExposureAuto"));
        if ( IsWritable( exposureAuto)){
            exposureAuto->FromString("Off");
            spdlog::info(">> Exposure auto disabled.");
        }

        // Set exposure.
        CIntegerPtr ExposureTimeRaw(nodemap.GetNode("ExposureTimeRaw"));
        if(ExposureTimeRaw.IsValid()) {
            if(frame.mExposure >= ExposureTimeRaw->GetMin() && frame.mExposure <= ExposureTimeRaw->GetMax()) {
                ExposureTimeRaw->SetValue(frame.mExposure);
            }else {
                ExposureTimeRaw->SetValue(ExposureTimeRaw->GetMin());
                spdlog::info(">> Exposure has been setted with the minimum available value.");
                spdlog::info(">> The available exposure range is [{}-{}] (us)", ExposureTimeRaw->GetMin(), ExposureTimeRaw->GetMax());
            }
        }else {
            spdlog::info(">> Fail to set exposure value.");
            return false;
        }

        // Disable auto gain.
        CEnumerationPtr gainAuto( nodemap.GetNode( "GainAuto"));
        if ( IsWritable( gainAuto)){
            gainAuto->FromString("Off");
            spdlog::info(">> Gain auto disabled.");
        }

        // Set gain.
        // Access the GainRaw integer type node. This node is available for Firewire and GigE Devices.
        CIntegerPtr gainRaw(nodemap.GetNode("GainRaw"));
        if(gainRaw.IsValid()) {
            if(frame.mGain >= gainRaw->GetMin() && frame.mGain <= gainRaw->GetMax()) {
                gainRaw->SetValue(frame.mGain);
            }else {
                gainRaw->SetValue(gainRaw->GetMin());
                spdlog::info(">> Gain has been setted to the minimum available value.");
                spdlog::info(">> The available gain range is [{}-{}]", gainRaw->GetMin(), gainRaw->GetMax());
            }
        }

        camera.Close();

        // This smart pointer will receive the grab result data.
        CGrabResultPtr ptrGrabResult;
        spdlog::info(">> Acquisition in progress... (Please wait)");

        int timeout = 1000 + frame.mExposure/1000;
        camera.GrabOne(timeout , ptrGrabResult);

        Mat newImg;
        // Image grabbed successfully ?
        if(ptrGrabResult->GrabSucceeded()){
            //Timestamping.
            string acqDateInMicrosec = TimeDate::IsoExtendedStringNow();

            frame.mDate = TimeDate::splitIsoExtendedDate(to_iso_extended_string(time));
            frame.mFps = 0;

            if(ptrGrabResult->GetPixelType()== PixelType_Mono8) {
                newImg = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC1, Scalar(0));
            }else if(ptrGrabResult->GetPixelType()== PixelType_Mono12) {
                newImg = Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_16UC1, Scalar(0));
            }
            memcpy(newImg.ptr(), ptrGrabResult->GetBuffer(), ptrGrabResult->GetPayloadSize());
            newImg.copyTo(frame.mImg);

            return true;
        }
    }catch(GenICam::GenericException &e) {
        logger->error(e.GetDescription());
    }catch(exception& e) {
        logger->error(e.what());
    }catch(const char * msg) {
        logger->error(msg);
        spdlog::error(msg);
    }

    if(pTlFactory != NULL) {
        pTlFactory->ReleaseTl(pTl);
        pTlFactory = NULL;
    }

    return false;
}

void CameraGigePylon::getExposureBounds(double &eMin, double &eMax){
    INodeMap *nodemap = pCamera->GetNodeMap();
    CIntegerPtr exposureTimeRaw(nodemap->GetNode("ExposureTimeRaw"));

    if(exposureTimeRaw.IsValid()) {
            eMin = exposureTimeRaw->GetMin();
            eMax = exposureTimeRaw->GetMax();
    }
}

void CameraGigePylon::getGainBounds(int &gMin, int &gMax){
    INodeMap *nodemap = pCamera->GetNodeMap();
    CIntegerPtr gainRaw(nodemap->GetNode("GainRaw"));

    if(gainRaw.IsValid()) {
            gMin = gainRaw->GetMin();
            gMax = gainRaw->GetMax();
    }
}

bool CameraGigePylon::getPixelFormat(CamPixFmt &format){
    if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono8){
        format = MONO8;
        return true;
    }else if(pCamera->PixelFormat.GetValue() == PixelFormat_Mono12){
        format = MONO12;
        return true;
    }

    return false;
}

bool CameraGigePylon::getFrameSize(int &w , int &h) {
    if(pCamera){
        auto logger = spdlog::get("acq_logger");
        try{
            if (pCamera->IsAttached() && pCamera->IsOpen()){
                w = pCamera->Width.GetValue();
                h = pCamera->Height.GetValue();
                return true;
            }else{
                logger->error("Can't access width image. Camera not opened or not attached.");
            }
        }catch (GenICam::GenericException &e){
            // Error handling
            logger->error("An exception occurred. {}", e.GetDescription());
        }
    }

    return false;
}

bool CameraGigePylon::getFPS(double &value) {
    if(pCamera!=NULL) {
        value = pCamera->ResultingFrameRateAbs.GetValue();
        return true;
    }

    value = 0;
    return false;
}

string CameraGigePylon::getModelName(){
 return "";
}

bool CameraGigePylon::setExposureTime(double exposition) {
    if(pCamera){
        try{
            if( pCamera->IsAttached() && pCamera->IsOpen() ){
                // Check whether auto exposure is available
                if (IsWritable( pCamera->ExposureAuto)){
                    // Disable auto exposure.
                    cout << "Disable ExposureAuto." << endl;
                    pCamera->ExposureAuto.SetValue(ExposureAuto_Off);
                }

                pCamera->ExposureTimeAbs = exposition;
            }else{
                 spdlog::info("Camera not opened or not attached");
            }

            return true;
        }catch (GenICam::GenericException &e){
            // Error handling
            auto logger = spdlog::get("acq_logger");
            logger->error("An exception occurred. {}", e.GetDescription());
            spdlog::error("An exception occurred. {}", e.GetDescription());
            return false;
        }
    }

    return false;
}

bool CameraGigePylon::setGain(int gain){
    if(pCamera){
        auto logger = spdlog::get("acq_logger");
        try{
            if( pCamera->IsAttached() && pCamera->IsOpen() ){
                // Check whether auto exposure is available
                if (IsWritable( pCamera->GainAuto)){
                    // Disable auto exposure.
                    spdlog::info("Disable GainAuto.");
                    pCamera->GainAuto.SetValue(GainAuto_Off);
                }

                pCamera->GainRaw = gain;
            }else{
                logger->error("Camera not opened or not attached");
            }

            return true;
        }catch (GenICam::GenericException &e){
            // Error handling
            logger->error("An exception occurred. {}", e.GetDescription());
            spdlog::error("An exception occurred. {}", e.GetDescription());
            return false;
        }
    }

    return false;
}

bool CameraGigePylon::setFPS(double fps){
    pCamera->AcquisitionFrameRateAbs = fps;
    return true;
}

bool CameraGigePylon::setPixelFormat(CamPixFmt format){
    Basler_GigECamera::PixelFormatEnums fpix;
    auto logger = spdlog::get("acq_logger");

    if(format == MONO8 ){
        fpix = PixelFormat_Mono8;
    }
    else if (format == MONO12 ){
        fpix = PixelFormat_Mono12;
    }

    if (pCamera){
        try{
            if(pCamera->IsAttached() && pCamera->IsOpen()){
                pCamera->PixelFormat.SetValue(fpix);
            }else{
               logger->error("Camera not opened or not attached");
            }
        }
        catch (GenICam::GenericException &e){
            // Error handling
            logger->error("An exception occurred. {}", e.GetDescription());
            spdlog::error("An exception occurred. {}", e.GetDescription());
        }

        return true;
    }

    return false;
}

double CameraGigePylon::getExposureTime(){
    if(pCamera!=0)
         return pCamera->ExposureTimeAbs.GetValue();
    else
        return 0;
}

/*
int CameraGigePylon::getGain(){
    return (int)(pCamera->GainRaw.GetValue());

}*/


#endif
