/*
                        CameraGigeAravis.cpp

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*   This file is part of:   freeture
*
*   Copyright:      (C) 2014-2016 Yoan Audureau
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
*   Last modified:      30/03/2018
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/**
* \file    CameraGigeAravis.cpp
* \author  Yoan Audureau -- Chiara Marmo -- GEOPS-UPSUD
* \version 1.2
* \date    30/03/2018
* \brief   Use Aravis library to pilot GigE Cameras.
*          https://wiki.gnome.org/action/show/Projects/Aravis?action=show&redirect=Aravis
*/

#include "CameraGigeAravis.h"
#include <spdlog/spdlog.h>
#include <fstream>

#if 0

    CameraGigeAravis::CameraGigeAravis(bool shift):
    camera(NULL), mStartX(0), mStartY(0), mWidth(0), mHeight(0), fps(0), gainMin(0.0), gainMax(0.0),
    payload(0), exposureMin(0), exposureMax(0), gain(0), exp(0), nbCompletedBuffers(0),
    nbFailures(0), nbUnderruns(0), frameCounter(0), shiftBitsImage(shift), stream(NULL) {
        mExposureAvailable = true;
        mGainAvailable = true;
        mInputDeviceType = CAMERA;
    }

    CameraGigeAravis::CameraGigeAravis():
    camera(NULL), mStartX(0), mStartY(0), mWidth(0), mHeight(0), fps(0), gainMin(0.0), gainMax(0.0),
    payload(0), exposureMin(0), exposureMax(0), gain(0), exp(0), nbCompletedBuffers(0),
    nbFailures(0), nbUnderruns(0), frameCounter(0), shiftBitsImage(false), stream(NULL) {
        mExposureAvailable = true;
        mGainAvailable = true;
        mInputDeviceType = CAMERA;
    }

    CameraGigeAravis::~CameraGigeAravis(){

        if(stream != NULL)
            g_object_unref(stream);

        if(camera != NULL)
            g_object_unref(camera);

    }

    vector<pair<int,string>> CameraGigeAravis::getCamerasList() {

        vector<pair<int,string>> camerasList;

        ArvInterface *interface;

        //arv_update_device_list();

        int ni = arv_get_n_interfaces();


        for (int j = 0; j< ni; j++){

            const char* name = arv_get_interface_id (j);
            if (strcmp(name,"GigEVision") == 0) {
                interface = arv_gv_interface_get_instance();
                arv_interface_update_device_list(interface);
                //int nb = arv_get_n_devices();

                int nb = arv_interface_get_n_devices(interface);

                for(int i = 0; i < nb; i++){

                    pair<int,string> c;
                    c.first = i;
                    //const char* str = arv_get_device_id(i);
                    const char* str = arv_interface_get_device_id(interface,i);
                    const char* addr = arv_interface_get_device_address(interface,i);
                    std::string s = str;
                    c.second = "NAME[" + s + "] SDK[ARAVIS] IP: " + addr;
                    camerasList.push_back(c);
                }
            }
        }

       return camerasList;

    }

    bool CameraGigeAravis::listCameras(){
        auto logger = spdlog::get("acq_logger");
        ArvInterface *interface;
        //arv_update_device_list();

        int ni = arv_get_n_interfaces ();
        spdlog::info("------------ GIGE CAMERAS WITH ARAVIS ----------");

        for (int j = 0; j< ni; j++){
            interface = arv_gv_interface_get_instance();
            arv_interface_update_device_list(interface);
            //int nb = arv_get_n_devices();

            int nb = arv_interface_get_n_devices(interface);
            for(int i = 0; i < nb; i++){
                spdlog::info("-> [{}]{}", i, arv_interface_get_device_id(interface,i));
            }

            if(nb == 0) {
                spdlog::info("-> No cameras detected...");
                return false;
            }
        }

        spdlog::info("------------------------------------------------");
        return true;
    }

    bool CameraGigeAravis::createDevice(int id){
        auto logger = spdlog::get("acq_logger");
        string deviceName;

        if(!getDeviceNameById(id, deviceName))
            return false;

        GError **error = NULL;
        camera = arv_camera_new(deviceName.c_str(), error);
        if(camera == NULL){
            logger->error("Fail to connect the camera.");
            return false;
        }

        return true;
    }

    bool CameraGigeAravis::setSize(int startx, int starty, int width, int height, bool customSize) {
        auto logger = spdlog::get("acq_logger");
        GError **error = NULL;

        if(customSize) {
            arv_camera_set_region(camera, startx, starty, width, height, error);
            arv_camera_get_region (camera, &mStartX, &mStartY, &mWidth, &mHeight, error);
            logger->info("Camera region size : {}x{}", mWidth, mHeight);
            if (arv_device_get_feature(arv_camera_get_device(camera), "OffsetX")) {
                logger->info("Starting from : {},{}", mStartX, mStartY);
            } else {
                logger->warn("OffsetX, OffsetY are not available: cannot set offset.");
            }
        // Default is maximum size
        }else {
            int sensor_width, sensor_height;
            arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);
            logger->info("Camera sensor size : {}x{}", sensor_width, sensor_height);

            arv_camera_set_region(camera, 0, 0,sensor_width,sensor_height, error);
            arv_camera_get_region (camera, NULL, NULL, &mWidth, &mHeight, error);
        }

        return true;
    }

    bool CameraGigeAravis::getDeviceNameById(int id, string &device){
        auto logger = spdlog::get("acq_logger");
        arv_update_device_list();
        int n_devices = arv_get_n_devices();

        for(int i = 0; i< n_devices; i++){
            if(id == i){
                device = arv_get_device_id(i);
                return true;
            }
        }

        logger->error("Fail to retrieve camera with this ID.");
        return false;
    }

    bool CameraGigeAravis::grabInitialization(){
        auto logger = spdlog::get("acq_logger");
        frameCounter = 0;
        GError **error = NULL;

        payload = arv_camera_get_payload (camera, error);
        logger->info("Camera payload : {}", payload);

        pixFormat = arv_camera_get_pixel_format(camera, error);

        arv_camera_get_exposure_time_bounds (camera, &exposureMin, &exposureMax, error);
        logger->info("Camera exposure bound min : {}", exposureMin);
        logger->info("Camera exposure bound max : {}", exposureMax);

        arv_camera_get_gain_bounds (camera, &gainMin, &gainMax, error);
        logger->info("Camera gain bound min : {}", gainMin);
        logger->info("Camera gain bound max : {}", gainMax);

        // left the set packet size as introduced by gmke/Marcus Kempf (commit 909b0a34ad7d63bf5d89bcef1ed002ffb06e33ae)
        if(arv_camera_is_gv_device (camera)) {
            // http://www.baslerweb.com/media/documents/AW00064902000%20Control%20Packet%20Timing%20With%20Delays.pdf
            // https://github.com/GNOME/aravis/blob/06ac777fc6d98783680340f1c3f3ea39d2780974/src/arvcamera.c

            // Configure the inter packet delay to insert between each packet for the current stream
            // channel. This can be used as a crude flow-control mechanism if the application or the network
            // infrastructure cannot keep up with the packets coming from the device.
            //arv_camera_gv_set_packet_delay (camera, 4000);

            // Specifies the stream packet size, in bytes, to send on the selected channel for a GVSP transmitter
            // or specifies the maximum packet size supported by a GVSP receiver.
            arv_camera_gv_set_packet_size (camera, 1488, error);
            packetsize = arv_camera_gv_get_packet_size(camera, error);
            logger->info("Camera packet size : {}", packetsize);
        }

        //arv_camera_set_frame_rate(camera, 30);
        fps = arv_camera_get_frame_rate(camera, error);
        logger->info("Camera frame rate : {}", fps);

        capsString = arv_pixel_format_to_gst_caps_string(pixFormat);
        logger->info("Camera format : {}", capsString);

        gain = arv_camera_get_gain(camera, error);
        logger->info("Camera gain : {}", gain);

        exp = arv_camera_get_exposure_time(camera, error);
        logger->info("Camera exposure : {}", exp);

        spdlog::info("DEVICE SELECTED : {}", arv_camera_get_device_id(camera, error));
        spdlog::info("DEVICE NAME     : {}", arv_camera_get_model_name(camera, error));
        spdlog::info("DEVICE VENDOR   : {}", arv_camera_get_vendor_name(camera, error));
        spdlog::info("PAYLOAD         : {}", payload);
        spdlog::info("Start X         : {}", mStartX);
        spdlog::info("Start Y         : {}", mStartY);
        spdlog::info("Width           : {}", mWidth);
        spdlog::info("Height          : {}", mHeight);
        spdlog::info("Exp Range       : [{} - {}]", exposureMin, exposureMax);
        spdlog::info("Exp             : {}", exp);
        spdlog::info("Gain Range      : [{} - {}]", gainMin, gainMax);
        spdlog::info("Gain            : {}", gain);
        spdlog::info("Fps             : {}", fps);
        spdlog::info("Type            : {}", capsString);
        spdlog::info("Packet Size     : {}", arv_camera_gv_get_packet_size(camera, error));

        // Create a new stream object. Open stream on Camera.
        stream = arv_camera_create_stream(camera, NULL, NULL, error);
        if(stream == NULL){
            logger->critical("Fail to create stream with arv_camera_create_stream()");
            return false;
        }

        if (ARV_IS_GV_STREAM(stream)){
            bool            arv_option_auto_socket_buffer   = true;
            bool            arv_option_no_packet_resend     = true;
            unsigned int    arv_option_packet_timeout       = 20;
            unsigned int    arv_option_frame_retention      = 100;

            if(arv_option_auto_socket_buffer){
                g_object_set(stream,
                            // ARV_GV_STREAM_SOCKET_BUFFER_FIXED : socket buffer is set to a given fixed value.
                            // ARV_GV_STREAM_SOCKET_BUFFER_AUTO: socket buffer is set with respect to the payload size.
                            "socket-buffer", ARV_GV_STREAM_SOCKET_BUFFER_AUTO,
                            // Socket buffer size, in bytes.
                            // Allowed values: >= G_MAXULONG
                            // Default value: 0
                            "socket-buffer-size", 0, NULL);
            }

            if(arv_option_no_packet_resend){
                // # packet-resend : Enables or disables the packet resend mechanism

                // If packet resend is disabled and a packet has been lost during transmission,
                // the grab result for the returned buffer holding the image will indicate that
                // the grab failed and the image will be incomplete.
                //
                // If packet resend is enabled and a packet has been lost during transmission,
                // a request is sent to the camera. If the camera still has the packet in its
                // buffer, it will resend the packet. If there are several lost packets in a
                // row, the resend requests will be combined.

                g_object_set(stream,
                            // ARV_GV_STREAM_PACKET_RESEND_NEVER: never request a packet resend
                            // ARV_GV_STREAM_PACKET_RESEND_ALWAYS: request a packet resend if a packet was missing
                            // Default value: ARV_GV_STREAM_PACKET_RESEND_ALWAYS
                            "packet-resend", ARV_GV_STREAM_PACKET_RESEND_NEVER, NULL);
            }

            g_object_set(stream,
                        // # packet-timeout

                        // The Packet Timeout parameter defines how long (in milliseconds) we will wait for
                        // the next expected packet before it initiates a resend request.

                        // Packet timeout, in µs.
                        // Allowed values: [1000,10000000]
                        // Default value: 40000
                        "packet-timeout",/* (unsigned) arv_option_packet_timeout * 1000*/(unsigned)40000,
                        // # frame-retention

                        // The Frame Retention parameter sets the timeout (in milliseconds) for the
                        // frame retention timer. Whenever detection of the leader is made for a frame,
                        // the frame retention timer starts. The timer resets after each packet in the
                        // frame is received and will timeout after the last packet is received. If the
                        // timer times out at any time before the last packet is received, the buffer for
                        // the frame will be released and will be indicated as an unsuccessful grab.

                        // Packet retention, in µs.
                        // Allowed values: [1000,10000000]
                        // Default value: 200000
                        "frame-retention", /*(unsigned) arv_option_frame_retention * 1000*/(unsigned) 200000,NULL);
        }else
            return false;

        // Push 50 buffer in the stream input buffer queue.
        for (int i = 0; i < 50; i++)
            arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));

        return true;
    }

    void CameraGigeAravis::grabCleanse(){}

    bool CameraGigeAravis::acqStart(){
        GError **error = NULL;
        auto logger = spdlog::get("acq_logger");
        logger->info("Set camera to CONTINUOUS MODE");
        arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, error);

        logger->info("Set camera TriggerMode to Off");
        arv_device_set_string_feature_value(arv_camera_get_device (camera), "TriggerMode" , "Off", error);

        logger->info("Start acquisition on camera");
        arv_camera_start_acquisition(camera, error);

        return true;
    }

    void CameraGigeAravis::acqStop(){
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

    bool CameraGigeAravis::grabImage(Frame &newFrame){
        auto logger = spdlog::get("acq_logger");
        GError **error = NULL;
        ArvBuffer *arv_buffer;
        //exp = arv_camera_get_exposure_time(camera);

        arv_buffer = arv_stream_timeout_pop_buffer(stream,2000000); //us
        char *buffer_data;
        size_t buffer_size;

        if(arv_buffer == NULL){
            throw runtime_error("arv_buffer is NULL");
            return false;
        }else{
            try{
                if(arv_buffer_get_status(arv_buffer) == ARV_BUFFER_STATUS_SUCCESS){
                    //BOOST_LOG_SEV(logger, normal) << "Success to grab a frame.";
                    buffer_data = (char *) arv_buffer_get_data (arv_buffer, &buffer_size);

                    //Timestamping.
                    //string acquisitionDate = TimeDate::localDateTime(microsec_clock::universal_time(),"%Y:%m:%d:%H:%M:%S");
                    //BOOST_LOG_SEV(logger, normal) << "Date : " << acquisitionDate;
                    string acquisitionDate = TimeDate::IsoExtendedStringNow();
                    //BOOST_LOG_SEV(logger, normal) << "Date : " << acqDateInMicrosec;

                    Mat image;
                    CamPixFmt imgDepth = MONO8;
                    int saturateVal = 0;

                    if(pixFormat == ARV_PIXEL_FORMAT_MONO_8){
                        //BOOST_LOG_SEV(logger, normal) << "Creating Mat 8 bits ...";
                        image = Mat(mHeight, mWidth, CV_8UC1, buffer_data);
                        imgDepth = MONO8;
                        saturateVal = 255;
                    }else if(pixFormat == ARV_PIXEL_FORMAT_MONO_12){
                        //BOOST_LOG_SEV(logger, normal) << "Creating Mat 16 bits ...";
                        image = Mat(mHeight, mWidth, CV_16UC1, buffer_data);
                        imgDepth = MONO12;
                        saturateVal = 4095;

                        //double t3 = (double)getTickCount();
                        if(shiftBitsImage){
                            //BOOST_LOG_SEV(logger, normal) << "Shifting bits ...";
                                unsigned short * p;
                                for(int i = 0; i < image.rows; i++){
                                    p = image.ptr<unsigned short>(i);
                                    for(int j = 0; j < image.cols; j++)
                                        p[j] = p[j] >> 4;
                                }
                            //BOOST_LOG_SEV(logger, normal) << "Bits shifted.";
                        }

                        //t3 = (((double)getTickCount() - t3)/getTickFrequency())*1000;
                        //cout << "> Time shift : " << t3 << endl;
                    }

                    //BOOST_LOG_SEV(logger, normal) << "Creating frame object ...";
                    newFrame = Frame(image, gain, exp, acquisitionDate);
                    //BOOST_LOG_SEV(logger, normal) << "Setting date of frame ...";
                    //newFrame.setAcqDateMicro(acqDateInMicrosec);
                    //BOOST_LOG_SEV(logger, normal) << "Setting fps of frame ...";
                    newFrame.mFps = fps;
                    newFrame.mFormat = imgDepth;
                    //BOOST_LOG_SEV(logger, normal) << "Setting saturated value of frame ...";
                    newFrame.mSaturatedValue = saturateVal;
                    newFrame.mFrameNumber = frameCounter;
                    frameCounter++;

                    //BOOST_LOG_SEV(logger, normal) << "Re-pushing arv buffer in stream ...";
                    arv_stream_push_buffer(stream, arv_buffer);
                    return true;
                }else{
                    switch(arv_buffer_get_status(arv_buffer)){
                        case 0 :
                            spdlog::info("ARV_BUFFER_STATUS_SUCCESS : the buffer contains a valid image");
                            break;
                        case 1 :
                            spdlog::info("ARV_BUFFER_STATUS_CLEARED: the buffer is cleared");
                            break;
                        case 2 :
                            spdlog::info("ARV_BUFFER_STATUS_TIMEOUT: timeout was reached before all packets are received");
                            break;
                        case 3 :
                            spdlog::info("ARV_BUFFER_STATUS_MISSING_PACKETS: stream has missing packets");
                            break;
                        case 4 :
                            spdlog::info("ARV_BUFFER_STATUS_WRONG_PACKET_ID: stream has packet with wrong id");
                            break;
                        case 5 :
                            spdlog::info("ARV_BUFFER_STATUS_SIZE_MISMATCH: the received image didn't fit in the buffer data space");
                            break;
                        case 6 :
                            spdlog::info("ARV_BUFFER_STATUS_FILLING: the image is currently being filled");
                            break;
                        case 7 :
                            spdlog::info("ARV_BUFFER_STATUS_ABORTED: the filling was aborted before completion");
                            break;

                    }

                    arv_stream_push_buffer(stream, arv_buffer);
                    return false;
                }
            }catch(exception& e){
                logger->critical(e.what()) ;
                spdlog::critical(e.what()) ;
                return false;
            }
        }
    }


    bool CameraGigeAravis::grabSingleImage(Frame &frame, int camID){

        bool res = false;
        GError **error = NULL;
        auto logger = spdlog::get("acq_logger");

        if(!createDevice(camID))
            return false;

        if(!setPixelFormat(frame.mFormat))
            return false;

        if(!setExposureTime(frame.mExposure))
            return false;

        if(!setGain(frame.mGain))
            return false;

        if(frame.mWidth > 0 && frame.mHeight > 0) {
            setFrameSize(frame.mStartX, frame.mStartY, frame.mWidth, frame.mHeight,1);
            //arv_camera_set_region(camera, frame.mStartX, frame.mStartY, frame.mWidth, frame.mHeight);
            //arv_camera_get_region (camera, NULL, NULL, &mWidth, &mHeight);
        }else{
            int sensor_width, sensor_height;
            arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);

            // Use maximum sensor size.
            arv_camera_set_region(camera, 0, 0,sensor_width,sensor_height, error);
            arv_camera_get_region (camera, NULL, NULL, &mWidth, &mHeight, error);
        }

        payload = arv_camera_get_payload (camera, error);
        pixFormat = arv_camera_get_pixel_format (camera, error);
        arv_camera_get_exposure_time_bounds (camera, &exposureMin, &exposureMax, error);
        arv_camera_get_gain_bounds (camera, &gainMin, &gainMax, error);
        arv_camera_set_frame_rate(camera, frame.mFps, error); /* Regular captures */
        fps = arv_camera_get_frame_rate(camera, error);
        capsString = arv_pixel_format_to_gst_caps_string(pixFormat);
        gain    = arv_camera_get_gain(camera, error);
        exp     = arv_camera_get_exposure_time(camera, error);

        spdlog::info("DEVICE SELECTED : {}", arv_camera_get_device_id(camera, error));
        spdlog::info("DEVICE NAME     : {}", arv_camera_get_model_name(camera, error));
        spdlog::info("DEVICE VENDOR   : {}", arv_camera_get_vendor_name(camera, error));
        spdlog::info("PAYLOAD         : {}", payload);
        spdlog::info("Start X         : {}", mStartX);
        spdlog::info("Start Y         : {}", mStartY);
        spdlog::info("Width           : {}", mWidth);
        spdlog::info("Height          : {}", mHeight);
        spdlog::info("Exp Range       : [{} - {}]", exposureMin, exposureMax);
        spdlog::info("Exp             : {}", exp);
        spdlog::info("Gain Range      : [{} - {}]", gainMin, gainMax);
        spdlog::info("Gain            : {}", gain);
        spdlog::info("Fps             : {}", fps);
        spdlog::info("Type            : {}", capsString);
        spdlog::info("Packet Size     : {}", arv_camera_gv_get_packet_size(camera, error));

        if(arv_camera_is_gv_device (camera)) {
            // http://www.baslerweb.com/media/documents/AW00064902000%20Control%20Packet%20Timing%20With%20Delays.pdf
            // https://github.com/GNOME/aravis/blob/06ac777fc6d98783680340f1c3f3ea39d2780974/src/arvcamera.c

            // Configure the inter packet delay to insert between each packet for the current stream
            // channel. This can be used as a crude flow-control mechanism if the application or the network
            // infrastructure cannot keep up with the packets coming from the device.
            arv_camera_gv_set_packet_delay (camera, 4000, error);

            // Specifies the stream packet size, in bytes, to send on the selected channel for a GVSP transmitter
            // or specifies the maximum packet size supported by a GVSP receiver.
            arv_camera_gv_set_packet_size (camera, 1444, error);
        }

        // Create a new stream object. Open stream on Camera.
        stream = arv_camera_create_stream(camera, NULL, NULL, error);
        if(stream != NULL){
            if(ARV_IS_GV_STREAM(stream)){
                bool            arv_option_auto_socket_buffer   = true;
                bool            arv_option_no_packet_resend     = true;
                unsigned int    arv_option_packet_timeout       = 20;
                unsigned int    arv_option_frame_retention      = 100;

                if(arv_option_auto_socket_buffer){
                    g_object_set(stream, "socket-buffer", ARV_GV_STREAM_SOCKET_BUFFER_AUTO, "socket-buffer-size", 0, NULL);
                }

                if(arv_option_no_packet_resend){
                    g_object_set(stream, "packet-resend", ARV_GV_STREAM_PACKET_RESEND_NEVER, NULL);
                }

                g_object_set(stream, "packet-timeout", (unsigned)40000, "frame-retention", (unsigned) 200000,NULL);
            }

            // Push 50 buffer in the stream input buffer queue.
            arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));

            // Set acquisition mode to continuous.
            arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_SINGLE_FRAME, error);

            // Very usefull to avoid arv buffer timeout status
            sleep(1);

            // Start acquisition.
            arv_camera_start_acquisition(camera, error);

            // Get image buffer.
            ArvBuffer *arv_buffer = arv_stream_timeout_pop_buffer(stream, frame.mExposure + 5000000); //us

            char *buffer_data;
            size_t buffer_size;

            spdlog::info(">> Acquisition in progress... (Please wait)");

            if (arv_buffer != NULL){
                if(arv_buffer_get_status(arv_buffer) == ARV_BUFFER_STATUS_SUCCESS){
                    buffer_data = (char *) arv_buffer_get_data (arv_buffer, &buffer_size);
                    if(pixFormat == ARV_PIXEL_FORMAT_MONO_8){
                        Mat image = Mat(mHeight, mWidth, CV_8UC1, buffer_data);
                        image.copyTo(frame.mImg);
                    }else if(pixFormat == ARV_PIXEL_FORMAT_MONO_12){
                        // Unsigned short image.
                        Mat image = Mat(mHeight, mWidth, CV_16UC1, buffer_data);

                        // http://www.theimagingsource.com/en_US/support/documentation/icimagingcontrol-class/PixelformatY16.htm
                        // Some sensors only support 10-bit or 12-bit pixel data. In this case, the least significant bits are don't-care values.
                        if(shiftBitsImage){
                            unsigned short * p;
                            for(int i = 0; i < image.rows; i++){
                                p = image.ptr<unsigned short>(i);
                                for(int j = 0; j < image.cols; j++) p[j] = p[j] >> 4;
                            }
                        }

                        image.copyTo(frame.mImg);
                    }

                    frame.mDate = TimeDate::splitIsoExtendedDate(TimeDate::IsoExtendedStringNow());
                    frame.mFps = arv_camera_get_frame_rate(camera, error);
                    res = true;
                }else{
                    switch(arv_buffer_get_status(arv_buffer)){
                        case 0 :
                            spdlog::info("ARV_BUFFER_STATUS_SUCCESS : the buffer contains a valid image");
                            break;
                        case 1 :
                            spdlog::info("ARV_BUFFER_STATUS_CLEARED: the buffer is cleared");
                            break;
                        case 2 :
                            spdlog::info("ARV_BUFFER_STATUS_TIMEOUT: timeout was reached before all packets are received");
                            break;
                        case 3 :
                            spdlog::info("ARV_BUFFER_STATUS_MISSING_PACKETS: stream has missing packets");
                            break;
                        case 4 :
                            spdlog::info("ARV_BUFFER_STATUS_WRONG_PACKET_ID: stream has packet with wrong id");
                            break;
                        case 5 :
                            spdlog::info("ARV_BUFFER_STATUS_SIZE_MISMATCH: the received image didn't fit in the buffer data space");
                            break;
                        case 6 :
                            spdlog::info("ARV_BUFFER_STATUS_FILLING: the image is currently being filled");
                            break;
                        case 7 :
                            spdlog::info("ARV_BUFFER_STATUS_ABORTED: the filling was aborted before completion");
                            break;
                    }

                    res = false;
                }
                arv_stream_push_buffer(stream, arv_buffer);
           }else{
                logger->error("Fail to pop buffer from stream.");
                res = false;
           }
            arv_stream_get_statistics(stream, &nbCompletedBuffers, &nbFailures, &nbUnderruns);

            spdlog::info(">> Completed buffers = {}", (unsigned long long) nbCompletedBuffers);
            spdlog::info(">> Failures          = {}", (unsigned long long) nbFailures);

            // Stop acquisition.
            arv_camera_stop_acquisition(camera, error);

            g_object_unref(stream);
            stream = NULL;
            g_object_unref(camera);
            camera = NULL;
        }

        return res;
    }

    void CameraGigeAravis::saveGenicamXml(string p){
        const char *xml;
        size_t size;

        xml = arv_device_get_genicam_xml (arv_camera_get_device(camera), &size);
        if (xml != NULL){
            std::ofstream oinfFile;
            string infFilePath = p + "genicam.xml";
            oinfFile.open(infFilePath.c_str());
            oinfFile << string ( xml, size );
            oinfFile.close();
        }
    }

    //https://github.com/GNOME/aravis/blob/b808d34691a18e51eee72d8cac6cfa522a945433/src/arvtool.c
    void CameraGigeAravis::getAvailablePixelFormats() {
        ArvGc *genicam;
        ArvDevice *device;
        ArvGcNode *node;
        GError **error = NULL;

        if(camera != NULL) {
            device = arv_camera_get_device(camera);
            genicam = arv_device_get_genicam(device);
            node = arv_gc_get_node(genicam, "PixelFormat");

            if (ARV_IS_GC_ENUMERATION (node)) {
                const GSList *childs;
                const GSList *iter;
                vector<string> pixfmt;

                spdlog::info(">> Device pixel formats :");

                childs = arv_gc_enumeration_get_entries (ARV_GC_ENUMERATION (node));
                for (iter = childs; iter != NULL; iter = iter->next) {
                    if (arv_gc_feature_node_is_implemented (ARV_GC_FEATURE_NODE (iter->data), NULL)) {
                        if(arv_gc_feature_node_is_available (ARV_GC_FEATURE_NODE (iter->data), NULL)) {
                            {
                                string fmt = string(arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE (iter->data)));
                                std::transform(fmt.begin(), fmt.end(),fmt.begin(), ::toupper);
                                pixfmt.push_back(fmt);
                                spdlog::info("- {}", fmt);
                            }
                        }
                    }
                }

                // Compare found pixel formats to currently formats supported by freeture

                spdlog::info(">> Available pixel formats :");
                EParser<CamPixFmt> fmt;

                for( int i = 0; i != pixfmt.size(); i++ ) {
                    if(fmt.isEnumValue(pixfmt.at(i))) {
                        spdlog::info("- {} available --> ID : {}", pixfmt.at(i), fmt.parseEnum(pixfmt.at(i)));
                    }
                }
            }else {
                spdlog::info(">> Available pixel formats not found.");
            }

            g_object_unref(device);
        }
    }

    void CameraGigeAravis::getExposureBounds(double &eMin, double &eMax){
        double exposureMin = 0.0;
        double exposureMax = 0.0;
        GError **error = NULL;
        arv_camera_get_exposure_time_bounds(camera, &exposureMin, &exposureMax, error);
        eMin = exposureMin;
        eMax = exposureMax;
    }

    double CameraGigeAravis::getExposureTime(){
        GError **error = NULL;
        return arv_camera_get_exposure_time(camera, error);
    }

    void CameraGigeAravis::getGainBounds(int &gMin, int &gMax){
        double gainMin = 0.0;
        double gainMax = 0.0;
        GError **error = NULL;
        arv_camera_get_gain_bounds(camera, &gainMin, &gainMax, error);
        gMin = gainMin;
        gMax = gainMax;
    }

    bool CameraGigeAravis::getPixelFormat(CamPixFmt &format){
        GError **error = NULL;
        ArvPixelFormat pixFormat = arv_camera_get_pixel_format(camera, error);

        switch(pixFormat){
            case ARV_PIXEL_FORMAT_MONO_8 :
                format = MONO8;
                break;
            case ARV_PIXEL_FORMAT_MONO_12 :
                format = MONO12;
                break;
            default :
                return false;
                break;
        }

        return true;
    }


    bool CameraGigeAravis::getFrameSize(int &x, int &y, int &w, int &h) {
        GError **error = NULL;
        if(camera != NULL) {
            int ww = 0, hh = 0, xx = 0, yy = 0;
            arv_camera_get_region(camera, &x, &y, &w, &h, error);
            x = xx;
            y = yy;
            w = ww;
            h = hh;
        }

        return false;
    }

    bool CameraGigeAravis::getFPS(double &value){
        if(camera != NULL) {
            GError **error = NULL;
            value = arv_camera_get_frame_rate(camera, error);
            return true;
        }

        return false;
    }

    string CameraGigeAravis::getModelName(){
        GError **error = NULL;
        return arv_camera_get_model_name(camera, error);
    }

    bool CameraGigeAravis::setExposureTime(double val){
        double expMin, expMax;
        GError **error = NULL;

        arv_camera_get_exposure_time_bounds(camera, &expMin, &expMax, error);
        if(camera != NULL){
            if(val >= expMin && val <= expMax) {
                exp = val;
                arv_camera_set_exposure_time(camera, val, error);
            }else{
                spdlog::info("> Exposure value ({}) is not in range [{} - {}]", val, expMin, expMax);
                return false;
            }
            return true;
        }

        return false;
    }

    bool CameraGigeAravis::setGain(int val){
        auto logger = spdlog::get("acq_logger");
        double gMin, gMax;
        GError **error = NULL;

        arv_camera_get_gain_bounds (camera, &gMin, &gMax, error);
        if (camera != NULL){
            if((double)val >= gMin && (double)val <= gMax){
                gain = val;
                arv_camera_set_gain (camera, (double)val, error);
            }else{
                spdlog::info("> Gain value ({}) is not in range [{} - {}]", val, gMin, gMax);
                logger->info("> Gain value ({}) is not in range [{} - {}]", val, gMin, gMax);
                return false;
            }
        return true;
        }

        return false;
    }

    bool CameraGigeAravis::setFPS(double fps){
        if (camera != nullptr){
            auto logger = spdlog::get("acq_logger");
            GError **error = nullptr;
            arv_camera_set_frame_rate(camera, fps, error);
            double setfps = arv_camera_get_frame_rate(camera, error);
            if (setfps!=fps) {
                spdlog::info("> Frame rate value ({}) can't be set! Please check genicam features.", fps);
                logger->info("> Frame rate value ({}) can't be set! Please check genicam features.", fps);
            }
            return true;
        }

        return false;
    }

    bool CameraGigeAravis::setPixelFormat(CamPixFmt depth){
        if (camera != NULL){
            GError **error = NULL;
            switch(depth){
                case MONO8 :
                    {
                        arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_8, error);
                    }
                    break;
                case MONO12 :
                    {
                        arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_12, error);
                    }
                    break;
            }

            return true;
        }

        return false;
    }

    bool CameraGigeAravis::setFrameSize(int startx, int starty, int width, int height, bool customSize) {
        auto logger = spdlog::get("acq_logger");
        GError **error = NULL;
        if (camera != NULL){
            if(customSize) {
                if (arv_device_get_feature(arv_camera_get_device(camera), "OffsetX")) {
                    spdlog::info("Starting from : {},{}", mStartX, mStartY);
                    logger->info("Starting from : {},{}", mStartX, mStartY);
                } else {
                    logger->warn("OffsetX, OffsetY are not available: cannot set offset.");
                }
                arv_camera_set_region(camera, startx, starty, width, height, error);
                arv_camera_get_region (camera, &mStartX, &mStartY, &mWidth, &mHeight, error);

            // Default is maximum size
            } else {
                int sensor_width, sensor_height;
                arv_camera_get_sensor_size(camera, &sensor_width, &sensor_height, error);
                logger->info("Camera sensor size : {}x{}", sensor_width, sensor_height);

                arv_camera_set_region(camera, 0, 0,sensor_width,sensor_height, error);
                arv_camera_get_region (camera, NULL, NULL, &mWidth, &mHeight, error);
            }
            return true;
        }
        return false;
    }

#endif
