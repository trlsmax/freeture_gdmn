/*
                        CameraUsbAravis.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau
*                               FRIPON-GEOPS-UPSUD-CNRS
*                   (C) 2018-2019 Martin Cupak -- DFN/GFO
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
*   Last modified:      14/08/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraUsbAravis.cpp
* \author  Yoan Audureau -- FRIPON-GEOPS-UPSUD
* \version 1.0
* \date    07/08/2018
* \brief   Use Aravis library to pilot USB Cameras.
*          https://wiki.gnome.org/action/show/Projects/Aravis?action=show&redirect=Aravis
*/

#include "CameraUsbAravis.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <date/date.h>

#if 0

CameraUsbAravis::CameraUsbAravis(bool shift) :
        camera(NULL), mStartX(0), mStartY(0), mWidth(0), mHeight(0), fps(0), gainMin(0.0), gainMax(0.0),
        payload(0), exposureMin(0), exposureMax(0), gain(0), exp(0), nbCompletedBuffers(0),
        nbFailures(0), nbUnderruns(0), frameCounter(0), shiftBitsImage(shift), stream(NULL)
{
    mExposureAvailable = true;
    mGainAvailable = true;
    mInputDeviceType = CAMERA;
}

CameraUsbAravis::CameraUsbAravis() :
        camera(NULL), mStartX(0), mStartY(0), mWidth(0), mHeight(0), fps(0), gainMin(0.0), gainMax(0.0),
        payload(0), exposureMin(0), exposureMax(0), gain(0), exp(0), nbCompletedBuffers(0),
        nbFailures(0), nbUnderruns(0), frameCounter(0), shiftBitsImage(false), stream(NULL)
{
    mExposureAvailable = true;
    mGainAvailable = true;
    mInputDeviceType = CAMERA;
}

CameraUsbAravis::~CameraUsbAravis()
{
    if (stream != NULL)
        g_object_unref(stream);

    if (camera != NULL)
        g_object_unref(camera);
}

vector<pair<int, string>> CameraUsbAravis::getCamerasList()
{
    vector<pair<int, string>> camerasList;
    ArvInterface *interface;
    //arv_update_device_list();
    int ni = arv_get_n_interfaces();

    for (int j = 0; j < ni; j++) {
        const char *name = arv_get_interface_id(j);
        if (strcmp(name, "USB3Vision") == 0) {
            interface = arv_uv_interface_get_instance();
            arv_interface_update_device_list(interface);
            //int nb = arv_get_n_devices();

            int nb = arv_interface_get_n_devices(interface);
            for (int i = 0; i < nb; i++) {
                pair<int, string> c;
                c.first = i;
                //const char* str = arv_get_device_id(i);
                const char *str = arv_interface_get_device_id(interface, i);
                //const char* addr = arv_interface_get_device_address(interface,i);
                const char *phyid = arv_interface_get_device_physical_id(interface, i);
                std::string s = str;
                c.second = "NAME[" + s + "] SDK[ARAVIS_USB] USB bus id: " + phyid;
                camerasList.push_back(c);
            }
        }
    }

    return camerasList;
}

bool CameraUsbAravis::listCameras()
{
    ArvInterface *interface;
    //arv_update_device_list();
    int ni = arv_get_n_interfaces();
    cout << endl << "------------ USB CAMERAS WITH ARAVIS ----------" << endl << endl;

    for (int j = 0; j < ni; j++) {
        interface = arv_uv_interface_get_instance();
        arv_interface_update_device_list(interface);
        //int nb = arv_get_n_devices();

        int nb = arv_interface_get_n_devices(interface);
        for (int i = 0; i < nb; i++) {
            cout << "-> [" << i << "] " << arv_interface_get_device_id(interface, i) << endl;
            //cout << "-> [" << i << "] " << arv_get_device_id(i)<< endl;
        }

        if (nb == 0) {
            cout << "-> No cameras detected..." << endl;
            return false;
        }
    }
    cout << endl << "------------------------------------------------" << endl << endl;

    return true;
}

bool CameraUsbAravis::createDevice(int id)
{
    string deviceName;
    GError **error = NULL;
    auto logger = spdlog::get("acq_logger");

    if (!getDeviceNameById(id, deviceName))
        return false;

    camera = arv_camera_new(deviceName.c_str(), error);

    if (camera == NULL) {
        logger->error("Fail to connect the camera.");
        return false;
    }

    return true;
}

bool CameraUsbAravis::setSize(int startx, int starty, int width, int height, bool customSize)
{
    auto logger = spdlog::get("acq_logger");
    GError **error = NULL;
    if (customSize) {
        arv_camera_set_region(camera, startx, starty, width, height, error);
        arv_camera_get_region(camera, &mStartX, &mStartY, &mWidth, &mHeight, error);
        logger->info("Camera region size : {}x{}", mWidth, mHeight);
        if (arv_device_get_feature(arv_camera_get_device(camera), "OffsetX")) {
            logger->info("Starting from : {}x{}", mStartX, mStartY);
        } else {
            logger->warn("OffsetX, OffsetY are not available: cannot set offset.");
        }

        // Default is maximum size
    } else {
        int sensor_width, sensor_height;
        arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);
        logger->info("Camera sensor size : {}x{}", sensor_width, sensor_height);
        arv_camera_set_region(camera, 0, 0, sensor_width, sensor_height, error);
        arv_camera_get_region(camera, NULL, NULL, &mWidth, &mHeight, error);
    }

    return true;
}

bool CameraUsbAravis::getDeviceNameById(int id, string &device)
{
    arv_update_device_list();
    int n_devices = arv_get_n_devices();

    for (int i = 0; i < n_devices; i++) {
        if (id == i) {
            device = arv_get_device_id(i);
            return true;
        }
    }

    auto logger = spdlog::get("acq_logger");
    logger->error("Fail to retrieve camera with this ID.");
    return false;
}

bool CameraUsbAravis::grabInitialization()
{
    auto logger = spdlog::get("acq_logger");
    logger->info("CameraUsbAravis::grabInitialization()");
    //saveGenicamXml("./");

    guint bandwidth;
    gboolean isUV, isGV, isCam;
    const char *firmwareVer;
    GError **error = NULL;

    // MC: need to get ArvDevice to be get more complete access to camera features
    ArvDevice *ad;
    ad = arv_camera_get_device(camera);

    bandwidth = arv_device_get_integer_feature_value(ad, "DeviceLinkThroughputLimit", error);
    logger->info("arv device Camera USB bandwidth limit : {}", bandwidth);
    //      std::cout << "arv device Camera USB bandwidth limit : " << bandwidth;

    firmwareVer = arv_device_get_string_feature_value(ad, "DeviceFirmwareVersion", error);
    logger->info("arv device Camera Firmware Version : {}", firmwareVer);

    // MC:
    //double pwrVoltage = arv_device_get_float_feature_value( ad, "PowerSupplyVoltage" );
    //logger->info("arv device PowerSupplyVoltage : ") << pwrVoltage;
    //double pwrCurrent = arv_device_get_float_feature_value( ad, "PowerSupplyCurrent" );
    //logger->info("arv device PowerSupplyCurrent : ") << pwrVoltage;
    //gint64 upTime = arv_device_get_integer_feature_value( ad, "DeviceUptime" );
    //logger->info("arv device DeviceUptime : ") << upTime;

    frameCounter = 0;
    payload = arv_camera_get_payload(camera, error);
    logger->info("Camera payload : {}", payload);

#define FLIR_FRAME_RATE_REGISTER_OFFSET 0x083C
#define FLIR_EXTENDED_SHUTTER_REGISTER_OFFSET 0x1028
    guint32 registerFrameRate, registerExtendedShutter;
    arv_device_read_register(ad, FLIR_FRAME_RATE_REGISTER_OFFSET, &registerFrameRate, NULL);
    arv_device_read_register(ad, FLIR_EXTENDED_SHUTTER_REGISTER_OFFSET, &registerExtendedShutter, NULL);

    logger->info("FLIR_FRAME_RATE_REGISTER_OFFSET       0x083C : {}", registerFrameRate);
    logger->info("FLIR_EXTENDED_SHUTTER_REGISTER_OFFSET 0x1028 : {}", registerExtendedShutter);

    pixFormat = arv_camera_get_pixel_format(camera, error);

    arv_camera_get_gain_bounds(camera, &gainMin, &gainMax, error);
    arv_camera_get_exposure_time_bounds(camera, &exposureMin, &exposureMax, error);
    logger->info("Camera exposure bound min : {}", exposureMin);
    logger->info("Camera exposure bound max : {}", exposureMax);

    logger->info("Camera gain bound min : {}", gainMin);
    logger->info("Camera gain bound max : {}", gainMax);

    ArvAuto gainAutoMode;
    gainAutoMode = arv_camera_get_gain_auto(camera, error);
    const char *gainAutoModeAtring;
    gainAutoModeAtring = arv_device_get_string_feature_value(ad, "GainAuto", error);
    logger->info("GainAuto : {} ({})", gainAutoModeAtring, gainAutoMode);
    gainAutoMode = arv_camera_get_gain_auto(camera, error);
    logger->info("arv_camera_get_gain_auto : {}", gainAutoMode);

    // added by MC
    // AcquisitionMode - FLIR sprcific
    bool ackFREnabled = arv_device_get_boolean_feature_value(ad, "AcquisitionFrameRateEnabled", error);
    logger->info("Camera Frame rate control enabled (AcquisitionFrameRateEnabled) : {}", ackFREnabled);

    gint64 *values;
    guint n_values;
    const char **valuesAsStrings;

    values = arv_device_dup_available_enumeration_feature_values(ad, "AcquisitionMode", &n_values, error);
    valuesAsStrings = arv_device_dup_available_enumeration_feature_values_as_strings(ad, "AcquisitionMode", &n_values,
                                                                                     error);
    logger->info("AcquisitionModes :");
    for (int i = 0; i < n_values; i++) {
        logger->info("  [{}, {}]", values[i], valuesAsStrings[i]);
    }

    ArvAcquisitionMode aaMode;
    aaMode = arv_camera_get_acquisition_mode(camera, error);
    logger->info("Camera ARV Acquisistion Mode : {}", aaMode);

    // MC: ExposureAuto needs to be off - otherwise it is not possibleto set ExposureTime
    values = arv_device_dup_available_enumeration_feature_values(ad, "ExposureAuto", &n_values, error);
    valuesAsStrings = arv_device_dup_available_enumeration_feature_values_as_strings(ad, "ExposureAuto", &n_values,
                                                                                     error);
    logger->info("ExposureAuto :");
    for (int i = 0; i < n_values; i++) {
        logger->info("  [{}, {}]", values[i], valuesAsStrings[i]);
    }
    ArvAuto expAuto;
    expAuto = (ArvAuto) arv_device_get_integer_feature_value(ad, "ExposureAuto", error);
    logger->info("Camera ExposureAuto Mode : {}", expAuto);
    expAuto = arv_camera_get_exposure_time_auto(camera, error);
    logger->info("arv_camera_get_exposure_time_auto Mode : {}", expAuto);

    // MC: VideoMode
    values = arv_device_dup_available_enumeration_feature_values(ad, "VideoMode", &n_values, error);
    valuesAsStrings = arv_device_dup_available_enumeration_feature_values_as_strings(ad, "VideoMode", &n_values, error);
    logger->info("VideoMode :");
    for (int i = 0; i < n_values; i++) {
        logger->info("  [{}, {}]", values[i], valuesAsStrings[i]);
    }
    gint64 videoMode;
    videoMode = arv_device_get_integer_feature_value(ad, "VideoMode", error);
    logger->info("Original Camera VideoMode : {}", videoMode);
    arv_device_set_integer_feature_value(ad, "VideoMode", 7, error);
    videoMode = arv_device_get_integer_feature_value(ad, "VideoMode", error);
    logger->info("Camera VideoMode : {}", videoMode);

    double frameRateMin, frameRateMax;
    // this ain't work arv_camera_get_frame_rate_bounds(camera, &frameRateMin, &frameRateMax);
    arv_device_get_float_feature_bounds(ad, "AcquisitionFrameRate", &frameRateMin, &frameRateMax, error);

    // arv_camera_set_frame_rate(camera, 32); // MC: here is hardcoded (was 30 FPS)
    //                                           but setFPS() is called later from AcqThread::prepareAcquisitionOnDevice()
    // MC TODO: remove this hard coded constant. Either use this->fps or value from config file
    bool retVal = setFPS(30);

    logger->info("Camera frame rate bounds min : {} max : {}", frameRateMin, frameRateMax);

    arv_camera_get_exposure_time_bounds(camera, &exposureMin, &exposureMax, error);
    logger->info("Camera exposure bound min : {}", exposureMin);
    logger->info("Camera exposure bound max : {}", exposureMax);

    capsString = arv_pixel_format_to_gst_caps_string(pixFormat);
    logger->info("Camera format : {}", capsString);

    gain = arv_camera_get_gain(camera, error);
    logger->info("Camera gain : {}", gain);

    exp = arv_camera_get_exposure_time(camera, error);
    logger->info("Camera exposure : {}", exp);

    double expTimeAbs = arv_device_get_float_feature_value(ad, "ExposureTimeAbs", error);
    logger->info("ExpTimeAbs      : {}", expTimeAbs);

    cout << endl;

    cout << "DEVICE SELECTED : " << arv_camera_get_device_id(camera, error) << endl;
    cout << "DEVICE NAME     : " << arv_camera_get_model_name(camera, error) << endl;
    cout << "DEVICE VENDOR   : " << arv_camera_get_vendor_name(camera, error) << endl;
    cout << "PAYLOAD         : " << payload << endl;
    cout << "Start X         : " << mStartX << endl
         << "Start Y         : " << mStartY << endl;
    cout << "Width           : " << mWidth << endl
         << "Height          : " << mHeight << endl;
    cout << "Exp Range       : [" << exposureMin << " - " << exposureMax << "]" << endl;
    cout << "Exp             : " << exp << endl;
    cout << "Gain Range      : [" << gainMin << " - " << gainMax << "]" << endl;
    cout << "Gain            : " << gain << endl;
    cout << "Fps             : " << fps << endl;
    cout << "Type            : " << capsString << endl;

    cout << endl;


    // Create a new stream object. Open stream on Camera.
    stream = arv_camera_create_stream(camera, NULL, NULL, error);

    if (stream == NULL) {
        logger->critical("Fail to create stream with arv_camera_create_stream()");
        return false;
    }

    // Push 50 buffer in the stream input buffer queue.
    for (int i = 0; i < 50; i++)
        arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));

    return true;
}

void CameraUsbAravis::grabCleanse()
{}

bool CameraUsbAravis::acqStart()
{
    auto logger = spdlog::get("acq_logger");
    GError **error = NULL;
    logger->info("Set camera to CONTINUOUS MODE");
    arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, error);

    logger->info("Set camera TriggerMode to Off");
    arv_device_set_string_feature_value(arv_camera_get_device(camera), "TriggerMode", "Off", error);

    logger->info("Start acquisition on camera");
    arv_camera_start_acquisition(camera, error);

    return true;
}

void CameraUsbAravis::acqStop()
{
    auto logger = spdlog::get("acq_logger");
    GError **error = NULL;
    arv_stream_get_statistics(stream, &nbCompletedBuffers, &nbFailures, &nbUnderruns);

    //cout << "Completed buffers = " << (unsigned long long) nbCompletedBuffers   << endl;
    //cout << "Failures          = " << (unsigned long long) nbFailures           << endl;
    //cout << "Underruns         = " << (unsigned long long) nbUnderruns          << endl;

    logger->info("Completed buffers = {}", (unsigned long long) nbCompletedBuffers);
    logger->info("Failures          = {}", (unsigned long long) nbFailures);
    logger->info("Underruns         = {}", (unsigned long long) nbUnderruns);

    logger->info("Stopping acquisition...");
    arv_camera_stop_acquisition(camera, error);
    logger->info("Acquisition stopped.");

    logger->info("Unreferencing stream.");
    g_object_unref(stream);
    stream = NULL;

    logger->info("Unreferencing camera.");
    g_object_unref(camera);
    camera = NULL;

}

bool CameraUsbAravis::grabImage(Frame &newFrame)
{
    auto logger = spdlog::get("acq_logger");
    ArvBuffer *arv_buffer;
    //exp = arv_camera_get_exposure_time(camera);
    arv_buffer = arv_stream_timeout_pop_buffer(stream, 12000000); //us; 12s
    char *buffer_data;
    size_t buffer_size;

    if (arv_buffer == NULL) {
        throw runtime_error("arv_buffer is NULL");
        return false;
    } else {
        try {
            if (arv_buffer_get_status(arv_buffer) == ARV_BUFFER_STATUS_SUCCESS) {
                logger->info("Success to grab a frame.");
                buffer_data = (char *) arv_buffer_get_data(arv_buffer, &buffer_size);

                //Timestamping.
                string acquisitionDate = TimeDate::localDateTime(std::chrono::system_clock::now(), "%Y:%m:%d:%H:%M:%S");
                logger->info("Date : {}", acquisitionDate);
                acquisitionDate = TimeDate::IsoExtendedStringNow();
                //logger->info("Date : ") << acqDateInMicrosec;

                Mat image;
                CamPixFmt imgDepth = MONO8;
                int saturateVal = 0;

                if (pixFormat == ARV_PIXEL_FORMAT_MONO_8) {
                    logger->info("Creating Mat 8 bits ...");
                    // MC: following Mat constructor does not copy data, only passes pounter to data (buffer_data)
                    // it's  a fast constructor  ;-)
                    image = Mat(mHeight, mWidth, CV_8UC1, buffer_data);
                    if (buffer_size != (mHeight * mWidth * sizeof(unsigned char))) {
                        std::cerr << "Creating Mat 8 bits ... "
                                  << "WARNING: buffer_size=" << buffer_size << "  mHeight=" << mHeight << "  mWidth="
                                  << mWidth << std::endl;
                    }
                    if (buffer_size < (mHeight * mWidth * sizeof(unsigned char))) {
                        std::cerr
                                << "ERROR: CameraUsbAravis::grabImage(Frame &): arv_buffer_get_data returned smaller buffer size than mHeight*mWidth!"
                                << std::endl;
                        exit(-1);
                    }
                    imgDepth = MONO8;
                    saturateVal = 255;
                } else if (pixFormat == ARV_PIXEL_FORMAT_MONO_12) {
                    logger->info("Creating Mat 16 bits ...");
                    image = Mat(mHeight, mWidth, CV_16UC1, buffer_data);
                    imgDepth = MONO12;
                    saturateVal = 4095;

                    //double t3 = (double)getTickCount();

                    if (shiftBitsImage) {
                        logger->info("Shifting bits ...");
                        unsigned short *p;
                        for (int i = 0; i < image.rows; i++) {
                            p = image.ptr<unsigned short>(i);
                            for (int j = 0; j < image.cols; j++)
                                p[j] = p[j] >> 4;
                        }

                        logger->info("Bits shifted.");
                    }
                    //t3 = (((double)getTickCount() - t3)/getTickFrequency())*1000;
                    //cout << "> Time shift : " << t3 << endl;
                } else if (pixFormat == ARV_PIXEL_FORMAT_MONO_16) {
                    logger->info("Creating Mat 16 bits ...");
                    image = Mat(mHeight, mWidth, CV_16UC1, buffer_data);
                    if (buffer_size != (mHeight * mWidth * sizeof(unsigned short))) {
                        std::cerr << "Creating Mat 16 bits ... "
                                  << "WARNING: buffer_size=" << buffer_size << "  mHeight=" << mHeight << "  mWidth="
                                  << mWidth << std::endl;
                    }
                    if (buffer_size < (mHeight * mWidth * sizeof(unsigned short))) {
                        std::cerr
                                << "ERROR: CameraUsbAravis::grabImage(Frame &): arv_buffer_get_data returned smaller buffer size than mHeight*mWidth!"
                                << std::endl;
                        exit(-1);
                    }
                    imgDepth = MONO16;
                    saturateVal = 4095;
                }
                logger->info("Creating frame object ...");
                newFrame = Frame(image, gain, exp, acquisitionDate);
                logger->info("Setting date of frame ...");
                //newFrame.setAcqDateMicro(acqDateInMicrosec);
                logger->info("Setting fps of frame ...");
                newFrame.mFps = fps;
                newFrame.mFormat = imgDepth;
                logger->info("Setting saturated value of frame ...");
                newFrame.mSaturatedValue = saturateVal;
                newFrame.mFrameNumber = frameCounter;
                frameCounter++;

                logger->info("Re-pushing arv buffer in stream ...");
                arv_stream_push_buffer(stream, arv_buffer);

                return true;
            } else {
                switch (arv_buffer_get_status(arv_buffer)) {
                    case 0 :
                        cout << "ARV_BUFFER_STATUS_SUCCESS : the buffer contains a valid image" << endl;
                        break;
                    case 1 :
                        cout << "ARV_BUFFER_STATUS_CLEARED: the buffer is cleared" << endl;
                        break;
                    case 2 :
                        cout << "ARV_BUFFER_STATUS_TIMEOUT: timeout was reached before all packets are received"
                             << endl;
                        break;
                    case 3 :
                        cout << "ARV_BUFFER_STATUS_MISSING_PACKETS: stream has missing packets" << endl;
                        break;
                    case 4 :
                        cout << "ARV_BUFFER_STATUS_WRONG_PACKET_ID: stream has packet with wrong id" << endl;
                        break;
                    case 5 :
                        cout
                                << "ARV_BUFFER_STATUS_SIZE_MISMATCH: the received image didn't fit in the buffer data space"
                                << endl;
                        break;
                    case 6 :
                        cout << "ARV_BUFFER_STATUS_FILLING: the image is currently being filled" << endl;
                        break;
                    case 7 :
                        cout << "ARV_BUFFER_STATUS_ABORTED: the filling was aborted before completion" << endl;
                        break;

                }
                arv_stream_push_buffer(stream, arv_buffer);

                return false;
            }
        } catch (exception &e) {
            spdlog::critical(e.what());
            logger->critical(e.what());
            return false;
        }
    }
}


bool CameraUsbAravis::grabSingleImage(Frame &frame, int camID)
{
    bool res = false;
    GError **error = NULL;
    auto logger = spdlog::get("acq_logger");

    if (!createDevice(camID))
        return false;

    // MC: need to get ArvDevice to be get more complete access to camera features
    ArvDevice *ad;
    ad = arv_camera_get_device(camera);

    // MC: NO! setting frame rate seables frame rate! this->setFPS(1);
    // MC: disable frame rate to be able to set longer than 1/frame rate exposure time
    //     Now the actual frame rate does not control the exposure time limits and
    //     it is possible to set longer (extended) exposure time
    //        if (camera->priv->has_acquisition_frame_rate_enabled)
    arv_device_set_integer_feature_value(ad, "AcquisitionFrameRateEnabled", false, error);
    //		else
    //			arv_device_set_boolean_feature_value (camera->priv->device, "AcquisitionFrameRateEnable", 1);

    if (!setPixelFormat(frame.mFormat))
        return false;

    if (!setExposureTime(frame.mExposure))
        return false;
    // read back gain from camera HW so thet a real value gets written to FITS file
    exp = arv_camera_get_exposure_time(camera, error);
    frame.mExposure = exp;

    if (!setGain(frame.mGain))
        return false;
    // read back gain from camera HW so thet a real value gets written to FITS file
    gain = arv_camera_get_gain(camera, error);
    frame.mGain = gain;

    if (frame.mWidth > 0 && frame.mHeight > 0) {
        setSize(frame.mStartX, frame.mStartY, frame.mWidth, frame.mHeight, 1);
    } else {
        int sensor_width, sensor_height;
        arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);

        // Use maximum sensor size.
        arv_camera_set_region(camera, 0, 0, sensor_width, sensor_height, error);
        arv_camera_get_region(camera, NULL, NULL, &mWidth, &mHeight, error);
    }

    payload = arv_camera_get_payload(camera, error);
    pixFormat = arv_camera_get_pixel_format(camera, error);
    arv_camera_get_exposure_time_bounds(camera, &exposureMin, &exposureMax, error);
    arv_camera_get_gain_bounds(camera, &gainMin, &gainMax, error);
    capsString = arv_pixel_format_to_gst_caps_string(pixFormat);
    double expTimeAbs = arv_device_get_float_feature_value(ad, "ExposureTimeAbs", error);
    double frameRateMin, frameRateMax;
    // this ain't work arv_camera_get_frame_rate_bounds(camera, &frameRateMin, &frameRateMax);
    arv_device_get_float_feature_bounds(ad, "AcquisitionFrameRate", &frameRateMin, &frameRateMax, error);
    fps = arv_camera_get_frame_rate(camera, error);

    logger->info("DEVICE SELECTED : {}", arv_camera_get_device_id(camera, error));
    logger->info("DEVICE NAME     : {}", arv_camera_get_model_name(camera, error));
    logger->info("DEVICE VENDOR   : {}", arv_camera_get_vendor_name(camera, error));
    logger->info("PAYLOAD         : {}", payload);
    logger->info("Width           : {}", mWidth);
    logger->info("Height          : {}", mHeight);
    logger->info("Exp Range       : [{} - {}]", exposureMin, exposureMax);
    logger->info("Exp             : {}", exp);
    logger->info("ExpTimeAbs      : {}", expTimeAbs);
    logger->info("Gain Range      : [{} - {}}", gainMin, gainMax);
    logger->info("Gain            : {}", gain);
    logger->info("Fps Range       : [{} - {}]", frameRateMin, frameRateMax);
    logger->info("Fps             : {}", fps);
    logger->info("Type            : {}", capsString);

    bool ackFREnabled = arv_device_get_boolean_feature_value(ad, "AcquisitionFrameRateEnabled", error);
    logger->info("Camera Frame rate control enabled (AcquisitionFrameRateEnabled) : {}", ackFREnabled);

    gint64 *values;
    guint n_values;
    values = arv_device_dup_available_enumeration_feature_values(ad, "AcquisitionMode", &n_values, error);
    const char **valuesAsStrings;
    valuesAsStrings = arv_device_dup_available_enumeration_feature_values_as_strings(ad, "AcquisitionMode", &n_values,
                                                                                     error);
    logger->info("AcquisitionModes :");
    for (int i = 0; i < n_values; i++) {
        logger->info("  [{}, {}]", values[i], valuesAsStrings[i]);
    }

    ArvAcquisitionMode aaMode;
    aaMode = arv_camera_get_acquisition_mode(camera, error);
    logger->info("Camera ARV Acquisistion Mode : {}", aaMode);

    cout << endl;

    cout << "DEVICE SELECTED : " << arv_camera_get_device_id(camera, error) << endl;
    cout << "DEVICE NAME     : " << arv_camera_get_model_name(camera, error) << endl;
    cout << "DEVICE VENDOR   : " << arv_camera_get_vendor_name(camera, error) << endl;
    cout << "PAYLOAD         : " << payload << endl;
    cout << "Width           : " << mWidth << endl
         << "Height          : " << mHeight << endl;
    cout << "Exp Range       : [" << exposureMin << " - " << exposureMax << "]" << endl;
    cout << "Exp             : " << exp << endl;
    cout << "Gain Range      : [" << gainMin << " - " << gainMax << "]" << endl;
    cout << "Gain            : " << gain << endl;
    cout << "Fps             : " << fps << endl;
    cout << "Type            : " << capsString << endl;
    cout << endl;

    // Create a new stream object. Open stream on Camera.
    stream = arv_camera_create_stream(camera, NULL, NULL, error);

    if (stream != NULL) {
        // Push 50 buffer in the stream input buffer queue.
        arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));
        // Set acquisition mode to single frame.
        arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_SINGLE_FRAME, error);
        aaMode = arv_camera_get_acquisition_mode(camera, error);
        logger->info("Camera ARV Acquisistion Mode : {}", aaMode);
        // Very usefull to avoid arv buffer timeout status
        sleep(2);

        //Timestamping
        auto start_t = std::chrono::system_clock::now();
        logger->info("Timestamp before arv_camera_start_acquisition call : {}",
                     date::format("%FT%T", date::floor<std::chrono::microseconds>(start_t)));

        // Start acquisition.
        arv_camera_start_acquisition(camera, error);

        cout << ">> Acquisition in progress... (Please wait)" << endl;

        // Get image buffer.
        ArvBuffer *arv_buffer = arv_stream_timeout_pop_buffer(stream, 2 * frame.mExposure + 5000000); //us

        auto end_t = std::chrono::system_clock::now();
        logger->info("Timestamp after arv_stream_timeout_pop_buffer      : {}",
                     date::format("%FT%T", date::floor<std::chrono::microseconds>(end_t)));
        auto time = end_t - std::chrono::microseconds((long) exp);
        logger->info("Timestamp after captured - exposure time           : {}",
            date::format("%FT%T", date::floor<std::chrono::microseconds>(time)));

        char *buffer_data;
        size_t buffer_size;
        if (arv_buffer != NULL) {
            if (arv_buffer_get_status(arv_buffer) == ARV_BUFFER_STATUS_SUCCESS) {
                buffer_data = (char *) arv_buffer_get_data(arv_buffer, &buffer_size);
                if (pixFormat == ARV_PIXEL_FORMAT_MONO_8) {
                    Mat image = Mat(mHeight, mWidth, CV_8UC1, buffer_data);
                    image.copyTo(frame.mImg);
                } else if (pixFormat == ARV_PIXEL_FORMAT_MONO_12) {
                    // Unsigned short image.
                    Mat image = Mat(mHeight, mWidth, CV_16UC1, buffer_data);

                    // http://www.theimagingsource.com/en_US/support/documentation/icimagingcontrol-class/PixelformatY16.htm
                    // Some sensors only support 10-bit or 12-bit pixel data. In this case, the least significant bits are don't-care values.
                    if (shiftBitsImage) {
                        unsigned short *p;
                        for (int i = 0; i < image.rows; i++) {
                            p = image.ptr<unsigned short>(i);
                            for (int j = 0; j < image.cols; j++) p[j] = p[j] >> 4;
                        }
                    }
                    image.copyTo(frame.mImg);
                }

                frame.mDate = TimeDate::splitIsoExtendedDate(date::format("%FT%T", date::floor<std::chrono::microseconds>(start_t)));
                frame.mFps = arv_camera_get_frame_rate(camera, error);
                res = true;
            } else {
                switch (arv_buffer_get_status(arv_buffer)) {
                    case 0 :
                        cout << "ARV_BUFFER_STATUS_SUCCESS : the buffer contains a valid image" << endl;
                        break;
                    case 1 :
                        cout << "ARV_BUFFER_STATUS_CLEARED: the buffer is cleared" << endl;
                        break;
                    case 2 :
                        cout << "ARV_BUFFER_STATUS_TIMEOUT: timeout was reached before all packets are received"
                             << endl;
                        break;
                    case 3 :
                        cout << "ARV_BUFFER_STATUS_MISSING_PACKETS: stream has missing packets" << endl;
                        break;
                    case 4 :
                        cout << "ARV_BUFFER_STATUS_WRONG_PACKET_ID: stream has packet with wrong id" << endl;
                        break;
                    case 5 :
                        cout
                                << "ARV_BUFFER_STATUS_SIZE_MISMATCH: the received image didn't fit in the buffer data space"
                                << endl;
                        break;
                    case 6 :
                        cout << "ARV_BUFFER_STATUS_FILLING: the image is currently being filled" << endl;
                        break;
                    case 7 :
                        cout << "ARV_BUFFER_STATUS_ABORTED: the filling was aborted before completion" << endl;
                        break;
                }
                res = false;
            }
            arv_stream_push_buffer(stream, arv_buffer);
        } else {
            logger->error("Fail to pop buffer from stream.");
            res = false;
        }
        arv_stream_get_statistics(stream, &nbCompletedBuffers, &nbFailures, &nbUnderruns);

        cout << ">> Completed buffers = " << (unsigned long long) nbCompletedBuffers << endl;
        cout << ">> Failures          = " << (unsigned long long) nbFailures << endl;
        //cout << ">> Underruns         = " << (unsigned long long) nbUnderruns          << endl;

        // Stop acquisition.
        arv_camera_stop_acquisition(camera, error);

        g_object_unref(stream);
        stream = NULL;
        g_object_unref(camera);
        camera = NULL;
    }

    return res;
}

void CameraUsbAravis::saveGenicamXml(string p)
{
    const char *xml;
    size_t size;
    xml = arv_device_get_genicam_xml(arv_camera_get_device(camera), &size);
    if (xml != NULL) {
        std::ofstream infFile;
        string infFilePath = p + "genicam.xml";
        infFile.open(infFilePath.c_str());
        infFile << string(xml, size);
        infFile.close();
    }
}

//https://github.com/GNOME/aravis/blob/b808d34691a18e51eee72d8cac6cfa522a945433/src/arvtool.c
void CameraUsbAravis::getAvailablePixelFormats()
{
    ArvGc *genicam;
    ArvDevice *device;
    ArvGcNode *node;
    GError **error = NULL;

    if (camera != NULL) {
        device = arv_camera_get_device(camera);
        genicam = arv_device_get_genicam(device);
        node = arv_gc_get_node(genicam, "PixelFormat");

        if (ARV_IS_GC_ENUMERATION(node)) {
            const GSList *childs;
            const GSList *iter;
            vector<string> pixfmt;

            cout << ">> Device pixel formats :" << endl;

            childs = arv_gc_enumeration_get_entries(ARV_GC_ENUMERATION(node));
            for (iter = childs; iter != NULL; iter = iter->next) {
                if (arv_gc_feature_node_is_implemented(ARV_GC_FEATURE_NODE(iter->data), NULL)) {
                    if (arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(iter->data), NULL)) {
                        {
                            string fmt = string(arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(iter->data)));
                            std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::toupper);
                            pixfmt.push_back(fmt);
                            cout << "- " << fmt << endl;
                        }
                    }
                }
            }

            // Compare found pixel formats to currently formats supported by freeture

            cout << endl << ">> Available pixel formats :" << endl;
            EParser<CamPixFmt> fmt;
            for (int i = 0; i != pixfmt.size(); i++) {
                if (fmt.isEnumValue(pixfmt.at(i))) {
                    cout << "- " << pixfmt.at(i) << " available --> ID : " << fmt.parseEnum(pixfmt.at(i)) << endl;
                }
            }
        } else {
            cout << ">> Available pixel formats not found." << endl;
        }

        g_object_unref(device);
    }
}

void CameraUsbAravis::getExposureBounds(double &eMin, double &eMax)
{
    double exposureMin = 0.0;
    double exposureMax = 0.0;
    GError **error = NULL;
    arv_camera_get_exposure_time_bounds(camera, &exposureMin, &exposureMax, error);
    eMin = exposureMin;
    eMax = exposureMax;
}

double CameraUsbAravis::getExposureTime()
{
    GError **error = NULL;
    return arv_camera_get_exposure_time(camera, error);
}

void CameraUsbAravis::getGainBounds(int &gMin, int &gMax)
{
    double gainMin = 0.0;
    double gainMax = 0.0;
    GError **error = NULL;
    arv_camera_get_gain_bounds(camera, &gainMin, &gainMax, error);
    gMin = gainMin;
    gMax = gainMax;
}

bool CameraUsbAravis::getPixelFormat(CamPixFmt &format)
{
    GError **error = NULL;
    ArvPixelFormat pixFormat = arv_camera_get_pixel_format(camera, error);

    switch (pixFormat) {
        case ARV_PIXEL_FORMAT_MONO_8 :
            format = MONO8;
            break;
        case ARV_PIXEL_FORMAT_MONO_12 :
            format = MONO12;
            break;
        case ARV_PIXEL_FORMAT_MONO_16 :
            format = MONO16;
            break;
        default :
            return false;
            break;
    }

    return true;
}


bool CameraUsbAravis::getFrameSize(int &w, int &h)
{
    GError **error = NULL;
    if (camera != NULL) {
        int ww = 0, hh = 0;
        arv_camera_get_region(camera, NULL, NULL, &ww, &h, error);
        w = ww;
        h = hh;
    }

    return false;
}

bool CameraUsbAravis::getFPS(double &value)
{
    GError **error = NULL;
    if (camera != NULL) {
        value = arv_camera_get_frame_rate(camera, error);
        return true;
    }
    return false;
}

string CameraUsbAravis::getModelName()
{
    GError **error = NULL;
    return arv_camera_get_model_name(camera, error);
}

bool CameraUsbAravis::setExposureTime(double val)
{
    double expMin, expMax;
    GError **error = NULL;
    arv_camera_get_exposure_time_bounds(camera, &expMin, &expMax, error);
    ArvDevice *ad;
    ad = arv_camera_get_device(camera);
    auto logger = spdlog::get("acq_logger");

    if (camera != NULL) {
        if (val < expMin && val > expMax) {
            logger->warn("Exposure value ({}) is not in range [{} - {}] ", val, expMin, expMax);
            cout << "Exposure value (" << val << ") is not in range [ " << expMin << " - " << expMax << " ]" << endl;
        } else {
            exp = arv_camera_get_exposure_time(camera, error);
            logger->info("Exp read pre set  : {}", exp);

            // MC: disable auto exposure control if set
            gint64 expAuto;
            expAuto = arv_device_get_integer_feature_value(ad, "ExposureAuto", error);
            // ExposureAuto :
            //   [0, Off]
            //   [1, Once]
            //   [2, Continuous]
            if (expAuto != 0) {
                logger->info("Original Camera ExposureAuto Mode : {}", expAuto);
                arv_device_set_integer_feature_value(ad, "ExposureAuto", 0, error);
                expAuto = arv_device_get_integer_feature_value(ad, "ExposureAuto", error);
                logger->info("Camera ExposureAuto Mode : {}", expAuto);
            }

            arv_camera_set_exposure_time(camera, val, error);
            // MC: no check for bounds in arv_camera_set_exposure_time(), but writes as follows: 
            /* arv_device_set_float_feature_value (camera->priv->device,
							    camera->priv->has_exposure_time ?
							    "ExposureTime" :
							    "ExposureTimeAbs", exposure_time_us);
            */
            exp = arv_camera_get_exposure_time(camera, error);
            logger->info("Exp read after set: {}", exp);
            // check if exposure time is pretty much what was requested
            // (ie does not differ more than 1/100 of average from requested and read back value)
            if (abs((val - exp) / ((val + exp) / (double) 2)) > ((double) 1 / (double) 100)) {
                logger->error("exposure time read back ({}) differs from value requested to be set ({})", exp, val);
            } else {
                logger->info("exposure time read back ({}) is pretty much same as value requested to be set ({})", exp, val);
            }
            // MC: need to get ArvDevice to be get more complete access to camera features
            ArvDevice *ad;
            ad = arv_camera_get_device(camera);
            double expTimeAbs = arv_device_get_float_feature_value(ad, "ExposureTimeAbs", error);
            logger->info("ExpTimeAbs      : {}", expTimeAbs);
        }
        return true;
    }
    return false;
}

bool CameraUsbAravis::setGain(int val)
{
    auto logger = spdlog::get("acq_logger");
    double gMin, gMax;
    GError **error = NULL;
    arv_camera_get_gain_bounds(camera, &gMin, &gMax, error);
    ArvDevice *ad;
    ad = arv_camera_get_device(camera);

    if (camera != NULL) {
        if ((double) val >= gMin && (double) val <= gMax) {
            // MC: disable auto exposure control if set
            gint64 gainAuto;
            gainAuto = arv_device_get_integer_feature_value(ad, "GainAuto", error);
            // GainAuto :
            //   [0, Off]
            //   [1, Once]
            //   [2, Continuous]
            if (gainAuto != 0) {
                logger->info("Original Camera GainAuto Mode : {}", gainAuto);
                arv_device_set_integer_feature_value(ad, "GainAuto", 0, error);
                gainAuto = arv_device_get_integer_feature_value(ad, "GainAuto", error);
                logger->info("Camera GainAuto Mode : {}", gainAuto);
            }

            arv_camera_set_gain(camera, (double) val, error);

            // read back to check if value accepted by camera
            gain = arv_camera_get_gain(camera, error);
            logger->info("Gain read after set: {}", gain);
            // check if gain is pretty much what was requested
            // (ie does not differ more than 1/100 of average from requested and read back value)
            if (abs((val - gain) / ((val + gain) / (double) 2)) > ((double) 1 / (double) 100)) {
                logger->error("gain read back ({}) differs from value requested to be set ({})", gain, val);
            } else {
                logger->info("gain time read back ({}) is pretty much same as value requested to be set ({})", gain, val);
            }
        } else {
            cout << "CameraUsbAravis::setGain(int): Gain value (" << val << ") is not in range [ " << gMin << " - "
                 << gMax << " ]" << endl;
            logger->error("CameraUsbAravis::setGain(int): Gain value ({}) is not in range [{} - {}]", val, gMin, gMax);
            return false;
        }
        return true;
    }
    return false;
}

bool CameraUsbAravis::setFPS(double val)
{
    auto logger = spdlog::get("acq_logger");
    GError **error = NULL;
    if (camera != NULL) {
        // MC: need to get ArvDevice to be get more complete access to camera features
        ArvDevice *ad;
        ad = arv_camera_get_device(camera);

        double frameRateMin, frameRateMax;
        // this ain't work arv_camera_get_frame_rate_bounds(camera, &frameRateMin, &frameRateMax);
        arv_device_get_float_feature_bounds(ad, "AcquisitionFrameRate", &frameRateMin, &frameRateMax, error);

        if (val >= frameRateMin && val <= frameRateMax)
            // MC note: arv_camera_set_frame_rate() silently limits written value to bounds
            //  also it does following for FLIR cameras
            /*		case ARV_CAMERA_VENDOR_POINT_GREY_FLIR:
			arv_device_set_string_feature_value (camera->priv->device, "TriggerSelector", "FrameStart");
			arv_device_set_string_feature_value (camera->priv->device, "TriggerMode", "Off");
			if (camera->priv->has_acquisition_frame_rate_enabled)
				arv_device_set_integer_feature_value (camera->priv->device, "AcquisitionFrameRateEnabled", 1);
			else
				arv_device_set_boolean_feature_value (camera->priv->device, "AcquisitionFrameRateEnable", 1);
			arv_device_set_string_feature_value (camera->priv->device, "AcquisitionFrameRateAuto", "Off");
			arv_device_set_float_feature_value (camera->priv->device, "AcquisitionFrameRate", frame_rate);
            */
        {
            arv_camera_set_frame_rate(camera, val, error);
            // read back what was set
            this->fps = arv_camera_get_frame_rate(camera, error);
            logger->info("Camera frame rate : {}", this->fps);
            // check if frame rate is pretty much what was requested
            // (ie does not differ more than 1/100 of average from requested and read back value)
            if (abs((val - fps) / ((val + fps) / (double) 2)) > ((double) 1 / (double) 100)) {
                logger->error("frame rate read back ({}) differs from value requested ({})", fps, val);
            } else {
                logger->info("frame rate read back ({}) is pretty much same as value requested ({})", fps, val);
            }
        } else {
            cout << "CameraUsbAravis::setFPS(double): FPS value (" << val << ") is not in range [ " << frameRateMin
                 << " - " << frameRateMax << " ]" << endl;
            logger->error("CameraUsbAravis::setFPS(double): FPS value ({}) is not in range [{} - {}]", val, frameRateMin, frameRateMax);
            return false;
        }
        return true;
    }
    return false;
}

bool CameraUsbAravis::setPixelFormat(CamPixFmt depth)
{
    GError **error = NULL;
    if (camera != NULL) {
        switch (depth) {
            case MONO8 : {
                arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_8, error);
            }
                break;
            case MONO12 : {
                arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_12, error);
            }
                break;
        }
        return true;
    }
    return false;
}

bool CameraUsbAravis::setFrameSize(int startx, int starty, int width, int height, bool customSize)
{
    auto logger = spdlog::get("acq_logger");
    GError **error = NULL;
    if (camera != NULL) {
        if (customSize) {
            if (arv_device_get_feature(arv_camera_get_device(camera), "OffsetX")) {
                cout << "Starting from : " << mStartX << "," << mStartY;
                logger->info("Starting from : {},{}", mStartX, mStartY);
            } else {
                logger->warn("OffsetX, OffsetY are not available: cannot set offset.");
            }
            arv_camera_set_region(camera, startx, starty, width, height, error);
            arv_camera_get_region(camera, &mStartX, &mStartY, &mWidth, &mHeight, error);

            // Default is maximum size
        } else {
            int sensor_width, sensor_height;
            arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);
            logger->info("Camera sensor size : {}x{}", sensor_width, sensor_height);
            arv_camera_set_region(camera, 0, 0, sensor_width, sensor_height, error);
            arv_camera_get_region(camera, NULL, NULL, &mWidth, &mHeight, error);
        }
        return true;
    }
    return false;
}

// Reset of BFLY-U3-23S6M-C with FW V1.9.3
// the LED turns orange and stays like that(sleep mode)
// camera needs to be re-connecetd  getCamerasList(); createDevice(int id);
void CameraUsbAravis::deviceReset()
{
    auto logger = spdlog::get("acq_logger");
    // MC: need to get ArvDevice to be get more complete access to camera features
    ArvDevice *ad;
    GError **error = NULL;
    ad = arv_camera_get_device(camera);
    logger->info("Initiating camera reset... ");
    arv_device_execute_command(ad, "DeviceReset", error);
    sleep(10);
    logger->info("Camera reset done, with 10s wait afterwards.");
}

#endif
