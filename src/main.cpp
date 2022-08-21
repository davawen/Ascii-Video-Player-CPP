#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <future>
#include <filesystem>
#include <sstream>
#include <string>
// #include <format> No std::format support for g++ yet :(
#include <fmt/core.h>

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

#include "flags.hpp"

namespace fs = std::filesystem;

using namespace std::chrono_literals;

/**
 * Returns the amount of time since the processor was started in the given unit
 */
template <typename Unit>
Unit now()
{
	return std::chrono::duration_cast<Unit>(std::chrono::high_resolution_clock::now().time_since_epoch());
}

template <typename T>
inline T &sample_array(cv::uint8_t v, std::vector<T> &vector) {
	return vector[ v * vector.size() / 255 ];
}

template <typename T>
inline const T &sample_array(cv::uint8_t v, const std::vector<T> &vector) {
	return vector[ v * vector.size() / 255 ];
}

int main(int argc, char *argv[])
{
	// Get input, such as video path, color mode wanted etc...
	auto flags = FlagMod::Flags(argc, argv)
		.name("AsciiVideoPlayer")
		.version("1.3.0");

	auto flag_help = flags.add_switch("help", 'h', "Show this help and exit.");
	auto flag_bright = flags.add_switch("bright", "Wether to recalibrate colored characters to max brightness");
	auto flag_color = flags.flag_required<int>("color", 'c', "Number of colors to use (int)");
	auto flag_file = flags.positional<std::string>("file");

	auto [help] = flags.parse(flag_help);
	if(help) {
		flags.print_help();
		return -1;
	}

	auto [b, c, videoPath] = flags.parse(flag_bright, flag_color, flag_file);

	if(!fs::exists(videoPath) || fs::is_directory(videoPath))
	{
		std::cout << "Non valid video path given.\n";
		return -1;
	}

	enum CharMode
	{
		BLOCK,
		ASCII,
	} chrMode = BLOCK;

	enum ColorMode
	{
		GRAYSCALE,
		COLOR,
		TRUE_COLOR
	} colorMode = COLOR;
	
	{
		char input[1024];
		
		while(true)
		{
			std::cout << "Color palette([C]olor(default), [G]rayscale, [T]rue Color): ";
			std::cin.getline(input, 2, '\n');
			
			switch(input[0])
			{
				case 'c':
				case 'C':
				case ' ':
				case '\0':
					colorMode = COLOR;
					break;
				case 'g':
				case 'G':
					colorMode = GRAYSCALE;
					break;
				case 't':
				case 'T':
					colorMode = TRUE_COLOR;
					break;
				default:
					continue;
			}
			
			break;
		}

		while(true)
		{
			std::cout << "How it should be rendered([B]lock(default), [A]scii): ";
			std::cin.getline(input, 2, '\n');

			switch(input[0])
			{
				case 'b':
				case 'B':
				case '\0':
				case ' ':
					chrMode = BLOCK;
					break;
				case 'a':
				case 'A':
					chrMode = ASCII;
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
	
	// Since they are unicode, they need to be stored as pointer arrays
	void (*transformer)(cv::uint8_t, cv::Vec3b, std::string &);

	std::vector<const char *> greyscaleChars =
	{
		"\x1b[48;5;232m ", "\x1b[48;5;234m ", "\x1b[48;5;236m ",
		"\x1b[48;5;238m ", "\x1b[48;5;240m ", "\x1b[48;5;242m ",
		"\x1b[48;5;244m ", "\x1b[48;5;246m ", "\x1b[48;5;248m ",
		"\x1b[48;5;250m ", "\x1b[48;5;252m ", "\x1b[48;5;254m "
	};

	// buffer += chars[static_cast<int>(std::floor(chars.size() * value / 256.))];

	switch(chrMode)
	{
		case BLOCK:
			if(colorMode == TRUE_COLOR) transformer = [](cv::uint8_t, cv::Vec3b value, std::string &buffer) {
				buffer += fmt::format("\x1b[48;2;{};{};{}m ", value[2], value[1], value[0]);
			};
			else if(colorMode == COLOR) transformer = [](cv::uint8_t, cv::Vec3b value, std::string &buffer) {
				buffer += fmt::format("\x1b[48;5;{}m ", 16 + value[0]/42 + value[1]/42*6 + value[2]/42*36);
			};
			else transformer = [](cv::uint8_t value, cv::Vec3b, std::string &buffer) {
				const std::vector<const char *> blockChars = { " ", "\u2591", "\u2592", "\u2593", "\u2589" };

				buffer += sample_array(value, blockChars);
			};
			break;
		case ASCII:
			if(colorMode == TRUE_COLOR) transformer = [](cv::uint8_t g, cv::Vec3b value, std::string &buffer) {
				const std::vector<char> asciiChars = { ' ', '.', '\"', ',', ':', '-', '~', '=', '|', '(', '{', '[', '&', '#', '@' };

				// Boost color to max brightness to counteract character size = dimming
				uint8_t maxValue = std::max(value[0], std::max(value[1], value[2]));
				float diff = 255.0 / maxValue;
				value[0] *= diff;
				value[1] *= diff;
				value[2] *= diff;

				buffer += fmt::format("\x1b[38;2;{};{};{}m{}", value[2], value[1], value[0], sample_array(g, asciiChars));
			};
			else if(colorMode == COLOR) transformer = [](cv::uint8_t g, cv::Vec3b value, std::string &buffer) {
				const std::vector<char> asciiChars = { ' ', '.', '\"', ',', ':', '-', '~', '=', '|', '(', '{', '[', '&', '#', '@' };

				buffer += fmt::format("\x1b[38;5;{}m{}", 16 + value[0]/42 + value[1]/42*6 + value[2]/42*36, sample_array(g, asciiChars));
			};
			else transformer = [](cv::uint8_t value, cv::Vec3b, std::string &buffer) {
				const std::vector<char> asciiChars = { ' ', '.', '\"', ',', ':', '-', '~', '=', '|', '(', '{', '[', '&', '#', '@' };

				buffer += sample_array(value, asciiChars);
			};
			break;
	}
	
	// Start music
	std::future<void> playMusic(std::async(std::launch::async,
		[videoPath]()
		{
			std::string cmd = fmt::format("mplayer -vo null -slave \"{}\" {}", videoPath, "> /dev/null");
			system(cmd.c_str());
		}
	));
	
	auto updateDelay = 1000000us*1000 / static_cast<long>(1000 * cap.get(cv::CAP_PROP_FPS));
	auto startTime = now<std::chrono::microseconds>();
	// Clear console
	std::cout << "\x1b[2J";
	
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ); // Set stdout to be fully buffered

	std::string buffer;
	while(true)
	{
		auto elapsedTime = ( now<std::chrono::microseconds>() - startTime );
		int index = elapsedTime / updateDelay;

		for(int k = 1; k < index; k++)
		{
			if(k >= index-1)
			{
				cv::Mat frame, grayFrame;

				if(!cap.read(frame)) goto endLoop;

				cv::resize(frame, frame, cv::Size(width, height), 0., 0., cv::INTER_AREA);
				
				grayFrame = frame.clone();
				cv::cvtColor(grayFrame, grayFrame, cv::COLOR_BGR2GRAY);
				
				fmt::print("\x1b[1;1H");
				
				for(int j = 0; j < frame.rows; j++)
				{
					buffer.clear();
					for(int i = 0; i < frame.cols; i++)
					{
						uint8_t gray = grayFrame.at<uint8_t>(j, i);
						cv::Vec3b value = frame.at<cv::Vec3b>(j, i);

						transformer(gray, value, buffer);
					}

					buffer += '\n';
					fmt::print("{}", buffer);
				}
				
				fmt::print("\x1b[0m");
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
	
	playMusic.get();
	
	return 0;
}
