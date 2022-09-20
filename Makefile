#name of output file
NAME = getlink
#build dir
BD = ./build

#Linker flags
LDLIBS +=
LDDIRS += -L$(BD)

#Compiler flags
CFLAGS += -Wall -O2
I += -I./

#Compiler
CC = gcc
AR = ar

#SRC=$(wildcard *.c)
LIBNAME = nl_getlink
SRC_LIB = libnl_getlink.c
SRC_BIN = main.c
SRC = $(SRC_LIB) $(SRC_BIN)

all: $(NAME) static shared

$(NAME): $(SRC)
		$(CC) $(CFLAGS) $(I) $(LDDIRS) $(LDLIBS) $^ -o build/$(NAME)

staticlib:
		$(CC) $(CFLAGS) $(I) $(LDDIRS) $(LDLIBS) $(SRC_LIB) -c -o $(BD)/lib$(LIBNAME).a
		#$(AR) rcs build/lib$(LIBNAME).a build/lib$(LIBNAME).o

sharedlib:
		$(CC) $(CFLAGS) $(I) $(LDDIRS) $(LDLIBS) $(SRC_LIB) -shared -fPIC -o $(BD)/lib$(LIBNAME).so

shared: sharedlib
		$(CC) $(CFLAGS) $(I) $(LDDIRS) $(LDLIBS) $(SRC_BIN) -L./build -Wl,-Bdynamic -l$(LIBNAME) -o $(BD)/$(NAME)_shared

static: staticlib
		$(CC) $(CFLAGS) $(I) $(LDDIRS) $(LDDIRS) $(SRC_BIN) -Wl,-Bstatic -l$(LIBNAME) -Wl,-Bdynamic -o $(BD)/$(NAME)_static

clean:
		rm -rf $(BD)/*
