// tutorial02.c
// A pedagogical video player that will stream through every video frame as fast as it can.
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
//
// Use the Makefile to build all examples.
//
// Run using
// tutorial02 myvideofile.mpg
//
// to play the video stream on your screen.


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

int
randomInt(int min, int max)
{
    return min + rand() % (max - min + 1);
}

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;
    int             i, videoStream;
    AVCodecContext  *pCodecCtx = NULL;
    AVCodec         *pCodec = NULL;
    AVFrame         *pFrame = NULL;
    AVPacket        packet;
    int             frameFinished;
    //float           aspect_ratio;

    AVDictionary    *optionsDict = NULL;
    struct SwsContext *sws_ctx = NULL;
    //SDL_CreateTexture();
    SDL_Texture    *bmp = NULL;
    SDL_Window     *screen = NULL;
    SDL_Rect        rect;
    SDL_Event       event;

    if(argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }

    av_register_all();

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    //打开文件
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
        return -1; // Couldn't open file

    //读取信息
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
        return -1;

    // 打印日志
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // 查找视频流
    videoStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            videoStream=i;
            break;
        }
    if(videoStream==-1)
        return -1;

    // 解码器ctx
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;

    // 查找解码器
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    // 打开（注册）解码器
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
        return -1; // Could not open codec

    // 开辟一帧存储
    pFrame=av_frame_alloc();//原avcodec_alloc_frame弃用
    AVFrame* pFrameYUV = av_frame_alloc();
    if( pFrameYUV == NULL )
        return -1;

    /*  创建一个视频播放的窗口
     *  title	：窗口标题
     *   x	：窗口位置x坐标。也可以设置为SDL_WINDOWPOS_CENTERED或SDL_WINDOWPOS_UNDEFINED。
     *   y	：窗口位置y坐标。同上。
     *   w	：窗口的宽
     *   h	：窗口的高
     *   flags ：支持下列标识。包括了窗口的是否最大化、最小化，能否调整边界等等属性。
     *          ::SDL_WINDOW_FULLSCREEN,    ::SDL_WINDOW_OPENGL,
     *          ::SDL_WINDOW_HIDDEN,        ::SDL_WINDOW_BORDERLESS,
     *          ::SDL_WINDOW_RESIZABLE,     ::SDL_WINDOW_MAXIMIZED,
     *          ::SDL_WINDOW_MINIMIZED,     ::SDL_WINDOW_INPUT_GRABBED,
     *          ::SDL_WINDOW_ALLOW_HIGHDPI.
     *   返回创建完成的窗口的ID。如果创建失败则返回0 */
    screen = SDL_CreateWindow("My Game Window",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              pCodecCtx->width,  pCodecCtx->height,
                              SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
    /*基于窗口创建渲染器
    * window	： 渲染的目标窗口。
    * index	：打算初始化的渲染设备的索引。设置“-1”则初始化默认的渲染设备。
    * flags	：支持以下值（位于SDL_RendererFlags定义中）
    *    SDL_RENDERER_SOFTWARE ：使用软件渲染
    *    SDL_RENDERER_ACCELERATED ：使用硬件加速
    *    SDL_RENDERER_PRESENTVSYNC：和显示器的刷新率同步
    *    SDL_RENDERER_TARGETTEXTURE ：不太懂
    * 返回创建完成的渲染器的ID。如果创建失败则返回NULL。    */
    SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);


    if(!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }

    /*  基于渲染器创建一个纹理
        renderer：目标渲染器。
        format	：纹理的格式。
        access	：可以取以下值（定义位于SDL_TextureAccess中）
            SDL_TEXTUREACCESS_STATIC	：变化极少
            SDL_TEXTUREACCESS_STREAMING	：变化频繁
            SDL_TEXTUREACCESS_TARGET	：暂时没有理解
        w		：纹理的宽
        h		：纹理的高
        创建成功则返回纹理的ID，失败返回0。*/
    bmp = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);


    sws_ctx =
    sws_getContext
    (
     pCodecCtx->width,
     pCodecCtx->height,
     pCodecCtx->pix_fmt,
     pCodecCtx->width,
     pCodecCtx->height,
     AV_PIX_FMT_YUV420P,//原PIX_FMT_YUV420P弃用
     SWS_BILINEAR,
     NULL,
     NULL,
     NULL
     );
    // 跟据解码器contex信息获取一帧图像的位图大小并开辟空间
    int numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                                  pCodecCtx->height);
    uint8_t* buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    // 为AVPicture挂上一段用于保存数据的空间，AVPicture是AVFrame的子类
    avpicture_fill((AVPicture *)pFrameYUV, buffer, AV_PIX_FMT_YUV420P,
                   pCodecCtx->width, pCodecCtx->height);

    i=0;

    rect.x = 0;
    rect.y = 0;
    rect.w = pCodecCtx->width;
    rect.h = pCodecCtx->height;
    //读取一帧
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
                                  &packet);

            // Did we get a video frame?
            if(frameFinished) {
                // 转yuv
                sws_scale
                (
                 sws_ctx,
                 (uint8_t const * const *)pFrame->data,
                 pFrame->linesize,
                 0,
                 pCodecCtx->height,
                 pFrameYUV->data,
                 pFrameYUV->linesize
                 );
                 /* 设置纹理的数据
                    texture：目标纹理。
                    rect：更新像素的矩形区域。设置为NULL的时候更新整个区域。
                    pixels：像素数据。
                    pitch：一行像素数据的字节数。
                    成功的话返回0，失败的话返回-1。*/
                SDL_UpdateTexture( bmp, &rect, pFrameYUV->data[0], pFrameYUV->linesize[0] );
                SDL_RenderClear( renderer );
                /*  将纹理数据复制给渲染目标
                    renderer：渲染目标。
                    texture：输入纹理。
                    srcrect：选择输入纹理的一块矩形区域作为输入。设置为NULL的时候整个纹理作为输入。
                    dstrect：选择渲染目标的一块矩形区域作为输出。设置为NULL的时候整个渲染目标作为输出。*/
                SDL_RenderCopy( renderer, bmp, &rect, &rect );
                //显示画面
                SDL_RenderPresent( renderer );
            }
            SDL_Delay(50);//同sleep函数
        }

        //进行释放等close处理
        av_free_packet(&packet);
        SDL_PollEvent(&event);
        switch(event.type) {
            case SDL_QUIT:
                SDL_Quit();
                exit(0);
                break;
            default:
                break;
        }

    }

    SDL_DestroyTexture(bmp);

    // Free the YUV frame
    av_free(pFrame);
    av_free(pFrameYUV);
    // Close the codec
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}

