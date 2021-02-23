#include "VMix.h"
#include "Decoder.h"

static bool compare(const Position& p1, const Position& p2) {
	return p1.level < p2.level;
}
VMix::VMix(const MuxContext& attr) :
	filterBase(attr),
	run(false), mixHandle(nullptr),step(40000)
{

//	int64_t start = av_gettime() - Decoder::getDecodeTime();
	int64_t start = av_gettime();
	for(auto& val:lastFramePts){
		val = start;
	}

	std::cout << "VMIX time start "<< start<<std::endl; 
	blackFrame = av_frame_alloc();
	blackFrame->width = attr.video_width;
	blackFrame->height = attr.video_height;
	blackFrame->format = AV_PIX_FMT_YUV420P;
	av_frame_get_buffer(blackFrame, 0);

	// fill data // black 
	memset(blackFrame->data[0], 0, (int64_t)blackFrame->linesize[0] * blackFrame->height);
	memset(blackFrame->data[1], 0x80, blackFrame->linesize[1] * blackFrame->height / 2);
	memset(blackFrame->data[2], 0x80, blackFrame->linesize[2] * blackFrame->height / 2);
}

VMix::~VMix()
{
	stopMix();	
	serialize();
	av_frame_free(&blackFrame);
	for(auto& frameList:frame_buffer){
		while(frameList.size() > 0){
			AVFrame* frame = frameList.front();
			av_frame_free(&frame);
			frameList.pop_front();
		}
	}
	
}

void VMix::stopMix()
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

void VMix::startMix()
{
	if (!mixHandle) {
		run = true;
		mixHandle = new std::thread([&]() {
			this->pushVideo();
			});
	}
}

int VMix::BroadData(DataSet& p, int pin)
{
	if (p.type == MEDIATYPE::VIDEO) {
		if (videoWindowID.find(pin+1) != videoWindowID.end()) {
			mutex_frame[pin+1].lock();  // 这里有个风险，若是输入流不在混合序列会导致内存无限增大
			frame_buffer[pin+1].push_back((AVFrame*)(p.srcData));
			mutex_frame[pin+1].unlock();
		}
		else {
			return -1;
		}
	}
	else {
		mutex_link.lock();
		int ret = -1;
		if (link && pin == 0) {
			ret = link->fillBuffer(p);
		}
		mutex_link.unlock();
		return ret;
	}

	return 0;
}

int VMix::init_effectfilter(TaskApply& task_)
{
	std::vector<Position>& label = task_.export_stream.videoMix;
	
	std::sort(label.begin(), label.end(), compare);

	video_graph = avfilter_graph_alloc();
	if (video_graph == NULL) {
		std::cout << "video_graph alloc fail \n";
		return -1;
	}
	// buffsink
	int ret = avfilter_graph_create_filter(&vbufferSink_Ctx, avfilter_get_by_name("buffersink"), "vout0", NULL, NULL, video_graph);
	if (ret < 0) {
		avfilter_free(vbuffer_Ctx);
		vbuffer_Ctx = nullptr;
		avfilter_graph_free(&video_graph);
		std::cout << "create vbufferSink_Ctx err " << ret << std::endl;
		return ret;
	}

	bool first = true;
	char args[1024] = {};
	AVFilterInOut* input = avfilter_inout_alloc();
	AVFilterInOut* out = avfilter_inout_alloc();
	int i = 0;
	for (auto in : label) {

		videoWindowID.insert(in.id);
		char tmp[128] = {};
		if (first) {
			snprintf(tmp, 128, "[vin%d]scale=%d:%d,pad=%d:%d:%d:%d[out_scale_%d]",
				in.id, in.rect.x, in.rect.y, task_.mediaCtx.video_width, task.mediaCtx.video_height, in.rect.offset_x, in.rect.offset_y, i);
			first = false;
		}
		else {
			snprintf(tmp, 128, ";[vin%d]scale=%d:%d[out_tmp_%d];[out_scale_%d][out_tmp_%d]overlay=%d:%d:repeatlast=1[out_scale_%d]",
				in.id, in.rect.x, in.rect.y, i, i, i, in.rect.offset_x, in.rect.offset_y, i + 1); // i++ 无法使用？？
			i += 1;
		}

		strcat(args, tmp);
	//	auto iter = 
	//	if(iter.first != videoWindowID.end() && iter.second)
		ff_BuffInput(in.id, input);
	}
	std::string str(args);
	size_t offset = str.find_last_of('[');

	snprintf(args + offset, 8, "[vout0]");
	std::cout << "\tinput args:	" << args << std::endl;


	out->filter_ctx = vbufferSink_Ctx;
	out->name = av_strdup("vout0");
	out->next = NULL;
	out->pad_idx = 0;
	ret = avfilter_graph_parse_ptr(video_graph, args, &out, &input, NULL);
	if (ret < 0) {
		std::cout << "videoMix: avfilter_graph_parse_ptr\n ";
		goto end;
	}

	ret = avfilter_graph_config(video_graph, NULL);

end:
	avfilter_inout_free(&input);
	avfilter_inout_free(&out);
	return ret;
}

int VMix::pushVideo()
{
	int ret = 0;
	bool brokenflag[9];
	for(int i(0);i<9;i++){
		brokenflag[i] = false;
	}

//	bool SYNCALL = false;
	int64_t lastT = av_gettime();
//	bool sendOneFrame = false;
//	av_usleep(5 * 1000 * 1000);
//	int64_t StartPts = syncTime();
	blackFramePts[0] = syncTime(run); 
	for (auto srcIn : vin_Ctx) {
		int index = srcIn.first;
		mutex_frame[index].lock();
		std::cout << "video input " << index << " pts " << frame_buffer[index].front()->pts << std::endl;
		mutex_frame[index].unlock();
	}

	while (run) {
		int64_t now = av_gettime();
		int64_t sT = (now - lastT) / 1000;
		lastT = now;
		if (sT < 35 * 1000) {
			av_usleep(35 * 1000 - sT);
		}


		for (auto srcIn : vin_Ctx) {
			int index = srcIn.first;
			mutex_frame[index].lock();
			if (frame_buffer[index].size() > 0) {

				if(brokenflag[index]){
					mutex_frame[index].unlock();
					
				//	int64_t nowPts = av_gettime() - Decoder::getDecodeTime();
					int64_t framePts = syncTime(blackFramePts[index],index);
					if(blackFramePts[index] > framePts){
						//streamInterruptDetect(srcIn);
						blackFrame->pts = blackFramePts[index]; // add one fps

						int ret = av_buffersrc_add_frame_flags(srcIn.second, blackFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
						if (ret < 0) {
							std::cout << "videoMix: av_buffersrc_add_frame_flags err\n";
						}
						blackFramePts[index] += step;
						continue;
					}

					brokenflag[index] = false;
					mutex_frame[index].lock();
				}
				lastFramePts[index] = av_gettime();
				blackFramePts[index] = frame_buffer[index].front()->pts;
				ret = av_buffersrc_add_frame_flags(srcIn.second, frame_buffer[index].front(), AV_BUFFERSRC_FLAG_KEEP_REF);
				if (ret < 0) {
					std::cout << "videoMix: av_buffersrc_add_frame_flags err\n";
				}
			}
			else {
//				if(index == 0 && !sendOneFrame){
//					sendOneFrame = true;
//					blackFrame->pts = blackFramePts[0];
//					std::cout << " blackFramePts "<< blackFrame->pts<<std::endl;
//					int ret = av_buffersrc_add_frame_flags(srcIn.second, blackFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
//					if (ret < 0) {
//						std::cout << "videoMix: id 0 window send black frame  av_buffersrc_add_frame_flags err\n";
//					}
//				}else{
					brokenflag[index] = streamInterruptDetect(srcIn);
//				}
			}
			mutex_frame[index].unlock();
		}

		
		for (auto srcIn : vin_Ctx) {

			mutex_frame[srcIn.first].lock();

			if (frame_buffer[srcIn.first].size() > 0 && !brokenflag[srcIn.first]) {
				AVFrame* frame = frame_buffer[srcIn.first].front();
				frame_buffer[srcIn.first].pop_front();
				av_frame_unref(frame);
				av_frame_free(&frame);
			}

			mutex_frame[srcIn.first].unlock();
		}


		while (run) {
			AVFrame* frame = av_frame_alloc();
			ret = av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
			if (ret < 0) {
				av_frame_free(&frame);
				break;
			}

			DataSet data;
			data.type = MEDIATYPE::VIDEO;
			data.srcData = frame;
			mutex_link.lock();
			if (!link || (link->fillBuffer(data) < 0)) {
				av_frame_free(&frame);
			}
			mutex_link.unlock();
		}
//		blackFramePts[0] += step;
	}
	std::cout << "VMix: mix thread is quit...\n";
	return 0;
}

int VMix::ff_BuffInput(int index, AVFilterInOut* chain)
{
	//init buff
	char vin[16] = {};
	snprintf(vin, 16, "vin%d", index);
	char args[512] = {};
	snprintf(args, 512, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d"
		, mediaInfo.video_width, mediaInfo.video_height, mediaInfo.video_format
		, mediaInfo.video_timebase.num, mediaInfo.video_timebase.den, 0, 1);

	AVFilterContext* vCtx = nullptr;
	int ret = avfilter_graph_create_filter(&vCtx, avfilter_get_by_name("buffer"), vin, args, NULL, video_graph);
	if (ret < 0) {
		return -1;
	}

	vin_Ctx.push_back(std::make_pair(index, vCtx));

	if (chain->filter_ctx) {

		AVFilterInOut* in = avfilter_inout_alloc();
		in->filter_ctx = vCtx;
		in->name = av_strdup(vin);
		in->next = NULL;
		in->pad_idx = 0;
		AVFilterInOut* tmp = chain;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = in;
	}
	else {
		chain->filter_ctx = vCtx;
		chain->name = av_strdup(vin);
		chain->next = NULL;
		chain->pad_idx = 0;
	}
	return 0;
}

int64_t VMix::syncTime(volatile bool& run){
	int64_t syncT = 0;
	for(auto id:videoWindowID){ // get sync pts (use max)
		if(id == 0)
			continue;
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
			}
		}while(run && loop);
	}


	// drop all <= syncT frames
	std::cout << "video sync time "<< syncT <<std::endl;

	for(auto id:videoWindowID){
		if(id == 0)
			continue;
		while(run && syncTime(syncT,id) < syncT){
			std::cout << "continue  drop video frame \n";
		}
	}
	std::cout << "finsh sync video \n";
	return syncT;
}

int64_t VMix::syncTime(int64_t syncT,int index){	
	int64_t framePts = 0;
	for(auto id:videoWindowID){
		if(id != index)
			continue;

		int threshold = 5;
		while(id > 0 && threshold > 0){
			mutex_frame[id].lock();
			while(frame_buffer[id].size() > 1){ // prevent streamInterruptDetect set stream broken flag
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
	std::cout << "video dst " << syncT << " now "<< framePts <<" finsh sync\n";
	return framePts;
}

void VMix::serialize(){
	if(fp){
		mutex_syncFile.lock();

		if(task.export_stream.videoMix.size() > 0){
			int i = 0;
			for(auto val: task.export_stream.videoMix){
				char head[64]={};
				snprintf(head,64,"\n[VideoMix_%d]",i);
				fwrite(head,1,strlen(head),fp);
				char tmp[256]={};
				snprintf(tmp,256,"\nvideoID=%d\nwidth=%d\nheight=%d\nx=%d\ny=%d\nlevel=%d",
					val.id,val.rect.x,val.rect.y,val.rect.offset_x,val.rect.offset_y,val.level);

				fwrite(tmp,1,strlen(tmp),fp);
				i++;
			}
		}
		mutex_syncFile.unlock();
	}
}

bool VMix::streamInterruptDetect(VINCTX& ctx){
	
	bool broken = false;
	int index = ctx.first;
//	int64_t nowPts = 
	int64_t nowPts = av_gettime();
//	if (lastFramePts[index] > 0 && (nowPts - lastFramePts[index]) > 5 / av_q2d(mediaInfo.video_timebase)) {
	if (lastFramePts[index] > 0 && (nowPts - lastFramePts[index] > 5 *1000*1000)) {
		broken = true;
		int64_t targetPts = (nowPts - Decoder::getDecodeTime())/1.0/1000/1000 / av_q2d(AVRational{1,AV_TIME_BASE});
		while((blackFramePts[index] + step) < targetPts){
		//	std::cout << "add black frame "<< index <<std::endl;
			blackFrame->pts = blackFramePts[index]; // add one fps

			int ret = av_buffersrc_add_frame_flags(ctx.second, blackFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
			if (ret < 0) {
				std::cout << "videoMix: av_buffersrc_add_frame_flags err\n";
			}
			blackFramePts[index] += step;
		}
	}	
	return broken;
}
