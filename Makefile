CC=gcc -g
LIBS= -L /home/hai/FFmpeg-master/libavutil -lavutil  -L /home/hai/FFmpeg-master/libavformat -lavformat -L /home/hai/FFmpeg-master/libavcodec -lavcodec -lz -lm -L /home/hai/FFmpeg-master/libavdevice -lavdevice -L /home/hai/FFmpeg-master/libavfilter -lavfilter -lswscale -L /home/hai/FFmpeg-master/libavcodec -lavcodec  -lSDL -lSDL2
OBJ=tutorial01 tutorial02 tutorial03 tutorial04
all:$(OBJ)

tutorial01:tutorial01.c
	$(CC) tutorial01.c -o tutorial01 $(LIBS)
tutorial02:tutorial02.c
	$(CC) tutorial02.c -o tutorial02 $(LIBS)
tutorial03:tutorial03.c
	$(CC) tutorial03.c -o tutorial03 $(LIBS)
tutorial04:tutorial04.c
	$(CC) tutorial04.c -o tutorial04 $(LIBS)
.PHONY : clean
clean :
	rm $(OBJ)
