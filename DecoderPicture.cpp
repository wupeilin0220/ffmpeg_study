// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "DecoderPicture.h"

DecoderPicture::DecoderPicture()	
{
//	sws_getContext()
	type = StreamType::PicStream;
	
}


DecoderPicture::~DecoderPicture()
{
}



int DecoderPicture::decoderStream()
{
	AVPacket packet;
	AVFrame* frame = av_frame_alloc();
	int ret = av_read_frame(formatContext, &packet);
	if (ret < 0 && ret != AVERROR_EOF) {
		goto end;
	}
	// 如果是图片数据则会直接输出数据
	ret = avcodec_send_packet(video_codecCtx, &packet);
	if (ret < 0) {
		goto end;
	}

	ret = avcodec_receive_frame(video_codecCtx, frame);
	if (ret < 0) {
		goto end;
	}
	
	DateSet data;
	data.srcData = frame;
	data.type = MEDIATYPE::VIDEO;
	mutex_link.lock();
	if(link)
		link->fillBuffer(data);
	mutex_link.unlock();

end:
	if (ret < 0) {
		av_frame_free(&frame);
	}
	return ret;
}


