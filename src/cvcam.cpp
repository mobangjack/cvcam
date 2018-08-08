#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdio.h>

int main(int argc, char** argv)
{
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

		if (image.empty())
			break;

		cv::imshow("image", image);
		int key = cv::waitKey(30);
		if (key == 'q') break;
	}

	return 0;
}
