CC=clang
CFLAGS=-shared -fPIC -I /usr/include/libevdev-1.0
LIBS=-levdev

SRC=$(shell find src -name *.cpp -o -name *.h)

OUTPUT=libmagicts.so

${OUTPUT}: ${SRC}
	${CC} ${CFLAGS} ${SRC} -o ${OUTPUT} ${LIBS}

