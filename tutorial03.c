// tutorial03.c
// 添加播放音频


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
#include <assert.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct PacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

PacketQueue audioq;

int quit = 0;
//队列初始化
void packet_queue_init(PacketQueue *q) {
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}
//将AVPacket放入队列
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

	AVPacketList *pkt1;
	if(av_dup_packet(pkt) < 0) {
		return -1;
	}
	pkt1 = av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;


	SDL_LockMutex(q->mutex);

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);
	return 0;
}
//从队列取出一个AVPacket
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for(;;) {

		if(quit) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1)//如果队列非空
		{
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block)//如果没有数据，是否阻塞等待
		{
			ret = 0;
			break;
		}
		else
		{
			//解锁，阻塞等待数据到来。
            SDL_CondWait(q->cond, q->mutex); /*FIXME:  现在是单线程，为什么整个程序并没有阻塞
                                               已经解决：音频播放是另一个线程并非单线程*/

		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

//解码，从队列里读取数据将数据放入audio_buf中，audio_buf的大小为buf_size
int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {

	static AVPacket pkt;
	static uint8_t *audio_pkt_data = NULL;
	static int audio_pkt_size = 0;
	static AVFrame frame;

	int len1, data_size = 0;

	for(;;)
	{
		while(audio_pkt_size > 0) //如果从队列中读取到的包的长度
		{
			int got_frame = 0;

			/*
			 * aCodecCtx:音频context
			 * frame:解码后的音频Frame
			 * got_frame:如果是0表示没有Frame可以解码了
			 * pkt：需要进行解码的Packet
			 * return:解码消耗的pkt中的data的长度，失败则返回错误码
			 */
			len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
			if(len1 < 0)
			{
				/* 解码出错 */
				audio_pkt_size = 0;
				break;
			}
			audio_pkt_data += len1;
			audio_pkt_size -= len1;//len1与audio_pkt_size相等  FIXME:有没有情况len1<audio_pkt_size
			data_size = 0;
			if(got_frame)//解码成功，并且解出数据
			{
				//计算音频数据(解码出的frame的数据域)需要占用的空间大小
				data_size = av_samples_get_buffer_size(NULL,
						aCodecCtx->channels,
						frame.nb_samples,
						aCodecCtx->sample_fmt,
						1);
				assert(data_size <= buf_size);
				memcpy(audio_buf, frame.data[0], data_size);
			}
			if(data_size <= 0)
			{
				continue;
			}
			//返回解码数据的长度
			return data_size;
		}
		if(pkt.data)//pkt中有数据
			av_free_packet(&pkt);

		if(quit)
		{
			return -1;
		}

		if(packet_queue_get(&audioq, &pkt, 1) < 0) //从队列中读取数据
		{
			return -1;
		}
		audio_pkt_data = pkt.data;//包中数据
		audio_pkt_size = pkt.size;//包中数据长度
	}
}

void audio_callback(void *userdata, Uint8 *stream, int len) {

	AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
	int len1, audio_size;

	static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;//解码出的音频长度
    static unsigned int audio_buf_index = 0;//stream中 和 音频缓存buf 当前指针位置

    while(len > 0)
    {
		if(audio_buf_index >= audio_buf_size)
		{
            /* 解码一个packet到audio_buf中 */
			audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
			if(audio_size < 0)
			{
                /* 如果出错输出静音 */
				audio_buf_size = 1024; // arbitrary?
				memset(audio_buf, 0, audio_buf_size);
			}
			else
			{
				audio_buf_size = audio_size;
			}
			audio_buf_index = 0;
		}
		len1 = audio_buf_size - audio_buf_index;
		if(len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
		len -= len1;
		stream += len1;
		audio_buf_index += len1;
	}
}

int main(int argc, char *argv[]) {
	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream, audioStream;
	AVCodecContext  *pCodecCtxOrig = NULL;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	AVPacket        packet;
	int             frameFinished;
	struct SwsContext *sws_ctx = NULL;

	AVCodecContext  *aCodecCtxOrig = NULL;
	AVCodecContext  *aCodecCtx = NULL;
	AVCodec         *aCodec = NULL;

	SDL_Texture     *bmp;
	SDL_Window     *screen;
	SDL_Rect        rect;
	SDL_Event       event;
	SDL_AudioSpec   wanted_spec, spec;

	if(argc < 2) {
		fprintf(stderr, "Usage: test <file>\n");
		exit(1);
	}
	av_register_all();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
		return -1;

	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1;

	av_dump_format(pFormatCtx, 0, argv[1], 0);

	videoStream=-1;
	audioStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO &&
				videoStream < 0) {
			videoStream=i;
		}
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
				audioStream < 0) {
			audioStream=i;
		}
	}
	if(videoStream==-1)
		return -1;
	if(audioStream==-1)
		return -1;

	aCodecCtxOrig=pFormatCtx->streams[audioStream]->codec;
	//查找音频解码器
	aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
	if(!aCodec) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	// 复制Context
	aCodecCtx = avcodec_alloc_context3(aCodec);
	if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}

	// 设置SDL音频播放
	wanted_spec.freq = aCodecCtx->sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = aCodecCtx->channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;//音频回调函数
	wanted_spec.userdata = aCodecCtx;

	if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
		return -1;
	}
	//注册音频解码器
	avcodec_open2(aCodecCtx, aCodec, NULL);

	// 初始化队列
	packet_queue_init(&audioq);
    //如果参数是非0值就暂停，如果是0值就播放。
	SDL_PauseAudio(0);

	// 视频解码器context
	pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;

	// 查找视频解码器
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}

	// 复制视频解码器context
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}

	// 打开视频解码器
	if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
		return -1; // Could not open codec

	// 开辟video frame
	pFrame=av_frame_alloc();
	AVFrame* pFrameYUV = av_frame_alloc();
	if( pFrameYUV == NULL )
		return -1;
	/*  创建一个视频播放的窗口 */
	screen = SDL_CreateWindow("My Game Window",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			pCodecCtx->width,  pCodecCtx->height,
			SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	/*基于窗口创建渲染器*/
	SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);
	if(!screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	/*  基于渲染器创建一个纹理*/
	bmp = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_YV12,SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);

	// 格式转换
	sws_ctx = sws_getContext(pCodecCtx->width,
			pCodecCtx->height,
			pCodecCtx->pix_fmt,
			pCodecCtx->width,
			pCodecCtx->height,
			AV_PIX_FMT_YUV420P,
			SWS_BILINEAR,
			NULL,
			NULL,
			NULL
			);
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
	while(av_read_frame(pFormatCtx, &packet)>=0) {
		if(packet.stream_index==videoStream)
		{
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
					&packet);
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
				/* 设置纹理的数据。*/
				SDL_UpdateTexture( bmp, &rect, pFrameYUV->data[0], pFrameYUV->linesize[0] );
				SDL_RenderClear( renderer );
				/*  将纹理数据复制给渲染目标*/
				SDL_RenderCopy( renderer, bmp, &rect, &rect );
				//显示画面
				SDL_RenderPresent( renderer );
			}
		}
		else if(packet.stream_index==audioStream)
		{
			//将包放入队列
			packet_queue_put(&audioq, &packet);
		}
		else
		{
			av_free_packet(&packet);
		}
		// Free the packet that was allocated by av_read_frame
		SDL_PollEvent(&event);
		switch(event.type) {
			case SDL_QUIT:
				quit = 1;
				SDL_Quit();
				exit(0);
				break;
			default:
				break;
		}

	}

	av_frame_free(&pFrame);

	avcodec_close(pCodecCtxOrig);
	avcodec_close(pCodecCtx);
	avcodec_close(aCodecCtxOrig);
	avcodec_close(aCodecCtx);

	avformat_close_input(&pFormatCtx);

	return 0;
}
