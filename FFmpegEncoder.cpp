// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "FFmpegEncoder.h"


FFmpegEncoder::FFmpegEncoder(const MuxContext& media):
	avframeData(),avpackData_v(),avpackData_a(),
	outFmtCtx(NULL),video_codecCtx(NULL),audio_codecCtx(NULL),
	video_index(0),audio_index(0),
	hand_encode(nullptr),hand_pushStream(nullptr),run(true),
	srcInfo(media),frameCount(0),timeCount(0), sos(false),sos_getPacket(false),
	sendVideo(false),sendAudio(false),offset_v(0),offset_a(0),getPacket(false),decSos(nullptr), 
	video_codecCtx_Emergency(nullptr), audio_codecCtx_Emergency(nullptr),
	handEmergency_encode(nullptr) ,sos_videoOffset(0),sos_audioOffset(0),emclink(nullptr),needDrop_v(false),needDrop_a(false),
	ssBroken(false),absTime(0),streamBrokenTime(0), funCallback(nullptr),sendtime(true)
{
	emclink = new EmergencyLink(this);
//	key_flag_sos = false;

}


FFmpegEncoder::~FFmpegEncoder()
{
	stopPush();
	clearFramebuffer();
	clearPacketbuffer();
	uninit_encoder();
	remove_Emergency();
	if(funCallback)
		serialize();
//	delete emclink;
}

int FFmpegEncoder::init_encoder(std::string url,TaskApply& task)
{
	
	int ret = 0;
	if (task.getType() != ApplyType::CREATE || task.mediaCtx.video_height <= 0 || task.mediaCtx.video_width <= 0){
		std::cout << "init_encoder: err task type\n"; 
		return -1;
	}
	outFmtCtx = avformat_alloc_context();
	if (!outFmtCtx) {
		std::cout << "avformat_alloc_context err\n";
		return -1;
	}

	ret = avformat_alloc_output_context2(&outFmtCtx, NULL, "flv",url.c_str());
	if(ret < 0){
		std::cout << "FFmpegEncoder:: alloc context err "<<ret <<std::endl; 
	}


	if (srcInfo.hasVideo) {
		ret = init_encoder_video(task);
		if (ret < 0){
			std::cout << "has init video encodec \n";
			return ret;
		}
	}

	if (srcInfo.hasAudio) {
		ret = init_encoder_audio(task);
		if (ret < 0){
			std::cout << "has init audio encodec \n";
			return ret;
		}
	}

	outFmtCtx->max_interleave_delta = 1*1000;
	ret = avio_open(&(outFmtCtx->pb), url.c_str(), AVIO_FLAG_WRITE);
	if (ret < 0) {
		std::cout << "avio_open err with " << AVERROR(ret) << std::endl;
		return ret;
	}
	
	if(task.export_stream.delay >= (Config::getConf(CONFPATH)->GetStreamChannelExp())){
		std::cout << "delay time more len 30s\n";
		avio_close(outFmtCtx->pb);	
		
	}else{
		ret = avformat_write_header(outFmtCtx,NULL);
		if (ret < 0) {
			std::cout << "avformat_write_header err with " << ret << std::endl;
			return ret;
		}
	}

	outputUrl = url;

	taskEnc = task;


	return 0;
}

int FFmpegEncoder::uninit_encoder()
{
	if (video_codecCtx) {
		avcodec_free_context(&video_codecCtx);
		video_codecCtx = NULL;
	}

	if (audio_codecCtx) {
		avcodec_free_context(&audio_codecCtx);
		audio_codecCtx = NULL;
	}



	if (outFmtCtx) {
		avformat_free_context(outFmtCtx);
		outFmtCtx = NULL;
	}

	return 0;
}

int FFmpegEncoder::init_encoder_video(TaskApply& task)
{
	MuxContext& muxinfo = task.mediaCtx;
	AVCodec* codec_v = avcodec_find_encoder_by_name("libx264");
	if (!codec_v)
		return -1;

	AVStream* stream_video = avformat_new_stream(outFmtCtx, codec_v); // video_index = 0;
	if (!stream_video ) {
		std::cout << "avformat_new_stream err\n";
		return -1;
	}
	stream_video->time_base = AVRational{ 1,1000 }; // rtmp


	video_codecCtx = avcodec_alloc_context3(codec_v);
	video_codecCtx->time_base = AVRational{ 1,muxinfo.fps * 2 };
	video_codecCtx->bit_rate = (int64_t)(muxinfo.video_bits) * 1024;
	video_codecCtx->bit_rate_tolerance = (muxinfo.video_bits) * 1024;
	video_codecCtx->ticks_per_frame = 2;
	video_codecCtx->height			= muxinfo.video_height;
	video_codecCtx->width			= muxinfo.video_width;
	video_codecCtx->max_b_frames	= 0;
	video_codecCtx->gop_size		= muxinfo.fps;
	video_codecCtx->pix_fmt			= AV_PIX_FMT_YUV420P;
	video_codecCtx->qmax			= 37;
	video_codecCtx->qmin			= 10;
	video_codecCtx->qcompress		= 0.5;
	video_codecCtx->qblur			= 0.5;
	av_opt_set(video_codecCtx->priv_data, "profile", "baseline", 0); // cfr 推荐18-28 范围0-51，0表示无丢失
	const Config *conf = Config::getConf(CONFPATH);
	av_opt_set(video_codecCtx->priv_data, "preset", conf->GetPreset().c_str(), 0);
	av_opt_set(video_codecCtx->priv_data, "tune", "zerolatency", 0);


	video_codecCtx->keyint_min		= 4;


	av_opt_set_int(video_codecCtx->priv_data, "b_strategy", 1, 0);
	av_opt_set_int(video_codecCtx->priv_data, "sc_threshold", 0, 0);

	video_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;  // 推流时不应使用此标志

	int ret = avcodec_open2(video_codecCtx, codec_v, 0);
	if (ret < 0) {
		std::cout << "avcodec_open2 err with " << ret << std::endl;
		return -1;
	}
	avcodec_parameters_from_context(stream_video->codecpar, video_codecCtx);
	video_index = 0;

	if (decSos) {
		// copy audio codec
		if (video_codecCtx_Emergency) {
			avcodec_free_context(&video_codecCtx_Emergency);
		}

	}

	std::cout << "Encoder::Video init\n";
	return 0;
}

int FFmpegEncoder::init_encoder_audio(TaskApply& task)
{
	MuxContext& muxinfo = task.mediaCtx;

	AVCodec* acodec = avcodec_find_encoder_by_name("aac");
	if (acodec == nullptr){
		std::cout << "find ecoder err\n";
		acodec = avcodec_find_encoder_by_name("aac");
		if (acodec == nullptr)
			return -1;
	}

	AVStream* stream_audio = avformat_new_stream(outFmtCtx, acodec);
	stream_audio->time_base = AVRational{1,1000};

	audio_codecCtx = avcodec_alloc_context3(acodec);

	audio_codecCtx->bit_rate = (int64_t)(muxinfo.audio_bits)*1024;
	audio_codecCtx->sample_rate = muxinfo.sample;
	audio_codecCtx->channels = muxinfo.audio_channel;
	audio_codecCtx->channel_layout = muxinfo.audio_channel_layout;
	audio_codecCtx->sample_fmt = (AVSampleFormat)muxinfo.audio_format;
	audio_codecCtx->time_base = { 1,audio_codecCtx->sample_rate };
	audio_codecCtx->ticks_per_frame = 1;


	int ret = avcodec_open2(audio_codecCtx, acodec, NULL);
	if (ret < 0) {
		std::cout << "can't open audio encoder\n";
		return ret;
	}
	audio_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;

	ret = avcodec_parameters_from_context(stream_audio->codecpar, audio_codecCtx);


	if (srcInfo.hasVideo)
		audio_index = 1;
	else
		audio_index = 0;

	if (decSos) {
		// copy audio codec
		if (audio_codecCtx_Emergency) {
			avcodec_free_context(&audio_codecCtx_Emergency);
		}
	}

	std::cout << "Encoder::Audio init\n";
	return ret;
}

int FFmpegEncoder::init_Emergency()
{
	if (!decSos) {
		std::cout << "there is no Sos stream\n";
		return -1;
	}
	std::cout << "\n start init Emergency \n";
	Linker* head = decSos;
	if(decSos->type == StreamType::FilStream){
		std::cout << "file staream Emergency init_______________________________\n";
		AspectRatio* aspec = nullptr;
		if (decSos->getMediaInfo().hasVideo) {
			aspec = new AspectRatio(decSos->getMediaInfo());
			TaskApply task_asp(ApplyType::ADAPT);
			
			task_asp.adstyle = AdaptStyle::Fill;
			task_asp.mediaCtx = taskEnc.mediaCtx;
			if (aspec->init_filter(task_asp) < 0) {
				std::cout << "init sos stream's  AspectRatio err\n";
				delete decSos;
				decSos = nullptr;

				delete aspec;
				aspec = nullptr;
				return -1;
			}
			std::cout << "Emergency has init AspectRatio______________\n";
			head->linkTo(aspec);
			head = aspec;
		}

		AudioResample* resample = nullptr;
		if (decSos->getMediaInfo().hasAudio) {
			resample = new AudioResample(decSos->getMediaInfo());
			TaskApply task_res(ApplyType::VOLUME);
			task_res.mediaCtx = taskEnc.mediaCtx;			
			task_res.volume = 1.0;

			if (resample->init_filter(task_res) < 0) {
				std::cout << "init sos stream's  AudioResample err\n";

				delete decSos;
				decSos = nullptr;

				delete aspec;
				aspec = nullptr;

				delete resample;
				resample = nullptr;
				return -1;
			}

			std::cout << "Emergency has init Resample Audio______________\n";
			head->linkTo(resample);
			head = resample;
		}
	}else if(decSos->type == StreamType::PicStream){	
		(dynamic_cast<PictureStream*>(decSos))->setSDProperty(taskEnc.mediaCtx.video_width,taskEnc.mediaCtx.video_height);
		std::cout << "set picture stream width "<< taskEnc.mediaCtx.video_width << " height "<< taskEnc.mediaCtx.video_height<<std::endl;
	}

	offset_v = taskEnc.export_stream.delay /av_q2d(srcInfo.video_timebase)/1000;	
	offset_a = taskEnc.export_stream.delay /av_q2d(srcInfo.audio_timebase)/1000;
	std::cout << "emergency stream has delay time: video "<<offset_v << " audio "<<offset_a <<std::endl; 
	std::cout << " \n init Emergency is over_________\n";
	head->linkTo(emclink);

	return 0;
}


int FFmpegEncoder::reinit_Emergency(){

	if (video_codecCtx_Emergency) {
		avcodec_free_context(&video_codecCtx_Emergency);
		video_codecCtx_Emergency = NULL;
	}

	if (audio_codecCtx_Emergency) {
		avcodec_free_context(&audio_codecCtx_Emergency);
		audio_codecCtx_Emergency = NULL;
	}

	mutex_avpack_vSos.lock();
	std::cout << "free sos video packet: "<<  avpackData_v_Sos.size()<<std::endl;
	while (!avpackData_v_Sos.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_v_Sos.front();
		av_packet_free(&pack);
		avpackData_v_Sos.pop();
	}
	mutex_avpack_vSos.unlock();

	mutex_avpack_aSos.lock();
	std::cout << "free sos audio packet: "<<  avpackData_a_Sos.size()<<std::endl;
	while (!avpackData_a_Sos.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_a_Sos.front();
		av_packet_free(&pack);
		avpackData_a_Sos.pop();
	}
	mutex_avpack_aSos.unlock();
	
	mutex_avframe_Sos.lock();
	while(!avframeData_Sos.empty()){
		AVFrame* frame = (AVFrame*)(avframeData_Sos.front().srcData);
		av_frame_free(&frame);
		avframeData_Sos.pop();
	}
	mutex_avframe_Sos.unlock();

	int ret = init_sosencoder_video(taskEnc);
	if(ret < 0){
		std::cout << "reinit emergency video codec fail "<< ret <<std::endl;
		return -1;
	}

	ret = init_sosencoder_audio(taskEnc);
	if(ret < 0){
		std::cout << "reinit emergency audio codec fail "<< ret <<std::endl;
		return -1;
	}

	Link *link = decSos->getNext();
	while(link){
		Link* tmp = link->getNext();
		if(tmp == emclink)
			break;
		delete link;	
		link = tmp;
	}

	init_Emergency();
	
	return 0;
}

int FFmpegEncoder::remove_Emergency(){
	if(decSos){
		Link *link = decSos->getNext();
		while(link){
			Link* tmp = link->getNext();
			delete link;	
			link = tmp;
		}
		delete decSos;
	}		
	return 0;
}

int FFmpegEncoder::encodeVideo(AVFrame* frame)
{

	int ret = 0;
	if (frame){
		frame->pts = av_rescale_q(frame->pts, srcInfo.video_timebase, video_codecCtx->time_base);
	}
	ret = avcodec_send_frame(video_codecCtx, frame);
	if (ret < 0) {
		if (AVERROR(EAGAIN) == ret ) {
			return 0;
		}
		return -1;
	}
	while (1) {
		AVPacket* packet = av_packet_alloc();
		ret = avcodec_receive_packet(video_codecCtx, packet);
		if (ret < 0) {
			av_packet_free(&packet);
			if (AVERROR(EAGAIN) == ret) {
				return 0;
			}
			else if (AVERROR_EOF == ret) {
				std::cout << "encoder is clean quit\n";
				return -1;
			}
			else { // EINVAL
				return -1;
			}
		}

		packet->pts = av_rescale_q(packet->pts, video_codecCtx->time_base, outFmtCtx->streams[video_index]->time_base);
	//	packet->dts = av_rescale_q(packet->dts, video_codecCtx->time_base, outFmtCtx->streams[video_index]->time_base);
		packet->dts = packet->pts;
		packet->stream_index = video_index;
		
		int64_t pts = packet->pts * av_q2d(outFmtCtx->streams[video_index]->time_base);
		if(pts >= absTime){
			mutex_avpack_v.lock();
			avpackData_v.push(packet);
			mutex_avpack_v.unlock();
			if(!getPacket){
				std::unique_lock<std::mutex> locker(cv_mutex);
				cv.notify_one();
				getPacket = true;
				std::cout << "first pack out time_______________________________ "<<av_gettime()<<std::endl;
			}
		}else{
			std::cout << "video packet pts < asbTime drop "<< pts << " abs " <<absTime<<std::endl;
			av_packet_free(&packet);
		}


	}
	return ret;
}

int FFmpegEncoder::encodeAudio(AVFrame* frame)
{

	int ret = 0;

	if (frame) {
		frame->pts = av_rescale_q(frame->pts, srcInfo.audio_timebase, audio_codecCtx->time_base);	
	}

	ret = avcodec_send_frame(audio_codecCtx, frame);
	if (ret < 0) {
		if (AVERROR(EAGAIN) == ret) {
			return 0;
		}
		return -1;
	}
	while (1) {
		AVPacket* packet = av_packet_alloc();
		ret = avcodec_receive_packet(audio_codecCtx, packet);
		if (ret < 0) {
			av_packet_free(&packet);
			if (AVERROR(EAGAIN) == ret) {
				return 0;
			}
			else if (AVERROR_EOF == ret) {
				std::cout << "encoder is clean quit\n";
				return -1;
			}
			else { // EINVAL
				return -1;
			}
		}

		packet->pts = av_rescale_q(packet->pts,  audio_codecCtx->time_base, outFmtCtx->streams[audio_index]->time_base);
	//	packet->dts = av_rescale_q(packet->dts, audio_codecCtx->time_base, outFmtCtx->streams[audio_index]->time_base);
		packet->dts = packet->pts;
		packet->stream_index = audio_index;

		int64_t pts = packet->pts* av_q2d(outFmtCtx->streams[audio_index]->time_base);
		if(pts >= absTime){
			mutex_avpack_a.lock();
			avpackData_a.push(packet);
			mutex_avpack_a.unlock();
			if(!getPacket){
				std::unique_lock<std::mutex> locker(cv_mutex);
				cv.notify_one();
				getPacket = true;
				std::cout << "first pack out time_______________________________ "<<av_gettime()<<std::endl;
			}
		}else{
			std::cout << "audio packet pts < asbTime drop\n";
			av_packet_free(&packet);
		}

	}
	return ret;
}

int FFmpegEncoder::encodeVideo_Sos(AVFrame* frame)
{
	int ret = 0;
	if (frame) {
		frame->pts = av_rescale_q(frame->pts, srcInfo.video_timebase, video_codecCtx_Emergency->time_base);
	}

	ret = avcodec_send_frame(video_codecCtx_Emergency, frame);
	if (ret < 0) {
		if (AVERROR(EAGAIN) == ret) {
			return 0;
		}
		return -1;
	}
	while (1) {
		AVPacket* packet = av_packet_alloc();
		ret = avcodec_receive_packet(video_codecCtx_Emergency, packet);
		if (ret < 0) {
			av_packet_free(&packet);
			if (AVERROR(EAGAIN) == ret) {
				return 0;
			}
			else if (AVERROR_EOF == ret) {
				std::cout << "encoder is clean quit\n";
				return -1;
			}
			else { // EINVAL
				return -1;
			}
		}
	
		packet->pts = av_rescale_q(packet->pts, video_codecCtx_Emergency->time_base, outFmtCtx->streams[video_index]->time_base) ;
	//	packet->dts = av_rescale_q(packet->dts, video_codecCtx_Emergency->time_base, outFmtCtx->streams[video_index]->time_base) ;
		packet->dts = packet->pts;
		
		packet->stream_index = video_index;

		mutex_avpack_vSos.lock();
		packet->pts -= sos_videoOffset;
		packet->dts = packet->pts;
		avpackData_v_Sos.push(packet);
		mutex_avpack_vSos.unlock();
	}
	return ret;
}

int FFmpegEncoder::encodeAudio_Sos(AVFrame* frame)
{
	int ret = 0;

	if (frame) {
		frame->pts = av_rescale_q(frame->pts, srcInfo.audio_timebase, audio_codecCtx_Emergency->time_base);
	}
	ret = avcodec_send_frame(audio_codecCtx_Emergency, frame);
	if (ret < 0) {
		if (AVERROR(EAGAIN) == ret) {
			return 0;
		}
		return -1;
	}
	while (1) {
		AVPacket* packet = av_packet_alloc();
		ret = avcodec_receive_packet(audio_codecCtx_Emergency, packet);
		if (ret < 0) {
			av_packet_free(&packet);
			if (AVERROR(EAGAIN) == ret) {
				return 0;
			}
			else if (AVERROR_EOF == ret) {
				return -1;
			}
			else { // EINVAL
				return -1;
			}
		}
		//		packet->pts = av_rescale_q(packet->pts, srcInfo.audio_timebase, outFmtCtx->streams[audio_index]->time_base);
		//		std::cout << "audio " << packet->pts * av_q2d(audio_codecCtx->time_base) << std::endl;

		packet->pts = av_rescale_q(packet->pts, audio_codecCtx_Emergency->time_base, outFmtCtx->streams[audio_index]->time_base) ;
//		packet->dts = av_rescale_q(packet->dts, audio_codecCtx_Emergency->time_base, outFmtCtx->streams[audio_index]->time_base) ;
		packet->dts = packet->pts;
		packet->stream_index = audio_index;

		mutex_avpack_aSos.lock();
		packet->pts -= sos_audioOffset;
		packet->dts = packet->pts;
		avpackData_a_Sos.push(packet);
		mutex_avpack_aSos.unlock();
	}
	return ret;
}

int FFmpegEncoder::init_sosencoder_video(TaskApply& task)
{
	MuxContext& muxinfo = task.mediaCtx;
	AVCodec* codec_v = avcodec_find_encoder_by_name("libx264");
	if (!codec_v)
		return -1;

	video_codecCtx_Emergency = avcodec_alloc_context3(codec_v);

	video_codecCtx_Emergency->time_base = AVRational{ 1,muxinfo.fps * 2 };
	video_codecCtx_Emergency->bit_rate = (int64_t)(muxinfo.video_bits) * 1024;
	video_codecCtx_Emergency->bit_rate_tolerance = (muxinfo.video_bits) * 1024;
	video_codecCtx_Emergency->ticks_per_frame = 2;
	video_codecCtx_Emergency->height = muxinfo.video_height;
	video_codecCtx_Emergency->width = muxinfo.video_width;
	std::cout << "muxinfo.video_height "<< muxinfo.video_height <<" muxinfo.video_width "<< muxinfo.video_width<<std::endl;
	video_codecCtx_Emergency->max_b_frames = 0;
	video_codecCtx_Emergency->gop_size = muxinfo.fps;
	video_codecCtx_Emergency->pix_fmt = AV_PIX_FMT_YUV420P;
	video_codecCtx_Emergency->qmax = 37;
	video_codecCtx_Emergency->qmin = 10;
	video_codecCtx_Emergency->qcompress = 0.5;
	video_codecCtx_Emergency->qblur = 0.5;
	video_codecCtx_Emergency->keyint_min = 4;

	const Config *conf = Config::getConf(CONFPATH);
//	av_opt_set(video_codecCtx->priv_data, "preset", conf->GetPreset().c_str(), 0);

//	av_opt_set(video_codecCtx->priv_data, "profile", "baseline", 0); // cfr 推荐18-28 范围0-51，0表示无丢失
	av_opt_set(video_codecCtx_Emergency->priv_data, "preset", conf->GetPreset().c_str(), 0);
	av_opt_set(video_codecCtx_Emergency->priv_data, "tune", "zerolatency", 0);

	av_opt_set_int(video_codecCtx_Emergency->priv_data, "b_strategy", 1, 0);
	av_opt_set_int(video_codecCtx_Emergency->priv_data, "sc_threshold", 0, 0);

	video_codecCtx_Emergency->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;  // 推流时不应使用此标志

	int ret = avcodec_open2(video_codecCtx_Emergency, codec_v, NULL);
	if (ret < 0) {
		std::cout << "avcodec_open2 err with " << ret << std::endl;
		return -1;
	}
	return 0;
}

int FFmpegEncoder::init_sosencoder_audio(TaskApply& task)
{
	MuxContext& muxinfo = task.mediaCtx;
	AVCodec* acodec = avcodec_find_encoder_by_name("aac");
	if (acodec == nullptr) {
		std::cout << "find ecoder err\n";
		acodec = avcodec_find_encoder_by_name("aac");
		if (acodec == nullptr)
			return -1;
	}

	audio_codecCtx_Emergency = avcodec_alloc_context3(acodec);

	audio_codecCtx_Emergency->bit_rate = (int64_t)(muxinfo.sample) * 2;
	audio_codecCtx_Emergency->sample_rate = muxinfo.sample;
	audio_codecCtx_Emergency->channels = muxinfo.audio_channel;
	audio_codecCtx_Emergency->channel_layout = muxinfo.audio_channel_layout;
	audio_codecCtx_Emergency->sample_fmt = AV_SAMPLE_FMT_FLTP;
	audio_codecCtx_Emergency->time_base = { 1,audio_codecCtx_Emergency->sample_rate };
	audio_codecCtx_Emergency->ticks_per_frame = 1;

	int ret = avcodec_open2(audio_codecCtx_Emergency, acodec, NULL);
	if (ret < 0) {
		std::cout << "can't open audio encoder\n";
		return ret;
	}
	audio_codecCtx_Emergency->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;  // 靠靠靠靠靠
	return 0;
}

int FFmpegEncoder::streamBroken(bool broken,MEDIATYPE type){
	if(broken){
		// check packet num 
		// buffer time limit 0.5s  10 frame, 20 audio packet
		if(type == MEDIATYPE::VIDEO){  
			if(avpackData_v.size() < 10){
				ssBroken = true;	
			}
		}else{
			if(avpackData_a.size() < 20){
				ssBroken = true;
			}	
		}

		// record break time
		if(ssBroken && streamBrokenTime == 0){
			streamBrokenTime = av_gettime(); 
		}
	}

	if(broken == false && ssBroken){
		int64_t Tre = av_gettime();
		std::cout << "stream break time "<< (Tre - streamBrokenTime)/1000 <<" ms\n";
//		if((Tre - streamBrokenTime)/1000 >= Config::getConf(CONFPATH)->GetStreamChannelExp()){
			std::cout <<"reopen push stream address \n";
			stopPush();
			uninit_encoder();
			clearPacketbuffer();
			clearFramebuffer();
			int ret = init_encoder(outputUrl,taskEnc);
			if(ret >= 0){
				startEncode();
				std::cout << "reinit encode \n";
				ssBroken = false;
				streamBrokenTime = 0;
			}else{
				std::cout << "streamBroken: init_encoder fail\n";
			//	ssBroken = false;
			}
//		}
	}
	return 0;
}

int FFmpegEncoder::syncPacket(){
	return 0;
}

int FFmpegEncoder::exec()
{

	//	AVPacket *packet = av_packet_alloc();

	int64_t v_lastPts = 0;;
	int64_t a_lastPts = 0;;
	while (run) {
		// get data
		mutex_avframe.lock();
		if (avframeData.empty()) {
			mutex_avframe.unlock();
			av_usleep(40*1000); // fps time			
			continue;
		}
		DateSet data = avframeData.front();
		avframeData.pop();
		mutex_avframe.unlock();

		// frame encode	
		AVFrame* src = (AVFrame*)(data.srcData);
		if (src == nullptr){
			std::cout << "src is empty\n";
			continue;
		}
	
		int ret = 0;
		if (MEDIATYPE::VIDEO == data.type && (!needDrop_v)) {
			if(v_lastPts >= src->pts){
				std::cout << "video pts is <= last pts, drop frame vlast "<<v_lastPts << " now "<< src->pts<<std::endl;
				av_frame_free(&src);
				continue;
			}
			v_lastPts = src->pts;
			if ((ret = encodeVideo(src)) < 0) {
				std::cout << "quit____\n";	
				break;
			}

		}
		else if (MEDIATYPE::AUDIO == data.type && (!needDrop_a)) {
			if(a_lastPts >= src->pts){
				std::cout << "audio pts is <= last pts, drop frame "<< a_lastPts << " now "<< src->pts <<std::endl;
				av_frame_free(&src);
				continue;
			}
			a_lastPts = src->pts;
			if ((ret = encodeAudio(src)) < 0) {
				std::cout << "quit____\n";	
				break;
			}	
		}
		av_frame_free(&src);
	}
	std::cout << "stop encodec___________\n";		
	return 0;
}

int FFmpegEncoder::emergencyEncode()
{
	std::cout << "emergencyEncode is start_____________\n";
	int64_t v_lastPts = 0;;
	int64_t a_lastPts = 0;;
	while (sos) {
		// get data
		mutex_avframe_Sos.lock();
		if (avframeData_Sos.empty()) {
			mutex_avframe_Sos.unlock();
			av_usleep(40 * 1000); // fps time			
			continue;
		}
		DateSet data = avframeData_Sos.front();
		avframeData_Sos.pop();
		mutex_avframe_Sos.unlock();

		// frame encode	
		AVFrame* src = (AVFrame*)(data.srcData);
		if (src == nullptr) {
			std::cout << "src is empty\n";
			continue;
		}

		int ret = 0;

		if (MEDIATYPE::VIDEO == data.type && (!needDrop_v)) {
			//	if (sos && srcInfo.hasVideo) {
			if (v_lastPts >= src->pts) {
				std::cout << "pts is <= last pts, drop frame\n";
				av_frame_free(&src);
				continue;
			}
			v_lastPts = src->pts;
			//			std::cout << "video time " << v_lastPts * av_q2d(srcInfo.video_timebase) << std::endl;
			if ((ret = encodeVideo_Sos(src)) < 0) {
				std::cout << "Video quit____\n";
				break;
			}
			//	}
		}
		else if (MEDIATYPE::AUDIO == data.type && (!needDrop_a)) {
			if (a_lastPts >= src->pts) {
				std::cout << "pts is <= last pts, drop frame\n";
				av_frame_free(&src);
				continue;
			}
			a_lastPts = src->pts;
			if ((ret = encodeAudio_Sos(src)) < 0) {
				std::cout << "Audio quit____\n";
				break;
			}
		}
		av_frame_free(&src);
	}
	
	// flush encodec 
	encodeVideo_Sos(nullptr);
	encodeAudio_Sos(nullptr);

	std::cout << "stop Emergency encodec___________\n";
	return 0;
}

/*
void FFmpegEncoder::replacePacket( AVPacket** pack_replace, std::queue<AVPacket*>& dataQue, std::mutex& mutexQue)
{
	int retry  = 0;
	do {
		mutexQue.lock();
		if (dataQue.size() > 0) {
			AVPacket* tmp = (AVPacket*)(dataQue.front());
			dataQue.pop();
			mutexQue.unlock();

			if (tmp->stream_index == video_index) {
				if (tmp->flags & AV_PKT_FLAG_KEY) {
					key_flag_sos = true;
				}
			}

			if (key_flag_sos || tmp->stream_index == audio_index) {
				tmp->pts = (*pack_replace)->pts;
				tmp->dts = (*pack_replace)->dts;
				//	tmp->duration = (*pack_replace)->duration;

				av_packet_free(pack_replace);
				*pack_replace = tmp;
			}
			else {
				av_packet_free(&tmp);
			}
			break;
		}
		mutexQue.unlock();
		av_usleep(40 * 1000);
		if(retry++ > 75)
			break;
	} while (1);
}
*/



int FFmpegEncoder::fillBuffer(DateSet date)
{
	//	return -1;
//	if(funCallback)
//		std::cout << "get data \n";
	mutex_avframe.lock();
	avframeData.push(date);
	mutex_avframe.unlock();
	return 0;
}

int FFmpegEncoder::startEncode()  // 不可以使用inline 
{
	run = true;
	if (!hand_encode) {
		hand_encode = new std::thread([&](){ this->exec(); });
	}
	if (!hand_pushStream) {
		hand_pushStream = new std::thread([&]() { this->StreamPush(); });
	}

	return 0;
}

int FFmpegEncoder::stopPush()
{

	run = false;
	sos = false;
	swicthNormalStream();
	std::cout << "stop encode "<<av_gettime()<<std::endl;

	if (hand_pushStream && hand_pushStream->joinable()) {
		hand_pushStream->join();
	//	std::cout << "891"<<std::endl;
		delete hand_pushStream;
		hand_pushStream = nullptr;
	}

	if (hand_encode && hand_encode->joinable()) {
		hand_encode->join();
		delete hand_encode;
		hand_encode = nullptr;
	}
	std::cout << "stop push "<<av_gettime()<<std::endl;

	//	run = false;
	std::cout << "FFmpegEncoder::stopPush over \n";
	return 0;
}

void FFmpegEncoder::stopEncodeEmergency(){

	if (decSos) {
		std::cout << "stop sos stream__________________\n";

		if (handEmergency_encode && handEmergency_encode->joinable()) {
			handEmergency_encode->join();
			delete handEmergency_encode;
			handEmergency_encode = nullptr;
		}
	}
}

void FFmpegEncoder::startEncodeEmergency(){
	if (decSos != nullptr) {
		std::cout << "\n run Emergency encode______\n";
		if (!handEmergency_encode) {
			handEmergency_encode = new std::thread([&]() {this->emergencyEncode(); });
		}
	}
		
}

int FFmpegEncoder::clearFramebuffer()
{
	mutex_avframe.lock();
	std::cout << "free frames: "<< avframeData.size()<<std::endl;
	while (!avframeData.empty()) {
		AVFrame* frame = (AVFrame*)avframeData.front().srcData;
		av_frame_free(&frame);
		avframeData.pop();
	}
	mutex_avframe.unlock();

	mutex_avframe_Sos.lock();
	std::cout << "free sos frames: "<< avframeData_Sos.size()<<std::endl;
	while (!avframeData_Sos.empty()) {
		AVFrame* frame = (AVFrame*)avframeData_Sos.front().srcData;
		av_frame_free(&frame);
		avframeData_Sos.pop();
	}
	mutex_avframe_Sos.unlock();
	return 0;
}

int FFmpegEncoder::clearPacketbuffer()
{
	mutex_avpack_v.lock();
	std::cout << "free video packet: "<< avpackData_v.size()<<std::endl;
	while (!avpackData_v.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_v.front();
		av_packet_free(&pack);
		avpackData_v.pop();
	}
	mutex_avpack_v.unlock();

	mutex_avpack_a.lock();
	std::cout << "free audio packet: "<< avpackData_a.size()<<std::endl;
	while (!avpackData_a.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_a.front();
		av_packet_free(&pack);
		avpackData_a.pop();
	}
	mutex_avpack_a.unlock();

	mutex_avpack_vSos.lock();
	std::cout << "free sos video packet: "<<  avpackData_v_Sos.size()<<std::endl;
	while (!avpackData_v_Sos.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_v_Sos.front();
		av_packet_free(&pack);
		avpackData_v_Sos.pop();
	}
	mutex_avpack_vSos.unlock();

	mutex_avpack_aSos.lock();
	std::cout << "free sos audio packet: "<<  avpackData_a_Sos.size()<<std::endl;
	while (!avpackData_a_Sos.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_a_Sos.front();
		av_packet_free(&pack);
		avpackData_a_Sos.pop();
	}
	mutex_avpack_aSos.unlock();

	return 0;
}

int FFmpegEncoder::addEmergencyStream(std::string url)
{
	if(sos)
		return -1;
	if (decSos) {
		delete decSos;
		decSos = nullptr;
	}
	std::cout << "add   file EmergencyStream_________\n";
	decSos = new DecoderFile();
	int ret = decSos->init_decoder(url);
	if (ret < 0) {
		std::cout << "can't decode Sos stream " << ret << std::endl;
		delete decSos;
		decSos = nullptr;
		return -1;
	}

	return 0;
}

int FFmpegEncoder::addEmergencyStream(std::vector<std::string> url, int64_t interval){
	if(sos)
		return -1;
	if(decSos){
		delete decSos;
		decSos = nullptr;
	}
	std::cout << "add   picture EmergencyStream_________\n";

	decSos = new PictureStream;
	auto failPic = (dynamic_cast<PictureStream*>(decSos))->addPicture(url);
	if(failPic.size() >= url.size()){
		delete decSos;
		decSos = nullptr;
		std::cout << "pic not support \n";
		return -1;
	}
	dynamic_cast<PictureStream*>(decSos)->setImgInterval(interval);
	return 0;
}

int FFmpegEncoder::removeEmergencyStream()
{
	int ret = 0;
	if (decSos) {
		if (decSos->getNext()) {
			std::cout << "output stream is set,can't change input\n";
			ret = -1;
		}
		delete decSos;
		decSos = nullptr;
	}
	return ret;
}

int FFmpegEncoder::switchEmergencyStream()
{
	if(decSos){
		if(!sos){
			sos = true;	

			reinit_Emergency();
			startEncodeEmergency();

			decSos->decoderStream();

			av_usleep(INTERRUPTTIME);
			sos_getPacket = true;
		}
		return 0;
	}

	return -1;
}

int FFmpegEncoder::swicthNormalStream()
{
	if(decSos){
		sos_getPacket = false;
		stopEncodeEmergency();
		decSos->stopStream();
		sos_videoOffset = 0;
		sos_audioOffset = 0;
	//	remove_Emergency();	
	}
	return 0;
}


int FFmpegEncoder::StreamDelay(int64_t delayMs){

	if(delayMs <= 0){
		return 0;
	}
	
	if(funCallback){
		funCallback(0,0);		
	}
	
	int64_t sT = delayMs * 1000;//us
	while(run){
		if(sT > INTERRUPTTIME){
			av_usleep(INTERRUPTTIME);
			sT -=  INTERRUPTTIME;
		}else{
			av_usleep(sT);
			break;
		}
	}

	if(!run){
		return -1;
	}

	int ret = 0;
	if(delayMs >= Config::getConf(CONFPATH)->GetStreamChannelExp()){
		std::cout << "stream delay time >= stream's channel exp "<< delayMs <<" ms\n";
		
		ret = avio_open(&(outFmtCtx->pb), outputUrl.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			std::cout << "avio_open err with " << AVERROR(ret) << std::endl;
			goto errQuit;
		}

		ret = avformat_write_header(outFmtCtx,NULL);
		if (ret < 0) {
			goto errQuit;
		}
		funCallback(0,1);			
	}	

errQuit:
	if(ret < 0){
		std::cout << "after delay stream channel is err\n"; 
		run = false;
		if(ret == -1){
			funCallback(0,4);
		}else{
			funCallback(0,3);
		}
	}

	return ret;
}


int FFmpegEncoder::SendData(int64_t tbs,AVStream *stream,std::queue<AVPacket*>& QueuePack,std::mutex& mux,int64_t& packPts,int64_t& lastT){
	int ret = 0;
	if(tbs < packPts){	
	//	std::cout << " don't need send data\n";
		return 0;
	}

	AVPacket* pack = nullptr;
	
	mux.lock();
	if(QueuePack.size() > 0){
		pack = (AVPacket*)QueuePack.front();
		packPts = pack->pts * av_q2d(stream ->time_base)*1000*1000;
		if(tbs >= packPts){
			QueuePack.pop();
		}else{
			pack = nullptr;
		}
	}
	mux.unlock();


//	std::cout << run << " "<< " pack "<< pack<< " time "<< tbs << " pack pts "<< packPts<<std::endl;
	while(run && pack &&(tbs >= packPts )){
//	std::cout << "send "<< tbs << " pack pts "<< packPts<<std::endl;
		if(!ssBroken){  // if video audio split push
			lastT = packPts;
			ret = av_interleaved_write_frame(outFmtCtx, pack);
		}
		
		if(sendtime){
			sendtime = false;
			std::cout << "get time "<< av_gettime()<<std::endl; 
		}
		av_packet_free(&pack);	
		pack = nullptr;
		if(ret < 0){
			std::cout << "av_interleaved_write_frame err "<< ret <<std::endl; 
			return ret;		
		}

		// compare  //next pack <= tbs get next pack else quit
		mux.lock();
		if(QueuePack.size() > 0){
			pack = (AVPacket*)QueuePack.front();
			int64_t packPts = pack->pts * av_q2d(stream ->time_base)*1000*1000;
			if(packPts <= tbs){
				QueuePack.pop();
			}else{
				pack = nullptr;
			}
		}
		mux.unlock();
	}
	return ret;
}


int FFmpegEncoder::StreamPush()
{
	int ret = 0;
	int64_t lastVT = 0,lastAT = 0;
	int64_t sT = av_gettime(),vT = 0,aT = 0;
	bool needfindKeyPack = false;

	int	NetErr = 1;

	std::unique_lock<std::mutex> locker(cv_mutex);
	while (run) {
		cv.wait_for(locker, std::chrono::seconds(3));
		if (getPacket) {
			locker.unlock();
			std::cout << "get signal \n";
			break;
		}
		std::cout << "wait condition timout__________\n";
	}

	// dealy
	if(taskEnc.export_stream.delay > 0){
		std::cout << "stream delay "<< taskEnc.export_stream.delay <<std::endl;
		ret =  StreamDelay(taskEnc.export_stream.delay);
		if(ret < 0){ 
			sos = false;
			swicthNormalStream();
			return -1;
		}
	}

	
	// get first pack pts
	mutex_avpack_v.lock();
	if (!avpackData_v.empty()) {
		AVPacket* pack = (AVPacket*)avpackData_v.front();
		vT = pack->pts * av_q2d(outFmtCtx->streams[video_index] ->time_base)* 1000 * 1000;
	}else
		vT = 0;
	mutex_avpack_v.unlock();


	mutex_avpack_a.lock();
	if (avpackData_a.empty()) {
		aT = 0;
	}
	else {
		AVPacket* pack = (AVPacket*)avpackData_a.front();
		aT = pack->pts * av_q2d(outFmtCtx->streams[audio_index]->time_base) * 1000 * 1000;
	}
	mutex_avpack_a.unlock();


	std::thread *monitorHandl = nullptr;
	if(funCallback){
		lastVT = vT;
		lastAT = aT;
		monitorHandl = new std::thread([&](){
			this->StreamStatusMonitor(NetErr,lastVT,lastAT);
			});
	}


	sT = av_gettime();
	sT -= (vT > aT) ? vT : aT;
		
	std::cout << "\n		__________start push_________\n\n";
	int64_t tT = av_gettime() - sT;
	std::cout << outputUrl << " now "<< av_gettime() <<" tT "<< tT << " vT first " <<vT << " aT first "<< aT<<std::endl;
	int64_t lastPts = av_gettime();
	while (run) {
		int64_t tT = av_gettime() -sT;

		// for check push stream pts 
		if((av_gettime() - lastPts) > 120*1000*1000){
			lastPts = av_gettime();
//			if(funCallback){
				std::cout << "pushUrl "<< outputUrl<<" now "<< av_gettime() <<" tT "<< tT <<" video time "<< vT << " audio time"<< aT << std::endl;
//			}
		}
		

		// push stream 
		
		ret = SendData(tT,outFmtCtx->streams[video_index],avpackData_v,mutex_avpack_v,vT,lastVT);
		if(ret < 0){
			NetErr = 2;
		}else{
			NetErr = 1;
		}
		
		ret = SendData(tT,outFmtCtx->streams[audio_index],avpackData_a,mutex_avpack_a,aT,lastAT);
		if(ret < 0){
			NetErr = 2;
		}else{
			NetErr = 1;
		}


		// emergency 
		if(sos_getPacket){
			if( /*(abs(tT - vT) < INTERRUPTTIME)*/ sendVideo && decSos->getMediaInfo().hasVideo){
				std::cout << "FFmpegEncoder:: need sync I frame send ";
				needfindKeyPack = true;	
			}	
			std::cout << "start use emergency stream\n"; 
			// set send stream type and record
			bool srcSendV = sendVideo;
			bool srcSendA = sendAudio;
			setSendStreamtype(decSos->getMediaInfo().hasVideo,decSos->getMediaInfo().hasAudio);
			int64_t baseTime = tT;//(lastVT>lastAT)?lastVT:lastAT;
			std::cout << "video now "<< lastVT <<"next time " << vT << " audio now Time " << lastAT << " next Time "<<aT<<std::endl; 
			emcPacketCorrect(baseTime);
			int64_t emTime = av_gettime();
			while(sos_getPacket && run){
				tT = av_gettime() -sT;
				ret = SendData(tT,outFmtCtx->streams[video_index],avpackData_v_Sos,mutex_avpack_vSos,vT,lastVT);
				if(ret < 0){
					NetErr = 2;
				}else{
					NetErr = 1;
				}

				ret = SendData(tT,outFmtCtx->streams[audio_index],avpackData_a_Sos,mutex_avpack_aSos,aT,lastAT);
				if(ret < 0){
					NetErr = 2;
				}else{
					NetErr = 1;
				}

				// drop src data
				if((av_gettime() - emTime) > INTERRUPTTIME){
					int dropSize = dropPacket(vT,outFmtCtx->streams[video_index],avpackData_v,mutex_avpack_v);
					std::cout << "drop video pack "<< dropSize<<std::endl;
					dropSize = dropPacket(aT,outFmtCtx->streams[audio_index],avpackData_a,mutex_avpack_a);
					std::cout << "drop audio pack "<< dropSize<<std::endl;
					emTime = av_gettime();
				}

				if ((tT < aT) || (tT < vT)) {  
					int64_t sleepTime = ((aT < vT) ? vT : aT) - tT;
					if(sleepTime > 5*1000*1000)
						sleepTime = 5*1000*1000;
					av_usleep(sleepTime);
				}
			}


			// stop use emergency stream
			int64_t sosTime = (vT > aT)?vT:aT;
			int dropSize = dropPacket(sosTime,outFmtCtx->streams[video_index],avpackData_v,mutex_avpack_v);
			std::cout << "befor use normal drop video pack "<< dropSize<<std::endl;
			dropSize = dropPacket(sosTime,outFmtCtx->streams[audio_index],avpackData_a,mutex_avpack_a);
			std::cout << "befor use normal drop audio pack "<< dropSize<<std::endl;
			
			while(run && needfindKeyPack){			
				tT = av_gettime() -sT;
				int64_t sleepT = tT - sosTime;
				std::cout << "switch normal stream need sleep "<< sleepT<<std::endl;
				if(sleepT > 0){
					// +1s to make sure get I frame
					av_usleep(sleepT + 1000*1000);
				}

				// drop again ,make sure normal stream is start sosTime
				dropSize = dropPacket(sosTime,outFmtCtx->streams[video_index],avpackData_v,mutex_avpack_v);
				std::cout << "befor use normal drop video pack "<< dropSize<<std::endl;
				dropSize = dropPacket(sosTime,outFmtCtx->streams[audio_index],avpackData_a,mutex_avpack_a);
				std::cout << "befor use normal drop audio pack "<< dropSize<<std::endl;

				// get src stream next key pack time
				int64_t nextKeyPackT = 0;
				while(run){
					mutex_avpack_v.lock();
					if(avpackData_v.size() > 0){
						AVPacket *dp = (AVPacket*) avpackData_v.front();
						if(dp->flags != AV_PKT_FLAG_KEY){
							avpackData_v.pop();
							av_packet_free(&dp);
							mutex_avpack_v.unlock();
						}else{
							nextKeyPackT = dp->pts * av_q2d(outFmtCtx->streams[video_index] ->time_base)* 1000 * 1000;
							mutex_avpack_v.unlock();
							break;
						}
					}else{
						mutex_avpack_v.unlock();
						break;
					}
				}
				std::cout << "find key frame time "<<  nextKeyPackT << " sos time "<< sosTime <<" now  "<< tT <<std::endl; 	

				//find 
				if(nextKeyPackT != 0){
					tT = nextKeyPackT - 40*1000;		
					needfindKeyPack = false;
					dropSize = dropPacket(tT,outFmtCtx->streams[audio_index],avpackData_a,mutex_avpack_a);
					std::cout << "befor use normal drop audio pack "<< dropSize<<std::endl;
					sos = false;
				}

				ret = SendData(tT,outFmtCtx->streams[video_index],avpackData_v_Sos,mutex_avpack_vSos,vT,lastVT);
				if(ret < 0){
					NetErr = 2;
				}else{
					NetErr = 1;
				}

				ret = SendData(tT,outFmtCtx->streams[audio_index],avpackData_a_Sos,mutex_avpack_aSos,aT,lastAT);
				if(ret < 0){
					NetErr = 2;
				}else{
					NetErr = 1;
				}

			}
			sos = false;

			setSendStreamtype(srcSendV,srcSendA);
		}
	

		
		// thread sleep
//		tT = av_gettime() -sT;
		if ((tT < aT) || (tT < vT)) {  
			int64_t sleepTime = ((aT < vT) ? vT : aT) - tT;
			if(sleepTime > 1000*1000)
				sleepTime = 1000*1000;

			av_usleep(sleepTime);
		}	
	}


	avio_close(outFmtCtx->pb);
	if(monitorHandl){
		if(monitorHandl->joinable()){
			monitorHandl->join();
		}
		delete monitorHandl;
		monitorHandl = nullptr;
	}
	std::cout << "stop push stream_____________________\n";
	return 0;
}

int FFmpegEncoder::EmergencyLink::fillBuffer(DataSet p)
{
	enc->mutex_avframe_Sos.lock();
	AVFrame* frame = (AVFrame*)(p.srcData);
	if(p.type == MEDIATYPE::VIDEO){
		frame->pts -= enc->offset_v;
	}else{
		frame->pts -= enc->offset_a;
	}
	enc->avframeData_Sos.push(p);
	enc->mutex_avframe_Sos.unlock();
	return 0;
}


int FFmpegEncoder::streamInfoCheck(int64_t vTime,int64_t aTime,int& lastStatus){
	int64_t diff = vTime -aTime;
	int tmpS = lastStatus;
	if(abs(diff) > 5*1000*1000){ //  diff  number?
		if(vTime > aTime){ //only  has video
			if(sendVideo && !sendAudio)
				tmpS = 5;
		}else{  // only has audio
			if(!sendVideo &&sendAudio)
				tmpS = 6;
		}
	}else{
		if(sendVideo &&sendAudio){
			tmpS = 7;
		}
	}
	
	return tmpS;
}


int FFmpegEncoder::dropPacket(int64_t tbs,AVStream *stream,std::queue<AVPacket*>& QueuePack,std::mutex& mux){
	int ret = 0;
	while(1){
		mux.lock();
		if(QueuePack.size() > 0){
			AVPacket *dp = (AVPacket*) QueuePack.front();
			if(dp->pts * av_q2d(stream->time_base)*1000*1000 < tbs){
				QueuePack.pop();
				av_packet_free(&dp);
				mux.unlock();
				ret++;
				continue;
			}else{
				break;
			}
		}else{
			break;
		}
	}

	mux.unlock();

	return ret ;
}


void FFmpegEncoder::StreamStatusMonitor(int& netStatus,int64_t& videoT,int64_t& audioT){
	int sType = 0;
	while(run){
		av_usleep(INTERRUPTTIME);

		//stream status report
		if(funCallback != nullptr){
			if(ssBroken){
				netStatus = 2;	
			}
			funCallback(0,netStatus);
//			std::cout << "stream status "<< netStatus; 
			sType = streamInfoCheck(videoT,audioT,sType);
			av_usleep(50*1000);
//			std::cout << " stream type "<< snit_EType <<std::endl; 
			funCallback(0,sType);
		}
	}

//	funCallback(0,2);
	std::cout << "FFmpegEncoder:: stop push stream send stream broken signal\n";
}

int64_t FFmpegEncoder::emcPacketCorrect(int64_t baseTime){

	mutex_avpack_vSos.lock();
	if(!avpackData_v_Sos.empty()){
		sos_videoOffset = avpackData_v_Sos.front()->pts - baseTime/1000.0/1000.0/av_q2d(outFmtCtx->streams[video_index]->time_base) ;
		std::cout << "so video start pts "<< avpackData_v_Sos.front()->pts << " system video pts "<<   baseTime/1000.0/1000.0/av_q2d(outFmtCtx->streams[video_index]->time_base)<< " offset "<<  sos_videoOffset <<std::endl;	
		std::queue<AVPacket*>	tmpVP;
		while(!avpackData_v_Sos.empty()){
			AVPacket* packet = avpackData_v_Sos.front();
			avpackData_v_Sos.pop();
			std::cout << "set sos video pts from "<< packet->pts << " to "<<  packet->pts -sos_videoOffset<<std::endl;
			packet->pts -= sos_videoOffset;
			packet->dts = packet->pts;
			tmpVP.push(packet);
		}
		avpackData_v_Sos = tmpVP;		
	}	
	mutex_avpack_vSos.unlock();


	mutex_avpack_aSos.lock();
	if(!avpackData_a_Sos.empty()){
		sos_audioOffset = avpackData_a_Sos.front()->pts -  baseTime/1000.0/1000.0/av_q2d(outFmtCtx->streams[audio_index]->time_base);
		std::cout << " audio start pts "<< avpackData_a_Sos.front()->pts <<" system audio pts "<<  baseTime/1000.0/1000.0/av_q2d(outFmtCtx->streams[audio_index]->time_base)<< " offset "<< sos_audioOffset << std::endl;
		std::queue<AVPacket*> tmpAP;
		while(!avpackData_a_Sos.empty()){
			AVPacket* packet = avpackData_a_Sos.front();
			avpackData_a_Sos.pop();
			packet->pts -= sos_audioOffset;
			packet->dts = packet->pts;
			tmpAP.push(packet);
		}
		avpackData_a_Sos = tmpAP;
	}
	mutex_avpack_aSos.unlock();
	
	return 0;
}


void FFmpegEncoder::serialize(){
	if(fp){
	std::cout << "FFmpegEncoder::serialize()\n";
		
		mutex_syncFile.lock();
		fwrite("\n[StreamPush]",1,sizeof("\n[StreamPush]")-1,fp);
		char url[256]={};
		snprintf(url,256,"\noutUrl=%s",outputUrl.c_str());
		fwrite(url,1,strlen(url),fp);

		char data[4096]={};
		snprintf(data,4096,"\nwidth=%d\nheight=%d\nvideoBits=%ld\nchannel=%d\ndelay=%ld\nfps=%d\n",
			taskEnc.mediaCtx.video_width,taskEnc.mediaCtx.video_height,taskEnc.mediaCtx.video_bits,taskEnc.mediaCtx.audio_channel,taskEnc.export_stream.delay,taskEnc.mediaCtx.fps);
		
		fwrite(data,1,strlen(data),fp);
		
		mutex_syncFile.unlock();
	}
}
