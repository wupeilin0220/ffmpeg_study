#include "DecoderFile.h"


DecoderFile::DecoderFile() :
	handle_decode(nullptr),
	Isloop(true),
	run(true),seekFlag(false),offset(0)
{
	type = StreamType::FilStream;
}


DecoderFile::~DecoderFile()
{
	stopStream();
}

int DecoderFile::decoderStream()
{
	run = true;
	handle_decode = new std::thread([&]() {	this->exec(); });
	return 0;
}

int DecoderFile::stopStream()
{
	std::cout << "wait DecoderFile::stopStream quit\n";
	run = false;
	if (handle_decode) {
		if (handle_decode->joinable()) {
			handle_decode->join();
		}
		delete handle_decode;
		handle_decode = nullptr;
	}
	return 0;
}

int DecoderFile::reSetDecode(int64_t& offset_v,int64_t& offset_a){

	int64_t offsetTime = av_gettime() - getDecodeTime();
	if (info.hasVideo) {
		avcodec_flush_buffers(video_codecCtx);
		offset_v = av_rescale_q(offsetTime, AVRational{ 1, AV_TIME_BASE }, formatContext->streams[video_index]->time_base);
	}

	if (info.hasAudio) {
		avcodec_flush_buffers(audio_codecCtx);
		offset_a = av_rescale_q(offsetTime, AVRational{ 1, AV_TIME_BASE }, formatContext->streams[audio_index]->time_base);
	}

	return 0;
}

int DecoderFile::exec()
{
	bool firstFrame_v = true, firstFrame_a = true;
	int ret = 0;
	
	AVPacket packet;

	int64_t offset_a = 0,offset_v = 0;		
	int64_t start_vpts = 0,start_apts = 0;	// record first frame pts ,sync different input streams's pts
	bool accurateSeek_v = false;
	bool accurateSeek_a = false;

	// clear last data (emergency stream)
	av_seek_frame(formatContext, -1, 0, AVSEEK_FLAG_FRAME);
	reSetDecode(offset_v,offset_a); 

	while (run) {	

		if(seekFlag){
			seekFlag = false;
			av_seek_frame(formatContext, -1, offset, AVSEEK_FLAG_BACKWARD); 
			reSetDecode(offset_v,offset_a);		
			firstFrame_v = true;
			firstFrame_a = true;
			accurateSeek_v = true;
			accurateSeek_a = true;
		}


		ret = av_read_frame(formatContext, &packet);
		if (ret < 0 ) {
			if (ret == AVERROR_EOF) {
				if (Isloop) {
					av_seek_frame(formatContext, -1, offset, AVSEEK_FLAG_BACKWARD);  //offset
					reSetDecode(offset_v,offset_a);		
					firstFrame_v = true;
					firstFrame_a = true;
					accurateSeek_v = true;
					accurateSeek_a = true;
					continue;
				}
				else {
					break;
				}
			}
			else
				break;
		}
		
		if (packet.stream_index == video_index && info.hasVideo) {
			ret = avcodec_send_packet(video_codecCtx, &packet); // video_codecCtx 的time_base 会根据packet填充
			if (ret < 0) {
				av_packet_unref(&packet);
				if (AVERROR(EINVAL) == ret) {
					break;
				}
				else {
					continue;
				}
			}
			
			while (1) {
				AVFrame* vframe = av_frame_alloc();
				ret = avcodec_receive_frame(video_codecCtx, vframe);
				if (ret < 0) {
					av_frame_free(&vframe);
					if (AVERROR(EINVAL) == ret) {
						return -1;
					}
					else {
						break;
					}
				}

				if(accurateSeek_v && SeekCheck(vframe,MEDIATYPE::VIDEO,offset) == 0){
					accurateSeek_v = false;
					av_frame_free(&vframe);
					continue;
				}

				vframe->pict_type = AV_PICTURE_TYPE_NONE;

		//		std::cout << "stream offset video pts "<< vframe->pts * av_q2d(formatContext->streams[video_index]->time_base) <<std::endl;
				if (firstFrame_v) {
					firstFrame_v = false;

	//				std::cout << "video stream input start "<< 	formatContext->streams[video_index]->start_time * av_q2d(formatContext->streams[video_index]->time_base) << "\n time base "<<formatContext->streams[video_index]->time_base.den<< " "<<formatContext->streams[video_index]->time_base.num ;
	//				std::cout << "stream offset video pts "<< vframe->pts * av_q2d(formatContext->streams[video_index]->time_base) <<std::endl;

					start_vpts = vframe->pts;
					vframe->pts = 0;
				}
				else {
					vframe->pts -= start_vpts;
				}

				vframe->pts += offset_v;

				if (pushStream(MEDIATYPE::VIDEO, vframe) < 0)
					av_frame_free(&vframe);				
			}
		}
		else if (packet.stream_index == audio_index) {
			ret = avcodec_send_packet(audio_codecCtx, &packet);
			if (ret < 0) {
				av_packet_unref(&packet);
				if (AVERROR(EINVAL) == ret) {
					break;
				}
				else {
					continue;
				}
			}
			while (1) {
				AVFrame* aframe = av_frame_alloc();
				ret = avcodec_receive_frame(audio_codecCtx, aframe);
				if (ret < 0) {
					av_frame_free(&aframe);
					if (AVERROR(EINVAL) == ret) {
						return -1;
					}
					else {
						break;
					}
				}

				if(accurateSeek_a && SeekCheck(aframe,MEDIATYPE::AUDIO,offset) == 0){
					accurateSeek_a = false;
					av_frame_free(&aframe);
					continue;
				}

				if (firstFrame_a) {
			//		std::cout << "audio stream input start "<< 	formatContext->streams[audio_index]->start_time * av_q2d(formatContext->streams[audio_index]->time_base);
			//		std::cout << "stream offset audio pts "<< aframe->pts * av_q2d(formatContext->streams[audio_index]->time_base) <<std::endl;

					firstFrame_a = false;


					start_apts = aframe->pts;
					aframe->pts = 0;
				}
				else {
					aframe->pts -= start_apts;
				}

				aframe->pts += offset_a;

				//			std::cout << "audio " << aframe->pts* av_q2d(audio_codecCtx->time_base)<<std::endl;
				if (pushStream(MEDIATYPE::AUDIO, aframe) < 0) {
					av_frame_free(&aframe);
				}
			}
		}
		av_packet_unref(&packet);
	}

	std::cout << "fileDecoder  send data time use " << av_gettime() - getDecodeTime()<< std::endl;


	return ret;	
}

int DecoderFile::pushStream(MEDIATYPE type, AVFrame* frame)
{
	// control data send speed
	int ret = -1;
	AVRational time_base;
	if (type == MEDIATYPE::VIDEO) {
		time_base = formatContext->streams[video_index]->time_base;
		nowVideoPts = frame->pts * av_q2d(time_base);
	}
	else {
		time_base = formatContext->streams[audio_index]->time_base;
	}
	
	int64_t now = av_gettime() - frame->pts * av_q2d(time_base)*1000*1000;
	
	
	if (now < getDecodeTime()) {
		int64_t sleepT = getDecodeTime() - now;
	//	std::cout << " sleep time "<< sleepT<<std::endl;
		while(run){
			if(sleepT > 3*1000*1000){	
				av_usleep(3*1000*1000);
				sleepT -= 3*1000*1000;
			}else{
				if (abs(sleepT) > 3 * 1000 * 1000)
					return -1;
				av_usleep(sleepT);
				break;
			}
			if(!run){
				return -1;
			}
		}
	}

	DateSet data;
	data.type = type;
	data.srcData = frame;
	
//	mutex_link.lock();
	if (link) {	
		ret = link->fillBuffer(data);
	}
//	mutex_link.unlock();
	return ret;
}

int DecoderFile::seekPos(int64_t position){
	int64_t duration = INT_MAX;
	if(formatContext){
		duration = formatContext->duration;
	}
	std::cout << "file stream duration "<< duration<<std::endl;
	if(duration <= position*1000){
		return -1;
	}

	offset = position*1000;
	seekFlag = true;

	return 0;
}

int DecoderFile::SeekCheck(AVFrame* frame,MEDIATYPE type,int64_t time){
	int64_t realTime = 0;
	if(type == MEDIATYPE::VIDEO){
//		std::cout << "video ";
		realTime= frame->pts*av_q2d(formatContext->streams[video_index]->time_base)*1000*1000;	
	}else{
//		std::cout << "audio ";
		realTime = frame->pts*av_q2d(formatContext->streams[audio_index]->time_base)*1000*1000;
	}
//	std::cout << "frame time "<< realTime << " seek time "<< time<<std::endl;
	if(abs(realTime - time) <= 40*1000)	
		return 0;

	return -1;
}
