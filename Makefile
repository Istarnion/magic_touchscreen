CC=clang
CFLAGS=-shared -fPIC -I /usr/include/libevdev-1.0
LIBS=-levdev

SRC=$(shell find src -name "*.cpp")
HEADERS=$(shell find src -name "*.h")

OUTPUT=libmagicts.so

${OUTPUT}: ${SRC} ${HEADERS}
	${CC} ${CFLAGS} ${SRC} -o $@ ${LIBS}

