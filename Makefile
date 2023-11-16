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

INC = -I$(SYSTEM_DIR) -I$(UI_DIR) -I$(WEB_SERVER_DIR)
LIB = -lpthread

CFLAGS = -Wall -O $(INC) -g $(LIB)
CC = gcc

RM = rm -fr

.c .o :
	$(CC) $(CFLAGS) -c 

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

clean:
	$(RM) $(OBJS) core

fclean: clean 
	$(RM) $(TARGET)

re: fclean all

.PHONY: re fclean clean all