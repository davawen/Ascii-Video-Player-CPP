#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <future>
#include <filesystem>
#include <sstream>

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

int main(/* int argc, char *argv[] */)
{
	
	
	// Get input, such as video path, color mode wanted etc...
	std::string videoPath;
	bool useColor;
	int chrMode = 0;
	
	{
		char input[1024];
		
		// Get home directory
		const char *home = getenv("HOME");
		
		do
		{
			std::cout << "Enter video file path: ";
			std::cin.getline(input, 1024, '\n');
			
			videoPath = std::string(input);
			
			// Replace ~ character with home directory
			if(videoPath[0] == '~')
			{
				videoPath.replace(0, 1, "");
				videoPath.insert(0, home);
			}
		}
		while(!fs::exists(videoPath) || fs::is_directory(videoPath));

		while(true)
		{
			std::cout << "Wether to use color(y/N): ";
			std::cin.getline(input, 2, '\n');
			
			switch(input[0])
			{
				case 'y':
				case 'Y':
					useColor = true;
					break;
				case 'n':
				case 'N':
				case ' ':
				case '\0':
					useColor = false;
					// Choose char mode
					while(true)
					{
						std::cout << "How it should be rendered(BLOCK, ascii, black and white) (0, 1, 2): ";
						std::cin.getline(input, 2, '\n');

						switch(input[0])
						{
							case '0':
							case '\0':
							case ' ':
								chrMode = 0;
								break;
							case '1':
								chrMode = 1;
								break;
							case '2':
								chrMode = 2;
								break;
							default:
								continue;
						}

						break;
					}
					break;
				default:
					continue;
			}
			
			break;
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

	// Limit video size to console size
	// Scope for cleanness
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
	

	const char **chr;
	int chrSize;
	int chrOffset;
	
	#pragma region Set chr based on char mode
	{
		// Since they are unicode, they need to be stored as pointer arrays
		const char *_chr0[] = { " ", "\u2591", "\u2592", "\u2593", "\u2589" };
		const char *_chr1[] = { " ", ".", "\"", ",", ":", "-", "~", "=", "|", "(", "{", "[", "&", "#", "@" };
		const char *_chr2[] =
		{
			"\x1b[48;5;232m ", "\x1b[48;5;234m ", "\x1b[48;5;236m ",
			"\x1b[48;5;238m ", "\x1b[48;5;240m ", "\x1b[48;5;242m ",
			"\x1b[48;5;244m ", "\x1b[48;5;246m ", "\x1b[48;5;248m ",
			"\x1b[48;5;250m ", "\x1b[48;5;252m ", "\x1b[48;5;254m "
		};
		
		switch(chrMode)
		{
			case 0:
				chr = _chr0;
				chrOffset = sizeof(*_chr0);
				chrSize = sizeof(_chr0);
				break;
			case 1:
				chr = _chr1;
				chrOffset = sizeof(*_chr1);
				chrSize = sizeof(_chr1);
				break;
			case 2:
				chr = _chr2;
				chrOffset = sizeof(*_chr2);
				chrSize = sizeof(_chr2);
				break;
			default:
				std::cout << "Undefined color mode choosen" << std::endl;
				return -1;
		}
		
		chrSize /= chrOffset;
	}
	#pragma endregion
	
	double updateDelay = static_cast<double>(Date::Unit::US) * 1000. / cap.get(cv::CAP_PROP_FPS);
	
	// Clear console
	std::cout << "\x1b[2J";
	
	// Start music
	
	std::future<void> playMusic(std::async(std::launch::async,
		[videoPath]()
		{
			char cmd[512];
			snprintf(cmd, 512, "mplayer -vo null -slave \"%s\" %s", videoPath.c_str(), "> /dev/null");
			system(cmd);
		}
	));
	
	auto startTime = Date::now(Date::Unit::US);
	
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ); // Set stdout to be fully buffered
	
	while(true)
	{
		long elapsedTime = ( Date::now(Date::Unit::US) - startTime );
		int index = elapsedTime / updateDelay;
		
		for(int k = 1; k < index; k++)
		{
			if(k >= index-1)
			{
				cv::Mat frame;

				if(!cap.read(frame)) goto endLoop;

				cv::resize(frame, frame, cv::Size(width, height), 0., 0., cv::INTER_AREA);
				
				if(!useColor) cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
				
				printf("\x1b[1;1H");
				
				for(int j = 0; j < frame.rows; j++)
				{
					for(int i = 0; i < frame.cols; i++)
					{
						if(!useColor)
						{
							uint8_t value = frame.at<uint8_t>(j, i);

							printf("%s", chr[static_cast<int>(std::floor(chrSize * value / 256.))]);
						}
						else
						{							
							cv::Vec3b value = frame.at<cv::Vec3b>(j, i);
							
							printf("\x1b[48;2;%hhu;%hhu;%hhum ", value[2], value[1], value[0]);
						}
					}
					
					std::cout.put('\n');
				}
				
				printf("\x1b[0m");
				std::cout.flush();
			}
			else
			{
				if(!cap.grab()) goto endLoop;
			}
			
			startTime += updateDelay;
		}
	}

endLoop:
	
	cap.release();
	
	std::cout << "\x1b[0m\x1b[1000B\x1b[1G";
	
	playMusic.get();
	
	return 0;
}
