中文版ffmpeg-tutorial，翻译并加入一些注释
# ffmpeg信息
```
ffmpeg version 3.3.git Copyright (c) 2000-2017 the FFmpeg developers
built with gcc 5.4.0 (Ubuntu 5.4.0-6ubuntu1~16.04.4) 20160609
configuration: --enable-shared --enable-debug --extra-cflags=-g --extra-ldflags=-g
libavutil      55. 61.100 / 55. 61.100
libavcodec     57. 92.100 / 57. 92.100
libavformat    57. 72.100 / 57. 72.100
libavdevice    57.  7.100 / 57.  7.100
libavfilter     6. 84.101 /  6. 84.101
libswscale      4.  7.101 /  4.  7.101
libswresample   2.  8.100 /  2.  8.100
```
# tutorial01.c

从视频文件中读出前五帧数据写成ppm格式图片保存到当前目录
