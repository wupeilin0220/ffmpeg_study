// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "filter.h"


filterBase::~filterBase()
{
	if (vbuffer_Ctx) {
		avfilter_free(vbuffer_Ctx);
	}

	if (vbufferSink_Ctx) {
		avfilter_free(vbufferSink_Ctx);
	}

	if (abuffer_Ctx) {
		avfilter_free(abuffer_Ctx);
	}

	if (abufferSink_Ctx) {
		avfilter_free(abufferSink_Ctx);
	}

	if (video_graph) {
		avfilter_graph_free(&video_graph);
	}

	if (audio_graph) {
		avfilter_graph_free(&audio_graph);
	}
}

int filterBase::init_filter(TaskApply& task_)
{
	this->task = task_;
	int ret = 0;
	if(mediaInfo.hasVideo){
		ret = init_videoInOutput(task_);
		if(ret < 0)
			return ret;
	}

	if(mediaInfo.hasAudio){	
		ret = init_audioInOutput(task_);
		if(ret < 0){
			return ret;
		}
	}

	ret = init_effectfilter(task);
	return ret;
}

int filterBase::init_videoInOutput(TaskApply& )
{
	if (!mediaInfo.hasVideo)
		return -1;

	if (mediaInfo.video_height <= 0 || mediaInfo.video_width <= 0) 
		return -1;

	video_graph = avfilter_graph_alloc();
	if(video_graph == NULL){
		std::cout << "video_graph alloc fail \n";
		return -1;
	}
	char args[512] = {};
	snprintf(args, 512, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d"
		, mediaInfo.video_width,mediaInfo.video_height,mediaInfo.video_format
		,mediaInfo.video_timebase.num,mediaInfo.video_timebase.den,0,1);

	int ret = avfilter_graph_create_filter(&vbuffer_Ctx, avfilter_get_by_name("buffer"), "vin0", args, 0, video_graph);
	if (ret < 0) {
		std::cout << "create vbuffe_Ctx err " << ret << std::endl;
		avfilter_graph_free(&video_graph);
		return ret;
	}
	
	ret = avfilter_graph_create_filter(&vbufferSink_Ctx, avfilter_get_by_name("buffersink"), "vout0", NULL, NULL, video_graph);
	if (ret < 0) {
		avfilter_free(vbuffer_Ctx);
		vbuffer_Ctx = nullptr;
		avfilter_graph_free(&video_graph);
		std::cout << "create vbufferSink_Ctx err " << ret << std::endl;
		return ret;
	}

	return 0;
}

int filterBase::init_audioInOutput(TaskApply& )
{
	if (!mediaInfo.hasAudio)
		return -1;
	if (mediaInfo.audio_channel <= 0) {
		return -1;
	}

	audio_graph = avfilter_graph_alloc();
	if(audio_graph == NULL){
		std::cout << "create audio_graph fail \n";
		return -1;
	}
	char layout[64] = {};
	av_get_channel_layout_string(layout, 64, mediaInfo.audio_channel, mediaInfo.audio_channel_layout);

	char args[512] = {};
	sprintf(args,"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channels=%d:channel_layout=%s"
		, mediaInfo.audio_timebase.num, mediaInfo.audio_timebase.den, mediaInfo.sample
		, av_get_sample_fmt_name((AVSampleFormat)mediaInfo.audio_format), mediaInfo.audio_channel, layout);

	int ret = avfilter_graph_create_filter(&abuffer_Ctx, avfilter_get_by_name("abuffer"), "ain0", args, NULL, audio_graph);
	if (ret < 0) {
		avfilter_graph_free(&audio_graph);
		std::cout << "avfilter_graph_create_filter err buffer_in " << ret << std::endl;
		return ret;
	}

	ret = avfilter_graph_create_filter(&abufferSink_Ctx, avfilter_get_by_name("abuffersink"),"aout0", NULL, NULL, audio_graph);
	if (ret < 0) {
		avfilter_free(abuffer_Ctx);
		abuffer_Ctx = nullptr;
		avfilter_graph_free(&audio_graph);
		std::cout << "avfilter_graph_create_filter err buffer_out " << ret << std::endl;
		return ret;
	}

	return 0;
}
