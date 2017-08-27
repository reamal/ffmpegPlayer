#include "com_example_fplayer_jni_PlayControl.h"
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include <unistd.h>
#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libavformat/avformat.h>
#include <ffmpeg/libswscale/swscale.h>

#include <libyuv.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jniLog",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jniLog",FORMAT,##__VA_ARGS__);

JNIEXPORT void JNICALL Java_com_example_fplayer_jni_PlayControl_startPlayer(
		JNIEnv *env, jobject jobj, jstring inputstr, jobject surface) {

	const char* c_inputstr = (*env)->GetStringUTFChars(env, inputstr, NULL);
	//注册
	av_register_all();
	//数据格式
	AVFormatContext* av_fromat_context = avformat_alloc_context();
	//打开视频输入文件
	int open_result = avformat_open_input(&av_fromat_context, c_inputstr, NULL,
			NULL);
	if (open_result != 0) {
		LOGE("%s", "视频输入流打开失败！！！");
		return;
	} else {
		LOGI("%s", "视频输入流打开success ！");
	}

	//获取输入视频的相关信息。
	int info_result = avformat_find_stream_info(av_fromat_context, NULL);
	if (info_result < 0) {
		LOGE("%s", "视频输入流获取信息失败 ！！！");
		return;
	} else {
		LOGI("%s", "视频输入流获取信息success ！");
	}

	//获取到视频流的索引。
	int stream_index = -1;
	int i = 0;
	for (; i < av_fromat_context->nb_streams; i++) {
		if (av_fromat_context->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_VIDEO) {
			stream_index = i;
			break;
		}
	}

	if (stream_index == -1) {
		LOGE("%s", "获取视频流索引失败！！！");
		return;
	} else {
		LOGI("%s", "获取视频流索引 success ！");
	}

	//后去解码器
	AVCodecContext *av_codec_context =
			av_fromat_context->streams[stream_index]->codec;
	AVCodec * av_codec = avcodec_find_decoder(av_codec_context->codec_id);
	if (av_codec == NULL) {
		LOGE("%s", "获取解码器失败！！！");
		return;
	} else {
		LOGI("%s", "获取解码器 success ！");
	}

	//打开解码器
	int codec_open_result = avcodec_open2(av_codec_context, av_codec, NULL);
	if (codec_open_result != 0) {
		LOGE("%s : %d", "打开解码器失败！！！", codec_open_result);
		return;
	} else {
		LOGI("%s", "打开解码器 success ！");
	}

	//一个数据包。
	AVPacket *packet = malloc(sizeof(AVPacket));

	//像素数据。
	AVFrame * av_frame_yuv = av_frame_alloc();
	AVFrame * av_frame_rgb = av_frame_alloc();

	//加载窗口
	ANativeWindow* native_window = ANativeWindow_fromSurface(env, surface);
	//绘制缓冲区
	ANativeWindow_Buffer buffer;

	int len, got_frame, frame_count = 0;

	//一帧一帧的读取数据。
	while (av_read_frame(av_fromat_context, packet) >= 0) {
		avcodec_decode_video2(av_codec_context, av_frame_yuv, &got_frame,
				packet);
		if (got_frame) {
			LOGI("解码第%d帧", frame_count++);

			//设置缓冲区的属性（宽、高、像素格式）
			ANativeWindow_setBuffersGeometry(native_window,
					av_codec_context->width, av_codec_context->height,
					WINDOW_FORMAT_RGB_565);
			ANativeWindow_lock(native_window, &buffer, NULL);

			//设置rgb_frame的属性（像素格式、宽高）和缓冲区
			//rgb_frame缓冲区与outBuffer.bits是同一块内存
			avpicture_fill((AVPicture*) av_frame_rgb, buffer.bits,
					PIX_FMT_RGB565, av_codec_context->width,
					av_codec_context->height);
			I420ToRGB565(av_frame_yuv->data[0], av_frame_yuv->linesize[0],
					av_frame_yuv->data[1], av_frame_yuv->linesize[1],
					av_frame_yuv->data[2], av_frame_yuv->linesize[2],
					av_frame_rgb->data[0], av_frame_rgb->linesize[0],
					av_codec_context->width, av_codec_context->height);
//			I420ToARGB4444(av_frame_yuv->data[0], av_frame_yuv->linesize[0],
//					av_frame_yuv->data[1], av_frame_yuv->linesize[1],
//					av_frame_yuv->data[2], av_frame_yuv->linesize[2],
//					av_frame_rgb->data[0], av_frame_rgb->linesize[0],
//					av_codec_context->width, av_codec_context->height);
//			I420ToARGB(av_frame_yuv->data[0], av_frame_yuv->linesize[0],
//					av_frame_yuv->data[2], av_frame_yuv->linesize[2],
//					av_frame_yuv->data[1], av_frame_yuv->linesize[1],
//					av_frame_rgb->data[0], av_frame_rgb->linesize[0],
//					av_codec_context->width, av_codec_context->height);

			ANativeWindow_unlockAndPost(native_window);
		}
		av_free_packet(packet);
	}
	LOGI("解码结束！！");
	ANativeWindow_release(native_window);
	av_frame_free(&av_frame_yuv);
	av_frame_free(&av_frame_rgb);

	avcodec_close(av_codec_context);
	avformat_free_context(av_fromat_context);

	(*env)->ReleaseStringUTFChars(env, inputstr, c_inputstr);
}
;
