#include <iostream>
#include <chrono>

#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio/videoio.hpp>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Date
{
	enum Unit
	{
		/**Nanosecond*/
		NS = 1,
		/**Microsecond*/
		US = 1000,
		/**Milisecond*/
		MS = 1000000
	};
	
	/**
	 * Returns the amount of time since the processor was started in the given unit
	 */
	long now(Unit unit = Unit::MS)
	{
		return std::chrono::high_resolution_clock::now().time_since_epoch().count() / unit;
	}
}

int main(int argc, char *argv[])
{
	#pragma region Setup Capture
	
	cv::VideoCapture cap{"./nichijou_op.mp4"};
	
	if(!cap.isOpened())
	{
		std::cout << "Error while opening video" << std::endl;
		return -1;
	}
	
	cv::Mat startFrame;
	cap >> startFrame;
	
	#pragma endregion
	
	auto startTime = Date::now(Date::Unit::US);
	
	double updateDelay = Date::Unit::US * 1000. / cap.get(cv::CAP_PROP_FPS);
	
	std::string str = "";
	str.reserve(startFrame.rows * startFrame.cols * 12);
	
	char chr[] = { '.', '\'', ',', ':', '-', '~', '=', '|', '(', '{', '[', '&', '#', '@'};
	
	std::cout << "\x1b[2J";
	
	while(true)
	{
		long elapsedTime = ( Date::now(Date::Unit::US) - startTime );
		int index = elapsedTime / updateDelay;
		
		for(int k = 1; k < index; k++)
		{
			if(k >= index-1)
			{
				cv::Mat frame;

				if(!cap.read(frame)) break;

				cv::resize(frame, frame, cv::Size(), 0.14, 0.07, cv::INTER_AREA);

				str.clear();

				for(int j = 0; j < frame.rows; j++)
				{
					for(int i = 0; i < frame.cols; i++)
					{
						cv::Vec3b value = frame.at<cv::Vec3b>(j, i);
						
						// str += chr[ static_cast<int>(std::floor( 14. * value )) ];
						str += "\x1b[48;2;";
						str += std::to_string(value[2]);
						str += ";";
						str += std::to_string(value[1]);
						str += ";";
						str += std::to_string(value[0]);
						str += "m ";
					}

					str.append("\n");
				}
				
				std::cout << "\x1b[1;1H" << str;
			}
			else
			{
				if(!cap.grab()) break;
			}
			
			startTime += updateDelay;
		}
	}
	
	std::cout << "\x1b[0m";
	
	return 0;
}