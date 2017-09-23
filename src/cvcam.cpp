#include <opencv2/opencv.hpp>
#include <iostream>

int main(int argc, char** argv)
{
	if (argc < 2) 
	{
		std::cout << "Usage: cvcam <dev>" << std::endl;
		return -1;
	}

	const char* device = argv[1];
	cv::VideoCapture cap(device);
	if (!cap.isOpened())
	{
		std::cout << "[ERROR] Can not open device: " << device <<  std::endl;
		return -1;
	}

	cv::Mat image;

	while (1)
	{
		cap >> image;

		if (image.empty())
			break;

		cv::imshow("image", image);
		int key = cv::waitKey(30);
		if (key == 'q') break;
	}

	return 0;
}
