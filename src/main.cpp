#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <future>
#include <filesystem>

#include <sys/ioctl.h>

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

namespace fs = std::filesystem;

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
	// Get input data
	std::string videoPath;
	
	char input[1024];
	
	{
		const char *home = getenv("HOME");
		
		bool isValid = false;
		
		while(!isValid)
		{
			std::cout << "Enter video file path: ";
			std::cin.getline(input, 1024, '\n');
			std::cout << std::endl;
			
			videoPath = std::string(input);
			
			// Replace ~ character with homepath
			if(videoPath[0] == '~')
			{
				videoPath.replace(0, 1, "");
				videoPath.insert(0, home);
			}
			
			isValid = fs::exists(videoPath) && !fs::is_directory(videoPath);
		}
	}
	
	#pragma region Setup Capture
	
	cv::VideoCapture cap{ videoPath };
	
	if(!cap.isOpened())
	{
		std::cout << "Error while opening video" << std::endl;
		return -1;
	}
	
	cv::Mat startFrame;
	cap >> startFrame;
	
	cv::resize(startFrame, startFrame, cv::Size(), 1., 0.5);
	
	
	
	int width = startFrame.cols;
	int height = startFrame.rows;

	{
		int columns, rows;
		
		#ifdef _WIN32
		// Windows
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		
		#else
		// Unix
		
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		
		columns = w.ws_col;
		rows = w.ws_row;
		
		#endif
		
		float fWidth = static_cast<float>(width);
		float fHeight = static_cast<float>(height);
		
		if(fHeight > rows - 2)
		{
			fHeight = rows - 2;
			
			fWidth = fWidth / ( ( fWidth / fHeight ) / ( static_cast<float>( startFrame.cols ) / startFrame.rows ) );
		}
		
		if(fWidth > columns)
		{
			fWidth = columns;
			
			fHeight = fHeight * ( ( fWidth / fHeight ) / ( static_cast<float>( startFrame.cols ) / startFrame.rows ) );
		}
		
		// Truncate towards zero
		width = static_cast<int>(fWidth);
		height = static_cast<int>(fHeight);
	}
	
	#pragma endregion
	
	
	// Since they are unicode, they need to be stored independently as strings
	std::string chr[] = { " ", "░", "▒", "▓", "█" };
	int chrSize = sizeof(chr) / sizeof(*chr);
	
	std::string display = "";
	display.reserve(width * ( height + 1 ) + 10);
	
	double updateDelay = Date::Unit::US * 1000. / cap.get(cv::CAP_PROP_FPS);
	
	// Clear console
	std::cout << "\x1b[2J";
	
	// Start music
	auto startTime = Date::now(Date::Unit::US);
	
	std::future<void> playMusic( std::async(std::launch::async,
		[videoPath]()
		{
			std::string command = std::string("mplayer -vo null ") + videoPath + " > /dev/null";
			
			system(command.data());
		}
	));
	
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

				cv::resize(frame, frame, cv::Size(width, height), 0., 0., cv::INTER_AREA);
				cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
				
				display.clear();
				
				for(int j = 0; j < frame.rows; j++)
				{
					for(int i = 0; i < frame.cols; i++)
					{
						uchar value = frame.at<uchar>(j, i);
						
						display += chr[static_cast<int>( std::floor(chrSize * value / 256.) )];
						
						// For RGB
						// display += "\x1b[48;2;";
						// display += std::to_string(value[2]);
						// display += ";";
						// display += std::to_string(value[1]);
						// display += ";";
						// display += std::to_string(value[0]);
						// display += "m ";
					}
					
					display.append("\n");
				}
				
				std::cout << "\x1b[1;1H" << display;
			}
			else
			{
				if(!cap.grab()) break;
			}
			
			startTime += updateDelay;
		}
	}
	
	cap.release();
	
	std::cout << "\x1b[0m\x1b[1000B\x1b[1G";
	
	playMusic.get();
	
	
	return 0;
}