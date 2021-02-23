// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "DecoderNet.h"

static int Interrupt_CB(void* p) {
	int64_t start = *(int64_t*)p;
	int64_t now = av_gettime();
//	std::cout << "interrupt now "<< now << " start "<< start<<std::endl;
	if ((now - start) > NetTimeout) {
		return 1;
	}
	return 0;
}

DecoderNet::DecoderNet():
	run(true), streamBroken(false),
	handle_decode(nullptr),
	firstFrame_v(true), firstFrame_a(true),
	start_vpts(0), start_apts(0),
	offset_v (0),offset_a (0),
	last_correct_vt(-1),last_correct_at(-1)
{
	type = StreamType::NetStream;
}


DecoderNet::~DecoderNet()
{
//	std::cout << "DecoderNet is stopStream.....\n";
	stopStream();
	std::cout << "DecoderNet is disstruct\n";
}



int DecoderNet::decoderStream()
{
	if(!handle_decode)
		run = true;
		handle_decode = new std::thread([&]() {	this->exec(); });
	return 0;
}

int DecoderNet::stopStream()
{
	std::cout << "stop decode net stream\n";
	mutex_handle.lock();
	std::cout << "mutex_handle.lock \n";
	run = false;
	if (handle_decode) {
		std::cout << "stop decode net stream\n";
		if (handle_decode->joinable()) {
			handle_decode->join();
		}
		delete handle_decode;
		handle_decode = nullptr;
	}
	mutex_handle.unlock();
	return 0;
}

int DecoderNet::reConnected()
{
	std::lock_guard<std::mutex> ck(mutex_handle);
	std::cout << "mutex_handle.lock \n";
	run = false;
	if (streamBroken) {
		uninit_decoder();
		std::cout << "reConnect ";
		int ret = init_decoder(inputUrl,1);
		if(ret < 0){
			std::cout <<" fail_____\n";
			return ret;
		}
		if (handle_decode) {
			if(handle_decode->joinable())
				handle_decode->join();
	
			delete handle_decode;
			handle_decode = nullptr;
		}
		
		std::cout << "decoderStream..........\n";
		decoderStream();
		std::cout << "mutex_handle.unlock \n";
	}
	mutex_handle.unlock();
	return 0;
}


void DecoderNet::setInterruptCallback()
{
	formatContext->interrupt_callback.callback = Interrupt_CB;
	formatContext->interrupt_callback.opaque = &start_time;
}

int DecoderNet::exec()
{
	std::cout << "DecoderNet::exec start run..............\n";
	int ret = 0;
	AVPacket packet;
	
	firstFrame_v = true;
	firstFrame_a = true;
	streamBroken = false;
	dec_time = av_gettime() - getDecodeTime();
	int err = 0;
		
	while (run) {
		
		start_time = av_gettime();
		ret = av_read_frame(formatContext, &packet);
		if (ret < 0 ) {
			if((ret == (AVERROR(EAGAIN))) && err < 5){
				err++;
				std::cout << "DecoderNet  AVERROR(EAGAIN)\n";
				av_packet_unref(&packet);
				av_usleep(10*1000);
				continue;
			}
			run =false;
			std::cout << inputUrl <<" stream broken\n"; 
			sendBrokenFlag();
			break;
		}

		if (packet.stream_index == video_index) {
			ret = avcodec_send_packet(video_codecCtx, &packet); 
			if (ret < 0) {
				std::cout << "err video send_packet err  "<< ret <<std::endl; 
				av_packet_unref(&packet);
				continue;
			}
			
			while (run) {
				AVFrame* vframe = av_frame_alloc();
				ret = avcodec_receive_frame(video_codecCtx, vframe);
				if (ret < 0) {
					av_frame_free(&vframe);
					if (AVERROR(EINVAL) == ret) {
						std::cout << "err avcodec_receive_frame video \n";
						sendBrokenFlag();
						return -1;
					}
					else if (AVERROR_EOF == ret) {
						break;
					}
					break;
				}
				
		//		std::cout << "stream offset video pts "<< vframe->pts * av_q2d(formatContext->streams[video_index]->time_base) <<std::endl;
//				nextV = framePtsCorrection(vframe,MEDIATYPE::VIDEO,nextV); 
				vframe->pict_type = AV_PICTURE_TYPE_NONE;
				if (firstFrame_v) {
					firstFrame_v = false;
					std::cout << "video stream input start "<< 	formatContext->streams[video_index]->start_time * av_q2d(formatContext->streams[video_index]->time_base) << "\n time base "<<formatContext->streams[video_index]->time_base.den<< " "<<formatContext->streams[video_index]->time_base.num ;
					std::cout << "stream offset video pts "<< vframe->pts * av_q2d(formatContext->streams[video_index]->time_base) <<std::endl;

					offset_v = av_rescale_q(dec_time, AVRational{ 1,AV_TIME_BASE }, formatContext->streams[video_index]->time_base);
					start_vpts = vframe->pts;
					vframe->pts = 0;
				}
				else {
					vframe->pts -= start_vpts;
				}
				vframe->pts += offset_v;

				DateSet data;
				data.type = MEDIATYPE::VIDEO;
				nowVideoPts = vframe->pts* av_q2d(formatContext->streams[video_index]->time_base);	
				int64_t absT = (vframe->pts* av_q2d(formatContext->streams[video_index]->time_base)*1000*1000) - (av_gettime() - getDecodeTime());
				if(absT > 0){
					if(absT >= 4*1000*1000){
						std::cout << "video sleep time is too long "<< absT<<"change to 4s\n";
						absT = 4*1000*1000;
					}
					av_usleep(absT);
				} 

				mutex_link.lock();
				if (link) {
					data.srcData = vframe;
					if (link->fillBuffer(data) < 0) {
						av_frame_free(&vframe);
					}
				}
				mutex_link.unlock();
			}
			if(!run){
				std::cout << "quit flag is set \n"; 
			}
		}
		else if (packet.stream_index == audio_index) {
			ret = avcodec_send_packet(audio_codecCtx, &packet);
			if (ret < 0) {
				std::cout << "err audio send_packet err  "<< ret <<std::endl; 
				av_packet_unref(&packet);
				continue;
			}

			while (run) {
				AVFrame* aframe = av_frame_alloc();
				ret = avcodec_receive_frame(audio_codecCtx, aframe);
				if (ret < 0) {
					av_frame_free(&aframe);
					if (AVERROR(EINVAL) == ret) {
						std::cout << "err avcodec_receive_frame audio\n";
						sendBrokenFlag();
						return -1;
					}
					else if (AVERROR_EOF == ret) {
						break;
					}
					break;
				}
				
//				nextA = framePtsCorrection(aframe,MEDIATYPE::AUDIO,nextA); 
//				std::cout << "stream offset audio pts "<< aframe->pts * av_q2d(formatContext->streams[audio_index]->time_base) <<std::endl;
				if (firstFrame_a) {
					std::cout << "audio stream input start "<< 	formatContext->streams[audio_index]->start_time * av_q2d(formatContext->streams[audio_index]->time_base);
					std::cout << "stream offset audio pts "<< aframe->pts * av_q2d(formatContext->streams[audio_index]->time_base) <<std::endl;
					firstFrame_a = false;
					offset_a = av_rescale_q(dec_time, AVRational{ 1,AV_TIME_BASE }, formatContext->streams[audio_index]->time_base);

					start_apts = aframe->pts;
					aframe->pts = 0;
				}
				else {
					aframe->pts -= start_apts;
				}
				aframe->pts += offset_a;


				int64_t absT = (aframe->pts* av_q2d(formatContext->streams[audio_index]->time_base)*1000*1000) - (av_gettime() - getDecodeTime());
				if(absT > 0){
					if(absT >= 4*1000*1000){
						std::cout << "audio sleep time is too long "<< absT<<"change to 4s\n";
						absT = 4*1000*1000;
					}
					av_usleep(absT);
				} 

				DateSet data;
				data.type = MEDIATYPE::AUDIO;
				mutex_link.lock();
				if (link) {
					data.srcData = aframe;
					if (link->fillBuffer(data) < 0)
						av_frame_free(&aframe);
				}else{
					av_frame_free(&aframe);
				}
				mutex_link.unlock();
			}

			if(!run){
				std::cout << "quit flag is set \n"; 
			}
		}

		av_packet_unref(&packet);
	}
	
	std::cout << "DecoderNet::exec is quit_________________\n";
	return ret;
}


void DecoderNet::sendBrokenFlag(){
	streamBroken = true;
	if(brokenFun){
		brokenFun(av_gettime());
	}
}

#define MAX_JITTLER_TIME 250 // 250ms
#define DEFAULT_FRAME_TIME 10 // 10ms
// 时间戳抖动检测
int64_t DecoderNet::framePtsCorrection(AVFrame*,MEDIATYPE,int64_t lastPts){
/*	AVRational timebase;

	if(type == MEDIATYPE::VIDEO){
		timebase = formatContext->streams[video_index]->time_base;
	}else if( type == MEDIATYPE::AUDIO){
		timebase = formatContext->streams[audio_index]->time_base;	
	}else 
		return frame->pts;

	int64_t delta = frame->pts + frame->pkt_duration - lastPts;
	int64_t time = frame->pts;
	if(abs(delta) * av_q2d(timebase) > MAX_JITTLER_TIME){
		std::cout << " correction frame pts from "<<  frame->pts;  
		if(type == MEDIATYPE::VIDEO){
			delta = av_rescale_q(1000.0/info.fps,AVRational{1,AV_TIME_BASE},timebase);
		}else if(type == MEDIATYPE::AUDIO){
				
		}else
			delta = DEFAULT_FRAME_TIME * 1000;

		std::cout << " to "<<  frame->pts<<std::endl; 
	}	

	
	return  time;
	*/
	return lastPts;
}
