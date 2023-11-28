TARGET_DIR = ./bin/
TARGET_NAME = toy_system
TARGET = $(addprefix $(TARGET_DIR), $(TARGET_NAME))

SYSTEM_DIR = ./system/
SYSTEM_FILES = system_server.c
SYSTEM_SRCS = $(addprefix $(SYSTEM_DIR), $(SYSTEM_FILES))

UI_DIR = ./ui/
UI_FILES = gui.c input.c
UI_SRCS = $(addprefix $(UI_DIR), $(UI_FILES))

WEB_SERVER_DIR = ./web_server/
WEB_SERVER_FILES = web_server.c
WEB_SERVER_SRCS = $(addprefix $(WEB_SERVER_DIR), $(WEB_SERVER_FILES))

SRC_FILES = main.c \
	$(SYSTEM_SRCS) \
	$(UI_SRCS) \
	$(WEB_SERVER_SRCS)

SRCS = $(addprefix $(SRC_DIR), $(SRC_FILES))
OBJS = $(SRCS:.c=.o)

HAL_DIR = ./hal/
HAL_FILES = hardware.c
CXX_SRCS = $(addprefix $(HAL_DIR), $(HAL_FILES))
CXX_OBJS = $(CXX_SRCS:.c=.o)

INC = -I$(SYSTEM_DIR) -I$(UI_DIR) -I$(WEB_SERVER_DIR) -I$(HAL_DIR) -I./ -I$(HAL_DIR)toy -I$(HAL_DIR)oem
CXX_INC = -I$(HAL_DIR) -I$(HAL_DIR)toy -I$(HAL_DIR)oem

LIB = -lpthread
CFLAGS = -Wall -O $(INC) -g $(LIB)

CXXLIBS = -lpthread -lm -lrt
CXXFLAGS = -Wall $(CXX_INC) -g $(CXXLIBS)

CC = gcc
CXX = g++

RM = rm -fr


.cpp .c .o:
	$(CXX) $(CXXFLAGS) -c
	$(CC) $(CFLAGS) -c 
 
all: $(TARGET) libcamera.toy.so libcamera.oem.so

$(TARGET): $(OBJS) $(CXX_OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(CXX_OBJS) -o $@

clean:
	$(RM) $(OBJS) $(CXX_OBJS) core

fclean: clean 
	$(RM) $(TARGET) libcamera.toy.so libcamera.oem.so libcamera.so

re: fclean all 

.PHONY: libcamera.oem.so
libcamera.oem.so:
	$(CXX) -g -shared -fPIC -o libcamera.oem.so $(CXXFLAGS) $(HAL_DIR)oem/camera_HAL_oem.cpp $(HAL_DIR)oem/ControlThread.cpp

.PHONY: libcamera.toy.so
libcamera.toy.so:
	$(CXX) -g -shared -fPIC -o libcamera.toy.so $(CXXFLAGS) $(HAL_DIR)toy/camera_HAL_toy.cpp $(HAL_DIR)toy/ControlThread.cpp

.PHONY: re fclean clean all 