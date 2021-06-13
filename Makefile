CXX = g++
CXXFLAGS = -Wall -Wno-unknown-pragmas -pthread

EXEC_NAME = main

SRC_FILES = main.cpp

LIBS = opencv_core opencv_highgui opencv_imgcodecs opencv_imgproc opencv_videoio

LIB_PATH = /usr/local/lib64
INCLUDE_PATH = /usr/local/include/opencv4/

# The rest is madness

SRC_FILES := $(addprefix src/,$(SRC_FILES))

LIBS := $(addprefix -l,$(LIBS))
LIB_PATH := $(addprefix -L,$(LIB_PATH))
INCLUDE_PATH := $(addprefix -I,$(INCLUDE_PATH))

all: dist prog
	./dist/bin/$(EXEC_NAME)

# Can't use object files apparently for some reason
prog: $(SRC_FILES)
	$(CXX) $(CXXFLAGS) $(SRC_FILES) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) -o dist/bin/$(EXEC_NAME)

clean: dist
	rm -rf dist/bin/$(EXEC_NAME)

dist:
	mkdir dist dist/bin

test_config:
	@echo Source Files: $(SRC_FILES)
	@echo Libs: $(LIBS)
	@echo Lib Path: $(LIB_PATH)
	@echo Include Path: $(INCLUDE_PATH)
	@echo Executable Name: $(EXEC_NAME)
	@echo Compiler: $(CXX) $(CXXFLAGS)