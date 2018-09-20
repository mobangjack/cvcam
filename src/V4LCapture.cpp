#include "V4LCapture.h"

#include <linux/videodev2.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>

void V4LCapture::init(int width, int height, bool mjpg, int size_buffer) {
    capture_width = width;
    capture_height = height;
    use_jpeg = mjpg;
    buffer_size = size_buffer;
    buffer_index = 0;
    frame_count = 0;
    fd = -1;
}

V4LCapture::V4LCapture(const char * device, int width, int height, bool mjpg, int size_buffer)
    : video_device(device) {
    init(width, height, mjpg, size_buffer);
    open(device, width, height, mjpg, size_buffer);
}

V4LCapture::V4LCapture(int device_id, int width, int height, bool mjpg, int size_buffer) {
    init(width, height, mjpg, size_buffer);
    open(device_id, width, height, mjpg, size_buffer);
}

V4LCapture::~V4LCapture(){
    close();
}

void V4LCapture::cvtRaw2Mat(const void * data, cv::Mat & image){
    if (format == V4L2_PIX_FMT_MJPEG){
        cv::Mat src(capture_height, capture_width, CV_8UC3, (void*) data);
        image = cv::imdecode(src, 1);
    }
    else if(format == V4L2_PIX_FMT_YUYV){
        cv::Mat yuyv(capture_height, capture_width, CV_8UC2, (void*) data);
        cv::cvtColor(yuyv, image, CV_YUV2BGR_YUYV);
    }
}

V4LCapture & V4LCapture::operator >> (cv::Mat & image) {
//    std::cout << "current buffr idx: " << buffer_index << std::endl;
    struct v4l2_buffer bufferinfo = {0};
    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    bufferinfo.index = buffer_index;
    if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
        perror("VIDIOC_DQBUF Error");
        return *this;
    }

    //std::cout << "raw data size: " << bufferinfo.bytesused << std::endl;
    cvtRaw2Mat(mb[buffer_index].ptr, image);

    //memset(&bufferinfo, 0, sizeof(bufferinfo));
    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    bufferinfo.index = buffer_index;

    // Queue the next one.
    if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
        perror("VIDIOC_DQBUF Error");
        return *this;
    }
    ++buffer_index;
    buffer_index = buffer_index >= buffer_size ? buffer_index - buffer_size : buffer_index;
    ++frame_count;
    return *this;
}

bool V4LCapture::mapBuffer(){
    struct v4l2_requestbuffers bufrequest = {0};
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = buffer_size;

    if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0){
        perror("VIDIOC_REQBUFS");
        return false;
    }

    for(int i = 0; i < buffer_size; ++i){
        struct v4l2_buffer bufferinfo = {0};
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index = i; /* Queueing buffer index 0. */

        // Put the buffer in the incoming queue.
        if(ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0){
            perror("VIDIOC_QUERYBUF");
            return false;
        }

        mb[i].ptr = mmap(
            NULL,
            bufferinfo.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            bufferinfo.m.offset);
        mb[i].size = bufferinfo.length;

        if(mb[i].ptr == MAP_FAILED){
            perror("MAP_FAILED");
            return false;
        }
        memset(mb[i].ptr, 0, bufferinfo.length);

        // Put the buffer in the incoming queue.
        memset(&bufferinfo, 0, sizeof(bufferinfo));
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index = i;
        if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
            perror("VIDIOC_QBUF");
            return false;
        }
    }
    return true;
}

bool V4LCapture::unmapBuffer()
{
    int ret = 0;
    for(int i = 0; i < buffer_size; ++i){
        ret += munmap(mb[i].ptr, mb[i].size);
    }
    return ret == 0;
}

bool V4LCapture::startStream(){
    frame_count = 0;
    refreshVideoFormat();
    if(mapBuffer() == false) {
        return false;
    }

    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        perror("VIDIOC_STREAMON");
        return false;
    }
    return true;
}

bool V4LCapture::closeStream(){
    frame_count = 0;
    buffer_index = 0;
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
        perror("VIDIOC_STREAMOFF");
        return false;
    }
    return unmapBuffer();
}

bool V4LCapture::isOpened()
{
    return fd > -1;
}

bool V4LCapture::open(const char * device, int width, int height, bool mjpg, int size_buffer)
{
    if (isOpened()) {
        return true;
    }
    video_device = device;
    fd = ::open(device, O_RDWR);
    if (fd < 0) {
        return false;
    }
    mb = new MapBuffer[buffer_size];
    if (!setVideoFormat(width, height, mjpg)) {
        // TODO
    }
    if (!startStream()) {
        ::close(fd);
        fd = -1;
        return false;
    }
    return true;
}

bool V4LCapture::open(int device_id, int width, int height, bool mjpg, int size_buffer)
{
    std::stringstream ss;
    ss << "/dev/video";
    ss << device_id;
    return open(ss.str().c_str(), width, height, mjpg, size_buffer);
}

bool V4LCapture::reopen(const char * device){
    if (close()) {
        return open(device, capture_width, capture_height, use_jpeg, buffer_size);
    } else {
        return false;
    }
}

bool V4LCapture::reopen(int device_id){
    if (close()) {
        return open(device_id, capture_width, capture_height, use_jpeg, buffer_size);
    } else {
        return false;
    }
}

bool V4LCapture::open(){
    return open(video_device, capture_width, capture_height, use_jpeg, buffer_size);
}

bool V4LCapture::reopen(){
    return reopen(video_device);
}

bool V4LCapture::close()
{
    if (isOpened())
    {
        closeStream();
        ::close(fd);
        fd = -1;
        delete [] mb;
    }
    return true;
}

bool V4LCapture::setExposureTime(bool auto_exp, int t){
    if (auto_exp){
        struct v4l2_control control_s;
        control_s.id = V4L2_CID_EXPOSURE_AUTO;
        control_s.value = V4L2_EXPOSURE_AUTO;
        if( xioctl(fd, VIDIOC_S_CTRL, &control_s) < 0){
            printf("Set Auto Exposure error\n");
            return false;
        }
    }
    else {
        struct v4l2_control control_s;
        control_s.id = V4L2_CID_EXPOSURE_AUTO;
        control_s.value = V4L2_EXPOSURE_MANUAL;
        if( xioctl(fd, VIDIOC_S_CTRL, &control_s) < 0){
            printf("::close MANUAL Exposure error\n");
            return false;
        }

        control_s.id = V4L2_CID_EXPOSURE_ABSOLUTE;
        control_s.value = t;
        if( xioctl(fd, VIDIOC_S_CTRL, &control_s) < 0){
            printf("Set Exposure Time error\n");
            return false;
        }
    }
    return true;
}

bool V4LCapture::changeVideoFormat(int width, int height, bool mjpg){
    closeStream();
    reopen();
    setVideoFormat(width, height, mjpg);
    startStream();
    return true;
}

bool V4LCapture::setVideoFormat(int width, int height, bool mjpg){
    if (capture_width == width && capture_height == height && mjpg == use_jpeg)
        return true;
    capture_width = width;
    capture_height = height;
    frame_count = 0;
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    if (mjpg == true)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    else
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
        printf("Setting Pixel Format\n");
        return false;
    }
    return true;
}

bool V4LCapture::refreshVideoFormat(){
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt)) {
        perror("Querying Pixel Format\n");
        return false;
    }
    capture_width = fmt.fmt.pix.width;
    capture_height = fmt.fmt.pix.height;
    format = fmt.fmt.pix.pixelformat;
    return true;
}


bool V4LCapture::getVideoSize(int & width, int & height){
    if (capture_width == 0 || capture_height == 0){
        if (refreshVideoFormat() == false)
            return false;
    }
    width = capture_width;
    height = capture_height;
    return true;
}

bool V4LCapture::setVideoFPS(int fps){
    struct v4l2_streamparm stream_param = {0};
    stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_param.parm.capture.timeperframe.denominator = fps;
    stream_param.parm.capture.timeperframe.numerator = 1;

    if (-1 == xioctl(fd, VIDIOC_S_PARM, &stream_param)){
        printf("Setting Frame Rate\n");
        return false;
    }
    return true;
}

bool V4LCapture::setBufferSize(int bsize){
    if (buffer_size != bsize){
        buffer_size = bsize;
        delete [] mb;
        mb = new MapBuffer[buffer_size];
    }
}

void V4LCapture::info(){
    struct v4l2_capability caps = {};
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps)) {
            perror("Querying Capabilities\n");
            return;
    }

    printf( "Driver Caps:\n"
            "  Driver: \"%s\"\n"
            "  Card: \"%s\"\n"
            "  Bus: \"%s\"\n"
            "  Version: %d.%d\n"
            "  Capabilities: %08x\n",
            caps.driver,
            caps.card,
            caps.bus_info,
            (caps.version>>16)&&0xff,
            (caps.version>>24)&&0xff,
            caps.capabilities);


    struct v4l2_cropcap cropcap = {0};
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap))  {
            perror("Querying Cropping Capabilities\n");
            return;
    }

    printf( "Camera Cropping:\n"
            "  Bounds: %dx%d+%d+%d\n"
            "  Default: %dx%d+%d+%d\n"
            "  Aspect: %d/%d\n",
            cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
            cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
            cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);

    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    char fourcc[5] = {0};
    char c, e;
    printf("  FMT : CE Desc\n--------------------\n");
    while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
            c = fmtdesc.flags & 1? 'C' : ' ';
            e = fmtdesc.flags & 2? 'E' : ' ';
            printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
            fmtdesc.index++;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt)) {
        perror("Querying Pixel Format\n");
        return;
    }
    strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    printf( "Selected Camera Mode:\n"
            "  Width: %d\n"
            "  Height: %d\n"
            "  PixFmt: %s\n"
            "  Field: %d\n",
            fmt.fmt.pix.width,
            fmt.fmt.pix.height,
            fourcc,
            fmt.fmt.pix.field);

    struct v4l2_streamparm streamparm = {0};
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(fd, VIDIOC_G_PARM, &streamparm)) {
        perror("Querying Frame Rate\n");
        return;
    }
    printf( "Frame Rate:  %f\n====================\n",
            (float)streamparm.parm.capture.timeperframe.denominator /
            (float)streamparm.parm.capture.timeperframe.numerator);
}

int V4LCapture::xioctl(int fd, int request, void *arg){
    int r;
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}


int V4LCapture::getVideoDevice()
{
  struct v4l2_capability caps = {};
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps)) {
            perror("Querying Capabilities\n");
            return -1;
    }
    if (!strcmp((char*)caps.bus_info,"usb-tegra-xhci-3.3"))
      return 0;//left
    else if (!strcmp((char*)caps.bus_info,"usb-tegra-xhci-2.1"))
      return 1;//right
    else
      return -1;
}


void V4LCapture::getDefaultSetting()
{

  struct v4l2_queryctrl  Setting;
  Setting.id = V4L2_CID_GAIN;
  int ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------GAIN:\t\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);


  Setting.id = V4L2_CID_EXPOSURE;
  ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------EXPOSURE:\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);

  Setting.id = V4L2_CID_WHITENESS;
  ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------WHITENESS:\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);


  Setting.id = V4L2_CID_BRIGHTNESS;
  ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------BRIGHTNESS:\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);


  Setting.id = V4L2_CID_SATURATION;
  ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------SATURATION:\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);


  Setting.id = V4L2_CID_CONTRAST;
  ret = ioctl(fd, VIDIOC_QUERYCTRL, &Setting);
  printf("--------CONTRAST:\t Default Value:%d\t Min Value:%d\t Max Value:%d\t Step:%d\n",Setting.default_value,Setting.minimum,Setting.maximum,Setting.step);



}


void V4LCapture::getCurrentSetting()
{

  struct v4l2_control ctrl;
  ctrl.id = V4L2_CID_GAIN;
  int ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------GAIN:\t\t %d\n",ctrl.value);

  ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;;
  ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------EXPOSURE:\t %d\n",ctrl.value);

  ctrl.id = V4L2_CID_WHITENESS;
  ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------WHITENESS:\t %d\n",ctrl.value);

  ctrl.id = V4L2_CID_BRIGHTNESS;
  ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------BRIGHTNESS:\t %d\n",ctrl.value);

  ctrl.id = V4L2_CID_SATURATION;
  ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------SATURATION:\t %d\n",ctrl.value);

  ctrl.id = V4L2_CID_CONTRAST;
  ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
  printf("--------CONTRAST:\t %d\n",ctrl.value);


}

void V4LCapture::setpara(int gain, int brightness, int whiteness,int saturation)
{
  struct v4l2_control ctrl;
  ctrl.id = V4L2_CID_BRIGHTNESS;
  ctrl.value = brightness;
  int ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);

  ctrl.id = V4L2_CID_GAIN;
  ctrl.value = gain;
  ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);

  ctrl.id = V4L2_CID_WHITENESS;
  ctrl.value = whiteness;
  ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);

  ctrl.id = V4L2_CID_SATURATION;
  ctrl.value = saturation;
  ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
}
