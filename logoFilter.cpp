#include "logoFilter.h"
#include <algorithm>
#include "Decoder.h" 
#include "ResourceCreater.h"

int logoFilter::logoNum = 0;
logoFilter::logoFilter(const MuxContext& attr):filterBase(attr),
	vbuffer_movie(nullptr),
	first(true)
{
	
}


logoFilter::~logoFilter()
{
	serialize();
	if (vbuffer_movie) {
		avfilter_free(vbuffer_movie);
		vbuffer_movie = nullptr;
	}
	std::cout << "logoFilter disstruct\n";
}

int logoFilter::fillBuffer(DataSet p)
{
	if (p.type == MEDIATYPE::AUDIO) {
		if (link) {
			return link->fillBuffer(p);
		}
	}
	else {
		return pushVideo(p);
	}

	return -1;
}

int logoFilter::init_effectfilter(TaskApply& task_)
{
	if (!mediaInfo.hasVideo)
		return -1;

	if (task_.getType() != ApplyType::LOGO)
		return -1;

	char args[1024] = {};
	int loop = 1;
	std::string str(task.logo.name);
	auto p = str.find_last_of('.');
	if (p == std::string::npos) {
		return -1;
	}
	p +=1;
	if ((task.logo.name + p) == std::string("mp4") || (task.logo.name + p) == std::string("MP4") || (task.logo.name + p) == std::string("flv")
		|| (task.logo.name + p) == std::string("FLV") || (task.logo.name + p) == std::string("avi") || (task.logo.name + p) == std::string("AVI") 
		|| (task.logo.name + p) == std::string("rmvb") || (task.logo.name + p) == std::string("RMVB")|| (task.logo.name + p) == std::string("gif")
		|| (task.logo.name + p) == std::string("mkv")) {
		loop = 0;
	}

	snprintf(args, 1024, "filename='%s':loop=%d",task.logo.name,loop);
	int ret = avfilter_graph_create_filter(&vbuffer_movie, avfilter_get_by_name("movie"), "movie",args, NULL, video_graph);
	if (ret < 0)
		return ret;
	AVFilterInOut* in = avfilter_inout_alloc();
	AVFilterInOut* out = avfilter_inout_alloc();
	AVFilterInOut* movie = avfilter_inout_alloc();

	out->filter_ctx = vbuffer_Ctx;
	out->name = av_strdup("vin0");
	out->next = movie;
	out->pad_idx = 0;

	movie->filter_ctx = vbuffer_movie;
	movie->name = av_strdup("movie");
	movie->next = NULL;
	movie->pad_idx = 0;

	in->filter_ctx = vbufferSink_Ctx;
	in->name = av_strdup("vout0");
	in->next = NULL;
	in->pad_idx = 0;
	
	double Alpha = task_.logo.Alpha;
	Alpha = (Alpha < 0) ? 0:Alpha;
	Alpha = (Alpha > 1.0) ? 1.0:Alpha;
	memset(args, 0, 1024);
	if(loop == 0){
		snprintf(args, 1024, "[movie]setpts=(N+ FRAME_RATE *%lf )/FRAME_RATE/TB,scale=%d:%d,format=rgb32,colorchannelmixer = aa = %1f[ms];[vin0][ms]overlay=%d:%d:eof_action=pass[vout0]", 
				(av_gettime()-Decoder::getDecodeTime())/1.0/AV_TIME_BASE,task_.logo.rect.x, task_.logo.rect.y,Alpha,task_.logo.rect.offset_x, task_.logo.rect.offset_y);
	}else{
		snprintf(args, 1024, "[movie]setpts=N/FRAME_RATE/TB,scale=%d:%d,format=yuva420p,colorchannelmixer = aa = %1f[ms];[vin0][ms]overlay=%d:%d[vout0]", 	
				task_.logo.rect.x, task_.logo.rect.y,Alpha,task_.logo.rect.offset_x, task_.logo.rect.offset_y);
	}
	std::cout << "logo args "<< args<<std::endl;
	ret = avfilter_graph_parse_ptr(video_graph, args,  &in, &out, NULL);
	if (ret < 0)
		goto end;

	ret = avfilter_graph_config(video_graph, NULL);
	if(ret > 0){
		AVFrame* frame = av_frame_alloc();
		av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
		av_frame_free(&frame);
	}
end:
	avfilter_inout_free(&in);
	avfilter_inout_free(&out);
	return ret;
}



int logoFilter::pushVideo(DataSet& p)
{
	AVFrame* frame_src = (AVFrame*)p.srcData;
	int64_t pts = frame_src->pts; 
	if(first){
		std::cout << " vin time "<<pts*av_q2d(mediaInfo.video_timebase) <<std::endl;
	}
	int ret = av_buffersrc_add_frame_flags(vbuffer_Ctx, frame_src, 0/*AV_BUFFERSRC_FLAG_KEEP_REF*/);
	if (ret < 0) {
		av_frame_free(&frame_src);
		return ret;
	}
	

	while (true) {
		AVFrame* frame = av_frame_alloc();
		int64_t s =av_gettime();
		ret = av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
		if (ret < 0 ) {
			av_frame_free(&frame);
			av_frame_free(&frame_src);
			if (ret == AVERROR(EAGAIN))
				return 0;
			else if (ret == AVERROR_EOF) {
				DataSet data;
				data.type = MEDIATYPE::VIDEO;
				data.srcData = nullptr;
				if (link) {
					link->fillBuffer(data);
				}			
			}else
				av_frame_free(&frame);
			return ret;
		}
		if(first){
			first = false;
			std::cout << "time "<<(av_gettime() - s)/1000.0<< " vin time "<<pts*av_q2d(mediaInfo.video_timebase) <<std::endl;
		}
		DataSet data;
		data.type = MEDIATYPE::VIDEO;
		data.srcData = frame;
		if (link) {
			if(link->fillBuffer(data) < 0){
				av_frame_free(&frame);		
			}
		}else{
			av_frame_free(&frame);	
		}

	}
	av_frame_free(&frame_src);
	return 0;
}

void logoFilter::serialize(){
	if(fp){
		mutex_syncFile.lock();
		char logoIndex[32]={};
		snprintf(logoIndex,32,"\n[logo_%d]",logoNum);
		fwrite(logoIndex,1,strlen(logoIndex),fp);
		
		std::string urlMovie = ResourceCreater::FindSourceUrl(std::string(task.logo.name));
		char param[2048]={};
		snprintf(param,1024,"\nlogoUrl=%s\nwidth=%d\nheight=%d\noffset_x=%d\noffset_y=%d\nalpha=%f\nid=%s\nlevel=%d",
			urlMovie.c_str(),task.logo.rect.x,task.logo.rect.y,task.logo.rect.offset_x,task.logo.rect.offset_y,task.logo.Alpha,task.logo.id,task.logo.level);
		fwrite(param,1,strlen(param),fp);
		mutex_syncFile.unlock();
		logoNum++;
	}
}

