#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdio.h>
#include "V4LCapture.h"

using namespace std;
using namespace cv;

int exp_time = 55;
int gain = 30;
int brightness_ = 10;
int whiteness_ = 86;
int saturation_ = 60;

V4LCapture capture("/dev/video1", 640, 480, true, 4);

void on_expTracker(int, void *)
{
    //capture.set(CV_CAP_PROP_EXPOSURE,exp_time);
    capture.setExposureTime(0, ::exp_time); //settings->exposure_time);
}
void on_gainTracker(int, void *)
{
    //capture.set(CV_CAP_PROP_GAIN,::gain);
    capture.setpara(gain, brightness_, whiteness_, saturation_); // cap.setExposureTime(0, ::exp_time);//settings->exposure_time);
}
void on_brightnessTracker(int, void *)
{

    //capture.set(CV_CAP_PROP_BRIGHTNESS,::brightness_);
    capture.setpara(gain, brightness_, whiteness_, saturation_); //cap.setExposureTime(0, ::exp_time);//settings->exposure_time);
}
void on_whitenessTracker(int, void *)
{
    //capture.set(CV_CAP_PROP_WHITE_BALANCE_BLUE_U,::whiteness_);
    capture.setpara(gain, brightness_, whiteness_, saturation_); // cap.setExposureTime(0, ::exp_time);//settings->exposure_time);
}
void on_saturationTracker(int, void *)
{
    //capture.set(CV_CAP_PROP_SATURATION,::saturation_);
    capture.setpara(gain, brightness_, whiteness_, saturation_); //cap.setExposureTime(0, ::exp_time);//settings->exposure_time);
}

bool setcamera()
{
#ifdef _SHOW_PHOTO
    namedWindow(wndname, 1);
#endif
    //V4LCapture capture("/dev/video0", 3);
    // capture.setVideoFormat(800, 600, 1);

    //capture.open(0);
    //capture.set(CV_CAP_PROP_FRAME_WIDTH,800);
    //capture.set(CV_CAP_PROP_FRAME_HEIGHT,600);
    // capture.setExposureTime(0, 62);//settings->exposure_time);
    if (!capture.open())
    {
        cout << "Open Camera failure.\n";
        return false;
    }
    Mat img;
    //printf("%d %d \n",img.cols,img.rows);
    //if(img.empty())
    //	return 0;
    capture >> img;
    cout << "image width: " << img.cols << " , image height: " << img.rows << endl;
    namedWindow("Tuning", 1);
    createTrackbar("exposure_time", "Tuning", &::exp_time, 100, on_expTracker);
    createTrackbar("gain", "Tuning", &::gain, 100, on_gainTracker);
    createTrackbar("whiteness", "Tuning", &::whiteness_, 100, on_whitenessTracker);
    createTrackbar("brightness_", "Tuning", &::brightness_, 100, on_brightnessTracker);
    createTrackbar("saturation", "Tuning", &::saturation_, 100, on_saturationTracker);
    on_brightnessTracker(0, 0);
    on_expTracker(0, 0);
    on_gainTracker(0, 0);
    on_saturationTracker(0, 0);
    on_whitenessTracker(0, 0);

    while (1)
    {
        capture >> img;

        if (img.empty())
            continue;
        imshow("Tuning", img);
        char c = waitKey(20);
        if (c == 'q') {
            break;
		}
    }
    destroyWindow("Tuning");
    return true;
    //V4LCapture cap("/dev/video0", 3);
}

int main(int argc, char** argv)
{
#if 1
	setcamera();
	return 0;
#else
	if (argc < 2)
	{
		std::cout << "Usage: cvcam <dev>" << std::endl;
		return -1;
	}

	//const char* device = argv[1];
    int device = atoi(argv[1]);
	cv::VideoCapture cap(device);
	if (!cap.isOpened())
	{
		std::cout << "[ERROR] Can not open device: " << device <<  std::endl;
		return -1;
	}

	cv::Mat image;

	while (1)
	{
		double t0 = cv::getTickCount();
		cap >> image;
		double t1 = cv::getTickCount();
		double dt = (t1 - t0) / cv::getTickFrequency();
		double expect = 34.0;
		dt = dt * 1000; // s->ms
		if (dt > expect) // assume the camera is 30fps
		{
			printf("[CVCAM] [WARNING] frame interval (%.f) is longer than expected (%.f)\n", dt, expect);
		}
		else
		{
			// printf("[CVCAM] [INFO] frame interval (%.f) is smaller than threshold (%.f)\n", dt, expect);
		}

		if (image.empty())
			break;

		cv::imshow("image", image);
		char key = cv::waitKey(30);
		if (key == 'q') break;
	}

	return 0;
#endif
}
