#pragma once
#include "MediaBase.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include "libavutil/audio_fifo.h"
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
};
class filterBase
{
public:
	filterBase(const MuxContext& attr) :
		mediaInfo(attr),
		video_graph(nullptr),audio_graph(nullptr),
		vbuffer_Ctx(nullptr),vbufferSink_Ctx(nullptr),
		abuffer_Ctx(nullptr),abufferSink_Ctx(nullptr),task()
	{
		
	}
	virtual ~filterBase();

	virtual int init_filter(TaskApply& task_);
protected:

	// init buffer£¬buffersink
	virtual int init_videoInOutput(TaskApply& task_);
	virtual int init_audioInOutput(TaskApply& task_);
	virtual int init_effectfilter(TaskApply& task_) = 0;

protected:
	const MuxContext mediaInfo;

	AVFilterGraph* video_graph;
	AVFilterGraph* audio_graph;

	AVFilterContext* vbuffer_Ctx;
	AVFilterContext* vbufferSink_Ctx;

	AVFilterContext* abuffer_Ctx;
	AVFilterContext* abufferSink_Ctx;
	
	TaskApply			task;
	
};

