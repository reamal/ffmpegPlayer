#include "com_example_fplayer_jni_PlayControl.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <android/log.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libavformat/avformat.h>
#include <ffmpeg/libswscale/swscale.h>
#include <ffmpeg/libswresample/swresample.h>

#include <libyuv.h>
#include "queue.h"




#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jniLog",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jniLog",FORMAT,##__VA_ARGS__);

#define MAX_STREAM 2
#define PACKET_QUEUE_SIZE 100
#define MAX_AUDIO_FRME_SIZE 48000 * 4
typedef struct _Player Player;
typedef struct _DecoderData DecoderData;

pthread_mutex_t mutex;
pthread_cond_t has_cond;

struct _Player {

	JavaVM* javaVm;
	//封装格式上下文
	AVFormatContext *av_format_context;
	//音频视频流索引位置
	int video_stream_index;
	int audio_stream_index;

	//流的总个数
	int captrue_streams_no;

	//解码器上下文数组
	AVCodecContext *input_codec_ctx[MAX_STREAM];

	//解码线程ID
	pthread_t decode_threads[MAX_STREAM];
	ANativeWindow* nativeWindow;

	/**
	 * 音频解码相关数据
	 */
	SwrContext *swr_ctx;
	//输入的采样格式
	enum AVSampleFormat in_sample_fmt;
	//输出采样格式16bit PCM
	enum AVSampleFormat out_sample_fmt;
	//输入采样率
	int in_sample_rate;
	//输出采样率
	int out_sample_rate;
	//输出的声道个数
	int out_channel_nb;

	//JNI
	jobject audio_track;
	jmethodID audio_track_write_mid;

	pthread_t thread_read_from_stream; //读取pkt的线程ID
	Queue *packets[MAX_STREAM];
};

struct _DecoderData {
	Player *player;
	int stream_index;
};

void init_format_context(const char* c_inputstr, Player* player) {
	//注册
	av_register_all();
	//数据格式
	AVFormatContext* av_fromat_context = avformat_alloc_context();
	//打开视频输入文件
	int open_result = avformat_open_input(&av_fromat_context, c_inputstr, NULL,
			NULL);
	if (open_result != 0) {
		LOGE("%s", "视频输入流打开失败！！！");
	} else {
		LOGI("%s", "视频输入流打开success ！");
	}

	//获取输入视频的相关信息。
	int info_result = avformat_find_stream_info(av_fromat_context, NULL);
	if (info_result < 0) {
		LOGE("%s", "视频输入流获取信息失败 ！！！");
	} else {
		LOGI("%s", "视频输入流获取信息success ！");
	}

	player->captrue_streams_no = av_fromat_context->nb_streams;
	//获取到视频流的索引。
	int i = 0;
	for (; i < player->captrue_streams_no; i++) {
		if (av_fromat_context->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_VIDEO) {
			player->video_stream_index = i;
		} else if (av_fromat_context->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			player->audio_stream_index = i;
		}
	}

	player->av_format_context = av_fromat_context;
}

void init_codec_context(Player * player, int stream_index) {
	//解码器
	AVCodecContext *av_codec_context =
			player->av_format_context->streams[stream_index]->codec;
	AVCodec * av_codec = avcodec_find_decoder(av_codec_context->codec_id);
	if (av_codec == NULL) {
		LOGE("%s", "获取解码器失败！！！");
	} else {
		LOGI("%s", "获取解码器 success ！");
	}

	//打开解码器
	int codec_open_result = avcodec_open2(av_codec_context, av_codec, NULL);
	if (codec_open_result != 0) {
		LOGE("%s : %d", "打开解码器失败！！！", codec_open_result);
	} else {
		LOGI("%s", "打开解码器 success ！");
	}

	player->input_codec_ctx[stream_index] = av_codec_context;
}

void decode_video_prepare(JNIEnv *env, Player *player, jobject surface) {
	//加载窗口
	player->nativeWindow = ANativeWindow_fromSurface(env, surface);
}

void decode_audio_perpare(Player* player, int stream_index) {
	//解码器
	AVCodecContext *codec_ctx =
			player->input_codec_ctx[player->audio_stream_index];

	//重采样设置参数-------------start
	//输入的采样格式
	enum AVSampleFormat in_sample_fmt = codec_ctx->sample_fmt;
	//输出采样格式16bit PCM
	enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	//输入采样率
	int in_sample_rate = codec_ctx->sample_rate;
	//输出采样率
	int out_sample_rate = in_sample_rate;
	//获取输入的声道布局
	//根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
	//av_get_default_channel_layout(codecCtx->channels);
	uint64_t in_ch_layout = codec_ctx->channel_layout;
	//输出的声道布局（立体声）
	uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

	//frame->16bit 44100 PCM 统一音频采样格式与采样率
	SwrContext *swr_ctx = swr_alloc();
	swr_alloc_set_opts(swr_ctx, out_ch_layout, out_sample_fmt, out_sample_rate,
			in_ch_layout, in_sample_fmt, in_sample_rate, 0, NULL);
	swr_init(swr_ctx);

	//输出的声道个数
	int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

	//重采样设置参数-------------end

	player->in_sample_fmt = in_sample_fmt;
	player->out_sample_fmt = out_sample_fmt;
	player->in_sample_rate = in_sample_rate;
	player->out_sample_rate = out_sample_rate;
	player->out_channel_nb = out_channel_nb;
	player->swr_ctx = swr_ctx;
}

void jni_audio_prepare(JNIEnv *env, jobject jthiz, Player *player) {
	//JNI begin------------------
	//JasonPlayer
	jclass player_class = (*env)->GetObjectClass(env, jthiz);

	//AudioTrack对象
	jmethodID create_audio_track_mid = (*env)->GetMethodID(env, player_class,
			"createAudioTrack", "(II)Landroid/media/AudioTrack;");
	jobject audio_track = (*env)->CallObjectMethod(env, jthiz,
			create_audio_track_mid, player->out_sample_rate,
			player->out_channel_nb);

	//调用AudioTrack.play方法
	jclass audio_track_class = (*env)->GetObjectClass(env, audio_track);
	jmethodID audio_track_play_mid = (*env)->GetMethodID(env, audio_track_class,
			"play", "()V");
	(*env)->CallVoidMethod(env, audio_track, audio_track_play_mid);

	//AudioTrack.write
	jmethodID audio_track_write_mid = (*env)->GetMethodID(env,
			audio_track_class, "write", "([BII)I");

	//JNI end------------------
	player->audio_track = (*env)->NewGlobalRef(env, audio_track);
	//(*env)->DeleteGlobalRef
	player->audio_track_write_mid = audio_track_write_mid;
}
;

/**
 * 解码子线程函数
 */
void decode_data(void* arg) {

	DecoderData *decoder_data = (DecoderData*) arg;
	Player *player = decoder_data->player;
	int stream_index = decoder_data->stream_index;
	//根据stream_index获取对应的AVPacket队列
	Queue *queue = player->packets[stream_index];
	AVFormatContext *format_ctx = player->av_format_context;
	//编码数据
	//6.一阵一阵读取压缩的视频数据AVPacket
	int video_frame_count = 0, audio_frame_count = 0;

	int result_video = 0;
	int result_audio = 0;
	for (;;) {

		while (get_ready(queue) <= 0) {
			pthread_cond_wait(&has_cond, &mutex);
			LOGI("%s : %d", "pthread_cond_wait after",stream_index);
		}

		if (get_ready(queue) <= 0) {
			continue;
		}

		//消费AVPacket
		AVPacket *packet = (AVPacket*) queue_pop(queue);
		if (stream_index == player->video_stream_index && !result_video) {
			result_video = decode_video(player, packet);
		} else if (stream_index == player->audio_stream_index
				&& !result_audio) {
			result_audio = decode_audio(player, packet);
		}

		packet_free_func(packet);

		if (result_video && result_audio) {
			break;
		}
	}
}
;

int decode_video(Player* player, AVPacket *packet) {
	//像素数据。
	AVFrame * av_frame_yuv = av_frame_alloc();
	AVFrame * av_frame_rgb = av_frame_alloc();

	//绘制缓冲区
	ANativeWindow_Buffer buffer;

	int got_frame = 0;

	int ret = avcodec_decode_video2(
			player->input_codec_ctx[player->video_stream_index], av_frame_yuv,
			&got_frame, packet);
	if (ret < 0) {
		return 1;
	}
	if (got_frame) {
		//设置缓冲区的属性（宽、高、像素格式）
		ANativeWindow_setBuffersGeometry(player->nativeWindow,
				player->input_codec_ctx[player->video_stream_index]->width,
				player->input_codec_ctx[player->video_stream_index]->height,
				WINDOW_FORMAT_RGB_565);
		ANativeWindow_lock(player->nativeWindow, &buffer, NULL);

		//设置rgb_frame的属性（像素格式、宽高）和缓冲区
		//rgb_frame缓冲区与outBuffer.bits是同一块内存
		avpicture_fill((AVPicture*) av_frame_rgb, buffer.bits, PIX_FMT_RGB565,
				player->input_codec_ctx[player->video_stream_index]->width,
				player->input_codec_ctx[player->video_stream_index]->height);
		I420ToRGB565(av_frame_yuv->data[0], av_frame_yuv->linesize[0],
				av_frame_yuv->data[1], av_frame_yuv->linesize[1],
				av_frame_yuv->data[2], av_frame_yuv->linesize[2],
				av_frame_rgb->data[0], av_frame_rgb->linesize[0],
				player->input_codec_ctx[player->video_stream_index]->width,
				player->input_codec_ctx[player->video_stream_index]->height);

		ANativeWindow_unlockAndPost(player->nativeWindow);
	} else {
		LOGI("%s : %d", "decode_video", ret);
	}

	av_frame_free(&av_frame_yuv);
	av_frame_free(&av_frame_rgb);
	return 0;
}
;
int decode_audio(Player* player, AVPacket *packet) {
	AVCodecContext *codec_ctx =
			player->input_codec_ctx[player->audio_stream_index];
	//解压缩数据
	AVFrame *frame = av_frame_alloc();

	//16bit 44100 PCM 数据（重采样缓冲区）
	uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRME_SIZE);

	int got_frame;

	int ret = avcodec_decode_audio4(codec_ctx, frame, &got_frame, packet);
	if (ret < 0) {
		return 1;
	}
	//解码一帧成功
	if (got_frame > 0) {
		swr_convert(player->swr_ctx, &out_buffer, MAX_AUDIO_FRME_SIZE,
				(const uint8_t **) frame->data, frame->nb_samples);
		//获取sample的size
		int out_buffer_size = av_samples_get_buffer_size(NULL,
				player->out_channel_nb, frame->nb_samples,
				player->out_sample_fmt, 1);

		//关联当前线程的JNIEnv
		JavaVM *javaVM = player->javaVm;
		JNIEnv *env;
		(*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

		//out_buffer缓冲区数据，转成byte数组
		jbyteArray audio_sample_array = (*env)->NewByteArray(env,
				out_buffer_size);
		jbyte* sample_bytep = (*env)->GetByteArrayElements(env,
				audio_sample_array, NULL);
		//out_buffer的数据复制到sampe_bytep
		memcpy(sample_bytep, out_buffer, out_buffer_size);
		//同步
		(*env)->ReleaseByteArrayElements(env, audio_sample_array, sample_bytep,
				0);

		//AudioTrack.write PCM数据
		(*env)->CallIntMethod(env, player->audio_track,
				player->audio_track_write_mid, audio_sample_array, 0,
				out_buffer_size);
		//释放局部引用
		(*env)->DeleteLocalRef(env, audio_sample_array);

		(*javaVM)->DetachCurrentThread(javaVM);

	} else {
		LOGI("%s : %d", "decode_audio", ret);
	}
	av_frame_free(&frame);
	return 0;
}
;

/**
 * 给AVPacket开辟空间，后面会将AVPacket栈内存数据拷贝至这里开辟的空间
 */
void* player_fill_packet() {
	//请参照我在vs中写的代码
	AVPacket *packet = malloc(sizeof(AVPacket));
	return packet;
}
;

/**
 * 初始化音频，视频AVPacket队列，长度50
 */
void player_alloc_queues(Player *player) {
	int i;
	//这里，正常是初始化两个队列
	for (i = 0; i < player->captrue_streams_no; ++i) {
		Queue *queue = queue_init(PACKET_QUEUE_SIZE,
				(queue_fill_func) player_fill_packet);
		player->packets[i] = queue;
	}
}
;

void packet_free_func(AVPacket *packet) {
	av_free_packet(packet);
}

/**
 * 生产者线程
 */JNIEXPORT void player_read_from_stream(Player* player) {
	int ret;
	AVPacket packet, *pkt = &packet;
	for (;;) {
		pthread_mutex_lock(&mutex);
		ret = av_read_frame(player->av_format_context, pkt);
		if (ret < 0) {
			LOGI("%s", "读取packet完成");
			break;
		}
		Queue *queue = player->packets[pkt->stream_index];
		AVPacket* packet_data = queue_push(queue);
		*packet_data = packet;

		pthread_cond_signal(&has_cond);
		LOGI("ready : %d , stream : %d",get_ready(queue),pkt->stream_index);
//		usleep(1000);

		if (get_ready(queue) >= get_size(queue)) {
			for (;;) {
				if (get_ready(queue) < get_size(queue)) {
					break;
				}
			}
		}

		pthread_mutex_unlock(&mutex);

	}
}
;

JNIEXPORT void JNICALL Java_com_example_fplayer_jni_PlayControl_startPlayer(
		JNIEnv *env, jobject jobj, jstring inputstr, jobject surface) {

	const char* c_inputstr = (*env)->GetStringUTFChars(env, inputstr, NULL);
	Player* player = malloc(sizeof(Player));
	(*env)->GetJavaVM(env, &(player->javaVm));

	//初始化音频控件
	init_format_context(c_inputstr, player);
	int video_stream_index = player->video_stream_index;
	int audio_stream_index = player->audio_stream_index;

	init_codec_context(player, video_stream_index);
	init_codec_context(player, audio_stream_index);

	//打开视频解码器
	decode_video_prepare(env, player, surface);
	//打开音频解码器
	decode_audio_perpare(player, player->audio_stream_index);

	jni_audio_prepare(env, jobj, player);

	player_alloc_queues(player);


	pthread_mutex_init(&mutex, NULL);

	pthread_cond_init(&has_cond, NULL);

	//生产者线程
	pthread_create(&(player->thread_read_from_stream), NULL,
			player_read_from_stream, (void*) player);

//	usleep(1000);

	//子线程解码
	DecoderData data1 = { player, player->video_stream_index }, *decoder_data1 =
			&data1;
	pthread_create(&(player->decode_threads[player->video_stream_index]), NULL,
			decode_data, (void*) decoder_data1);

	DecoderData data2 = { player, player->audio_stream_index }, *decoder_data2 =
			&data2;
	pthread_create(&(player->decode_threads[player->audio_stream_index]), NULL,
			decode_data, (void*) decoder_data2);

	pthread_join(player->thread_read_from_stream, NULL);
	pthread_join(player->decode_threads[video_stream_index], NULL);
	pthread_join(player->decode_threads[audio_stream_index], NULL);

	(*env)->ReleaseStringUTFChars(env, inputstr, c_inputstr);
}
;

