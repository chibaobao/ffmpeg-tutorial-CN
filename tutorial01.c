// tutorial01.c
//从视频文件中读出前五帧数据写成ppm格式图片保存到当前目录
/*  gcc命令参考： -g -o tutorial01 tutorial01.c   -lavutil   -lavformat
               -lavcodec -lz -lm  -lavdevice  -lavfilter -lswscale
    注意：  想调试ffmpeg的库函数需要.so需要是debug版的*/
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;

  // 打开文件
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  // 写ppm格式图片文件的头信息
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // 按行，一行一行将数据写入文件
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

  // 关闭文件
  fclose(pFile);
}

int main(int argc, char *argv[]) {
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL;
  AVFrame         *pFrameRGB = NULL;
  AVPacket        packet;
  int             frameFinished;
  int             numBytes;
  uint8_t         *buffer = NULL;

  AVDictionary    *optionsDict = NULL;
  struct SwsContext      *sws_ctx = NULL;

  if(argc < 2) {
    printf("Please provide a movie file\n");
    return -1;
  }
  // 注册了所有的文件格式和编解码器的库
  av_register_all();

  // 打开一个视频文件，（ 旧API：av_open_input_file ）
  if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
    return -1; // Couldn't open file

  // 通过检测文集头来检测文件信息，初始化pFormatCtx中的一些信息，如streams[]数组
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information

  //通过log打印读到的文件信息，如streams数组信息，编解码信息
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  // 找到第一个视频流，也就是第一个视频流在streams[]中的下标。（FIXME:也就是说有可能有第二个视频流）
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  if(videoStream==-1)
    return -1; // Didn't find a video stream

  // 得到视频流的解码器context（可以理解为包含关于解码信息的句柄，如解码器名称）
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;

  // 从库中利用解码器信息查找解码器
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
  // 为pCodecCtx打开解码器（API注释：安装解码器），经解码器安装到pCodecCtx
  //optionsDict传入传出参数可以做一些设置，也可以直接传NULL
  if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    return -1; // Could not open codec

  // 开辟一帧存储视频帧
  pFrame=av_frame_alloc();
  if(pFrame==NULL)
    return -1;
  // 开辟一帧存储转换后的RGB帧图像
  pFrameRGB=av_frame_alloc();
  if(pFrameRGB==NULL)
    return -1;

  // 跟据解码器contex信息获取一帧图像的位图大小并开辟空间
  numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                  pCodecCtx->height);
  buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
  //初始化一个SwsContext,libsws用于图片处理如：yuv转rgb
  sws_ctx =
    sws_getContext
    (
        pCodecCtx->width,//源图像的宽
        pCodecCtx->height,//源图像的高
        pCodecCtx->pix_fmt,//源图像的像素格式
        pCodecCtx->width,//目标图像的宽
        pCodecCtx->height,//目标图像的高
        AV_PIX_FMT_RGB24,//目标图像的像素格式
        SWS_BILINEAR,//设定图像拉伸使用的算法
        NULL,
        NULL,
        NULL
    );

  // 为AVPicture挂上一段用于保存数据的空间，AVPicture是AVFrame的子类
  avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
         pCodecCtx->width, pCodecCtx->height);

  // 读取前五帧图像并保存
  i=0;
  //读取一个数据包
  while(av_read_frame(pFormatCtx, &packet)>=0) {
    // 判断数据包类型
    if(packet.stream_index==videoStream) {
      // 解码视频包，将packet解码到pFrame，frameFinished为0表示无数据可以解码
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
               &packet);

      // 判断是否解码到数据
      if(frameFinished) {
        // 转rgb
        sws_scale
        (
            sws_ctx,
            (uint8_t const * const *)pFrame->data,//指向src的bufer
            pFrame->linesize,                     //每一行的比特数
            0,                                    //要处理的数据的起始位置，第几行
            pCodecCtx->height,                    //一共有多少行
            pFrameRGB->data,                      //指向dst的bufer
            pFrameRGB->linesize                   //dst每一行的比特数
        );

    // 存储
    if(++i<=5)
      SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height,
            i);
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }

  // 释放内存，做相应关闭操作
  av_free(buffer);
  av_free(pFrameRGB);
  av_free(pFrame);
  avcodec_close(pCodecCtx);
  avformat_close_input(&pFormatCtx);

  return 0;
}
