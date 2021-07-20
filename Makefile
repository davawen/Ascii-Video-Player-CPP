CXX = g++
CXXFLAGS = -c -Wall -Wno-unknown-pragmas
CXXLINKFLAGS = -pthread

EXEC_NAME = main

SRC_FILES = main.cpp

LIBS = opencv_core opencv_highgui opencv_imgcodecs opencv_imgproc opencv_videoio

LIB_PATH = /usr/local/lib64
INCLUDE_PATH = /usr/local/include/opencv4/

# The rest is madness

SRC_FILES := $(addprefix src/,$(SRC_FILES))
OBJ_FILES := $(SRC_FILES:src/%.cpp=build/obj/%.o)

LIBS := $(addprefix -l,$(LIBS))
LIB_PATH := $(addprefix -L,$(LIB_PATH))
INCLUDE_PATH := $(addprefix -I,$(INCLUDE_PATH))

all: prog
	./build/bin/$(EXEC_NAME)

prog: $(OBJ_FILES)
	$(CXX) $(CXXLINKFLAGS) $(LIB_PATH) $(LIBS) $^ -o build/bin/$(EXEC_NAME)

build/obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -o $@ $< 

clean:
	rm -rf build/bin/$(EXEC_NAME) build/obj/*.o

init:
	mkdir -p build/bin build/obj

test:
	@echo Source Files: $(SRC_FILES)
	@echo Object Files: $(OBJ_FILES)
	@echo Libs: $(LIBS)
	@echo Lib Path: $(LIB_PATH)
	@echo Include Path: $(INCLUDE_PATH)
	@echo Executable Name: $(EXEC_NAME)
	@echo Compiler: $(CXX) $(CXXFLAGS)
