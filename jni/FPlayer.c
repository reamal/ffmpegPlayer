#include "com_example_fplayer_jni_PlayControl.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>   //线程
#include <android/log.h>
#include <unistd.h>
#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libavformat/avformat.h>
#include <ffmpeg/libswscale/swscale.h>
#include <ffmpeg/libswresample/swresample.h>

#include <libyuv.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jniLog",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jniLog",FORMAT,##__VA_ARGS__);

#define MAX_STREAM 2
#define MAX_AUDIO_FRME_SIZE 48000 * 4

struct Player {
	//封装格式上下文
	AVFormatContext *av_fromat_context;
	//音频视频流索引位置
	int video_stream_index;
	int audio_stream_index;
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
};

int init_format_context(const char* c_inputstr, struct Player* player) {
	//注册
	av_register_all();
	//数据格式
	AVFormatContext* av_fromat_context = avformat_alloc_context();
	//打开视频输入文件
	int open_result = avformat_open_input(&av_fromat_context, c_inputstr, NULL,
			NULL);
	if (open_result != 0) {
		LOGE("%s", "视频输入流打开失败！！！");
		return 0;
	} else {
		LOGI("%s", "视频输入流打开success ！");
	}

	//获取输入视频的相关信息。
	int info_result = avformat_find_stream_info(av_fromat_context, NULL);
	if (info_result < 0) {
		LOGE("%s", "视频输入流获取信息失败 ！！！");
		return 0;
	} else {
		LOGI("%s", "视频输入流获取信息success ！");
	}

	//获取到视频流的索引。
	int i = 0;
	for (; i < av_fromat_context->nb_streams; i++) {
		if (av_fromat_context->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_VIDEO) {
			player->video_stream_index = i;
		} else if (av_fromat_context->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			player->audio_stream_index = i;
		}
	}

	player->av_fromat_context = av_fromat_context;
	return 1;
}

int decode_stream_prepare(struct Player* player, int stream_index) {
	//解码器
	AVCodecContext *av_codec_context =
			player->av_fromat_context->streams[stream_index]->codec;
	AVCodec * av_codec = avcodec_find_decoder(av_codec_context->codec_id);
	if (av_codec == NULL) {
		LOGE("%s", "获取解码器失败！！！");
		return 0;
	} else {
		LOGI("%s", "获取解码器 success ！");
	}

	//打开解码器
	int codec_open_result = avcodec_open2(av_codec_context, av_codec, NULL);
	if (codec_open_result != 0) {
		LOGE("%s : %d", "打开解码器失败！！！", codec_open_result);
		return 0;
	} else {
		LOGI("%s", "打开解码器 success ！");
	}

	player->input_codec_ctx[stream_index] = av_codec_context;
	return 1;
}

/**
 * 解码子线程函数
 */
void* decode_data(void* arg) {
	struct Player *player = (struct Player*) arg;
	AVFormatContext *format_ctx = player->av_fromat_context;
	//编码数据
	AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
	//6.一阵一阵读取压缩的视频数据AVPacket
	int video_frame_count = 0;
	while (av_read_frame(format_ctx, packet) >= 0) {
		if (packet->stream_index == player->video_stream_index) {
			decode_video(player, packet); //解码一帧视频
			decode_audio(player, packet); //解码一帧音频
			LOGI("video_frame_count:%d", video_frame_count++);
			av_free_packet(packet);
		}

	}

	LOGI("解码结束！！");
	ANativeWindow_release(player->nativeWindow);
	avcodec_close(player->input_codec_ctx[player->video_stream_index]);
	avformat_free_context(player->av_fromat_context);
	return arg;
}

void decode_video(struct Player* player, AVPacket *packet) {

	//像素数据。
	AVFrame * av_frame_yuv = av_frame_alloc();
	AVFrame * av_frame_rgb = av_frame_alloc();

	//绘制缓冲区
	ANativeWindow_Buffer buffer;

	int got_frame = 0;

	avcodec_decode_video2(player->input_codec_ctx[player->video_stream_index],
			av_frame_yuv, &got_frame, packet);

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
	}

	av_free_packet(packet);
	av_frame_free(&av_frame_yuv);
	av_frame_free(&av_frame_rgb);
	return;

}
;
void decode_audio(struct Player* player, AVPacket *packet) {

}
;

JNIEXPORT void JNICALL Java_com_example_fplayer_jni_PlayControl_startPlayer(
		JNIEnv *env, jobject jobj, jstring inputstr, jobject surface) {

	const char* c_inputstr = (*env)->GetStringUTFChars(env, inputstr, NULL);
	struct Player* player = malloc(sizeof(struct Player));

	//初始化音频控件
	int init_result = init_format_context(c_inputstr, player);
	if (init_result == 0) {
		LOGE("%s", "init_format_context 中间出现错误！");
		return;
	};
	//打开视频解码器
	int video_prepare = decode_stream_prepare(player,
			player->video_stream_index);
	if (video_prepare == 0) {
		LOGE("%s", "decode_stream_prepare 中间出现错误！");
		return;
	};
	//打开音频解码器
	int video_prepare = decode_stream_prepare(player,
			player->audio_stream_index);
	if (video_prepare == 0) {
		LOGE("%s", "decode_stream_prepare 中间出现错误！");
		return;
	};
	//加载窗口
	player->nativeWindow = ANativeWindow_fromSurface(env, surface);
	//子线程解码
	pthread_create(&(player->decode_threads[player->video_stream_index]), NULL,
			decode_data, (void*) player);

	(*env)->ReleaseStringUTFChars(env, inputstr, c_inputstr);
}
;
