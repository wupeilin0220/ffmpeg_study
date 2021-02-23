#include "AMix.h"
#include "Decoder.h"

AMix::AMix(const MuxContext& attr):
	filterBase(attr),run(false),
	muteFrame(nullptr),mixHandle(nullptr),stepPts(1024) // aac sample size
{
	std::cout << "audio time_base "<< attr.audio_timebase.num<<" "<<attr.audio_timebase.den<<std::endl;

	int64_t start = av_gettime() - Decoder::getDecodeTime();
	std::cout << "AMIX time start "<< start<<std::endl; 
	for(auto &val:lastFramePts){
		val = 0;
	}

	for(auto &val:muteFramePts){
		val = 0;
	}
		

}

AMix::~AMix()
{
	stopMix();
	serialize();
	av_frame_free(&muteFrame);
	for(auto& frameList:frame_buffer){
		while(frameList.size() > 0){
			AVFrame* frame = frameList.front();
			av_frame_free(&frame);
			frameList.pop_front();
		}
	}
}

void AMix::stopMix()
{
	if (mixHandle) {
		run = false;
		if (mixHandle->joinable()) {
			mixHandle->join();
		}
		delete mixHandle;
		mixHandle = nullptr;
	}

	//clear frame buffer
	for(int i(0);i<8;i++){
		mutex_frame[i].lock();
		while(frame_buffer[i].size() > 0){
			AVFrame* frame = frame_buffer[i].front();
			av_frame_free(&frame);
			frame_buffer[i].pop_front();
		}
		mutex_frame[i].unlock();
	}
}

void AMix::startMix()
{
	if (!mixHandle) {
		run = true;
		mixHandle = new std::thread([&]() {
			this->pushAudio();
			});
	}
}

int AMix::BroadData(DataSet& p, int pin)
{
	if (p.type == MEDIATYPE::AUDIO) {
		if (audioWindowID.find(pin) != audioWindowID.end()) {
			mutex_frame[pin].lock(); 
			frame_buffer[pin].push_back((AVFrame*)(p.srcData));
			mutex_frame[pin].unlock();
		}
		else {
			return -1;
		}
	}
	else {
		int ret =-1;
		mutex_link.lock();
		if (link) {
			ret = link->fillBuffer(p);
		}
		mutex_link.unlock();
		return ret;
		
	}
	return 0;
}

int AMix::init_effectfilter(TaskApply& task_)
{
	if (task_.export_stream.audioMix.size() <= 1)
		return -1;
	
	std::vector<AudioInput>& label = task_.export_stream.audioMix;
	// abuffsink
	audio_graph = avfilter_graph_alloc();
	if (audio_graph == NULL) {
		std::cout << "audio_graph alloc fail \n";
		return -1;
	}

	int ret = avfilter_graph_create_filter(&abufferSink_Ctx, avfilter_get_by_name("abuffersink"), "aout0", NULL, NULL, audio_graph);
	if (ret < 0) {
		avfilter_free(abufferSink_Ctx);
		abufferSink_Ctx = nullptr;
		avfilter_graph_free(&audio_graph);
		std::cout << "create abufferSink_Ctx err " << ret << std::endl;
		return ret;
	}

	// abuff
	char args[1024] = {};
	AVFilterInOut* input = avfilter_inout_alloc();
	AVFilterInOut* out = avfilter_inout_alloc();
	AVFilterInOut** ti = &input;
	for (auto val:label) {

		
		audioWindowID.insert(val.id-1);
		if (*ti == nullptr) {
			*ti = avfilter_inout_alloc();
		}

		char ain[16] = {};
		snprintf(ain,16,"[ain%d]",val.id-1);
		strcat(args, ain);
	
		snprintf(ain, 16, "ain%d", val.id - 1);
		char layout[64] = {};
		av_get_channel_layout_string(layout, 64, mediaInfo.audio_channel, mediaInfo.audio_channel_layout);
		char abuff[512] = {};
		snprintf(abuff, 512,"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channels=%d:channel_layout=%s"
			, mediaInfo.audio_timebase.num, mediaInfo.audio_timebase.den, mediaInfo.sample
			, av_get_sample_fmt_name((AVSampleFormat)mediaInfo.audio_format), mediaInfo.audio_channel, layout);

		AVFilterContext* aCtx = nullptr;
		ret = avfilter_graph_create_filter(&aCtx, avfilter_get_by_name("abuffer"), ain, abuff, NULL, audio_graph);
		if (ret < 0) {
			return -1;
		}

		ain_Ctx.push_back(std::make_pair(val.id - 1, aCtx));
		
		(*ti)->filter_ctx = aCtx;
		(*ti)->name = av_strdup(ain);
		(*ti)->pad_idx = 0;
		(*ti)->next = NULL;
		
		
		ti = &(*ti)->next;
	}
	
	char param[64] = {};
//	snprintf(param, 64, "amix=inputs=%lu[a_mix];[a_mix]volume=volume=%f[aout0]", 
//		(unsigned long)label.size(),task_.export_stream.volume);
	snprintf(param, 64, "amix=inputs=%lu[aout0]", 
		(unsigned long)label.size());
	strcat(args, param);
	std::cout << "AMix: input args " << args<<std::endl;


	
	out->filter_ctx = abufferSink_Ctx;
	out->name = av_strdup("aout0");
	out->next = NULL;
	out->pad_idx = 0;

	ret = avfilter_graph_parse_ptr(audio_graph, args, &out, &input , NULL);
	if (ret < 0) {
		std::cout << "AMix: avfilter_graph_parse_ptr\n ";
		goto end;
	}

	ret = avfilter_graph_config(audio_graph, NULL);

	if (makeSilenceFrame(task) == nullptr) {
		std::cout << "AMix: make silence frame fail\n";
	}
end:
	avfilter_inout_free(&input);
	avfilter_inout_free(&out);
	return ret;
}

int AMix::pushAudio()
{
	int ret = 0;
	bool syncflag[8] = {};
	for(int i(0);i< 8;i++){
		syncflag[i] = false;
	}
	int64_t lastT = av_gettime();
	syncTime(run);

	while (run) {
		int64_t now = av_gettime();
		int64_t sT = (now - lastT) / 1000;
		lastT = now;
		if (sT < 22 * 1000) {
			av_usleep(20 * 1000 - sT);
		}

		for (auto srcIn : ain_Ctx) {

			int index = srcIn.first;
			mutex_frame[index].lock();
			if (frame_buffer[index].size() > 0) {
				if(syncflag[index]){
					std::cout << "audio stream is broken\n";
					mutex_frame[index].unlock();
			//		int64_t nowPts = av_rescale_q((av_gettime() - Decoder::getDecodeTime()),mediaInfo.video_timebase,mediaInfo.audio_timebase);
					int64_t syncT = syncTime(muteFramePts[index],index);
					if(syncT < muteFramePts[index]){
						//						streamInterruptDetect(srcIn);
						muteFrame->pts = muteFramePts[index]; // add one sample

						int ret = av_buffersrc_add_frame_flags(srcIn.second, muteFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
						if (ret < 0) {
							std::cout << "audioMix: av_buffersrc_add_frame_flags err\n";
						}
					//	muteFramePts[index] += stepPts;
						continue;
					}

					syncflag[index] = false;
					mutex_frame[index].lock();
				}
				
				lastFramePts[index] = av_gettime();	
				muteFramePts[index] = frame_buffer[index].front()->pts;
				ret = av_buffersrc_add_frame_flags(srcIn.second, frame_buffer[index].front(), 0);
				if (ret < 0) {
					std::cout << "AMix: av_buffersrc_add_frame_flags err\n";
					mutex_frame[index].unlock();
				//	break;
				}
				av_frame_free(&(frame_buffer[index].front()));
				frame_buffer[index].pop_front();
			}
			else {
				// stream broken
				syncflag[index] =  streamInterruptDetect(srcIn);
			}
			mutex_frame[index].unlock();
		}


	
		while (run) {
			AVFrame* frame = av_frame_alloc();
			ret = av_buffersink_get_frame_flags(abufferSink_Ctx, frame, 0);
			if (ret < 0) {
				av_frame_free(&frame);
				break;
			}

			DataSet data;
			data.type = MEDIATYPE::AUDIO;
			data.srcData = frame;
			if (!link || (link->fillBuffer(data) < 0)) {
				av_frame_free(&frame);
			}
		}
	}
	std::cout << "AMix:  audio mix thread is quit...\n";
	return 0;
}

AVFrame* AMix::makeSilenceFrame(TaskApply& task)
{
	// silence frame
	std::cout << "AMIX::MakeSilenceFrame sample_rate " << task.mediaCtx.sample<< "\nchannel_layout "<< task.mediaCtx.audio_channel_layout<<"\nchannels "<< task.mediaCtx.audio_channel<<"\nformat "<<task.mediaCtx.audio_format<<std::endl;
	muteFrame = av_frame_alloc();
	muteFrame->sample_rate = task.mediaCtx.sample;
	muteFrame->channel_layout = task.mediaCtx.audio_channel_layout;
	muteFrame->channels = task.mediaCtx.audio_channel;
	muteFrame->format = task.mediaCtx.audio_format;
	muteFrame->nb_samples = 1024; // aac sample
	int ret = av_frame_get_buffer(muteFrame, 0);
	if (ret < 0) {
		av_frame_free(&muteFrame);
		muteFrame = nullptr;
	}
	av_samples_set_silence(muteFrame->data, 0, muteFrame->nb_samples, muteFrame->channels,(AVSampleFormat)muteFrame->format);
	return muteFrame;
}

int64_t AMix::syncTime(volatile bool& run){
	int64_t syncT = 0;
	for(auto id:audioWindowID){
		bool loop = false;
		do{
			if(frame_buffer[id].size() > 0){
				int64_t pts = frame_buffer[id].front()->pts;
				if(pts > syncT){
					syncT = pts;
				}
				loop = false;
			}else{
				loop = true;
				av_usleep(40*1000);
//				std::cout << "loop\n";
			}
		}while(run && loop);
	}

	std::cout << "audio sync time "<< syncT <<std::endl;

	for(auto id:audioWindowID){
		while(run && syncTime(syncT,id) < syncT);
	}

	return syncT;
}

int64_t AMix::syncTime(int64_t syncT,int index){
	std::cout << "audio sync pts"<< syncT<<std::endl;	
	int64_t framePts = 0;
	int threshold = 10; 
	for(auto id:audioWindowID){
		if(id != index)
			continue;
		while(id >= 0 && threshold > 0){
			mutex_frame[id].lock();
			while(frame_buffer[id].size() > 1){
				if(frame_buffer[id].front()->pts < syncT){
					AVFrame* frame = frame_buffer[id].front();
					framePts = frame->pts;
					av_frame_free(&frame);
					frame_buffer[id].pop_front();
				}else{
					framePts = frame_buffer[id].front()->pts;
					break;
				}
			}
			mutex_frame[id].unlock();

			if(framePts >= syncT){
				break;
			}
			threshold--;		
			av_usleep(40*1000);
		}
	}
	return framePts;
}

void AMix::serialize(){
	if(fp){	
		auto& input = task.export_stream.audioMix;
		if(input.size() > 1){
			mutex_syncFile.lock();
			int i = 0;

			for(auto& chain: input){
				char head[64]={};
				snprintf(head,64,"\n[AudioMix_%d]",i);
				fwrite(head,1,strlen(head),fp);

				char tmp[64]={};
				snprintf(tmp,64,"\naudioID=%d\nvolume=%f",chain.id,chain.volume);
				fwrite(tmp,1,strlen(tmp),fp);

				i++;	
			}
			mutex_syncFile.unlock();
		}
	}	
}

bool AMix::streamInterruptDetect(AINCTX& ctx){
	bool broken = false;
	int index = ctx.first;

//	int64_t nowPts = av_rescale_q((av_gettime() - Decoder::getDecodeTime()),mediaInfo.video_timebase,mediaInfo.audio_timebase);
	int64_t nowPts = av_gettime();

//	if (lastFramePts[index] > 0 && (nowPts - lastFramePts[index]) > 10 / av_q2d(mediaInfo.audio_timebase)) {
	if (lastFramePts[index] > 0 && (nowPts - lastFramePts[index] > 5*1000*1000)) {
		broken = true;
		// video_timebase same as AVRational{1,1000000}
		int64_t targetPts = av_rescale_q((av_gettime() - Decoder::getDecodeTime()),mediaInfo.video_timebase,mediaInfo.audio_timebase);
		while((muteFramePts[index] + stepPts) < targetPts){
			muteFrame->pts = muteFramePts[index]; // add one sample

			int ret = av_buffersrc_add_frame_flags(ctx.second, muteFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
			if (ret < 0) {
				std::cout << "audioMix: av_buffersrc_add_frame_flags err\n";
			}
			muteFramePts[index] += stepPts;
		}
	}
	return broken;
}
