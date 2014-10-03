CC := gcc

INCLUDES := $(shell pkg-config --cflags libavformat libavcodec libswscale libavutil sdl)
CFLAGS := -Wall -ggdb
LDFLAGS := $(shell pkg-config --libs libavformat libavcodec libswscale libavutil sdl) -lm -lrt -ljpeg

video2jpg:video2jpg.o
	${CC} video2jpg.o ${CFLAGS} ${INCLUDES} ${LDFLAGS} ${LDFLAGS} -o video2jpg

clean:
	rm -rf *.o
	rm video2jpg
