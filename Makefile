CC=clang
CFLAGS=-shared -I /usr/include/libevdev-1.0
LIBS=-levdev

SRC=$(shell find . -name src/*)

OUTPUT=libmagicts.so

${OUTPUT}: ${SRC}
	${CC} ${CFLAGS} -o ${OUTPUT} ${LIBS}

