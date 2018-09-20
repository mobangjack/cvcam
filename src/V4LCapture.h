#pragma once

#include <opencv2/opencv.hpp>

class V4LCapture {
public:
    V4LCapture(const char * device, int width = 640, int height = 480, bool mjpg = true, int size_buffer = 4);
    V4LCapture(int device_id, int width = 640, int height = 480, bool mjpg = 1, int size_buffer = 4);
    ~V4LCapture();
    bool isOpened();
    bool open(const char * device, int width = 640, int height = 480, bool mjpg = true, int size_buffer = 4);
    bool open(int device_id, int width = 640, int height = 480, bool mjpg = 1, int size_buffer = 4);
    bool open();
    bool reopen(const char * device);
    bool reopen(int device_id);
    bool reopen();
    bool close();
    bool startStream();
    bool closeStream();
    bool setExposureTime(bool auto_exp, int t);
    bool setVideoFormat(int width, int height, bool mjpg = 1);
    bool changeVideoFormat(int width, int height, bool mjpg = 1);
    bool getVideoSize(int & width, int & height);

    bool setVideoFPS(int fps);
    bool setBufferSize(int bsize);

    inline int getFrameCount() {
        return frame_count;
    }

    int getVideoDevice();
    void info();
    void getDefaultSetting();
    void getCurrentSetting();
    void setpara(int gain,int brightness,int whiteness,int saturation);

    V4LCapture& operator >> (cv::Mat & image);

protected:
    void init(int width = 640, int height = 480, bool mjpg = true, int size_buffer = 4);
    void cvtRaw2Mat(const void * data, cv::Mat & image);
    bool refreshVideoFormat();
    bool mapBuffer();
    bool unmapBuffer();
    int xioctl(int fd, int request, void *arg);

protected:
    struct MapBuffer {
        void * ptr;
        unsigned int size;
    };
    unsigned int capture_width;
    unsigned int capture_height;
    unsigned int format;
    int fd;
    unsigned int buffer_size;
    unsigned int buffer_index;
    unsigned int frame_count;
    MapBuffer * mb;
    const char * video_device;
    bool use_jpeg;
};
