#include "FadeFilter.h"

FadeFilter::FadeFilter(const MuxContext& attr):
	filterBase(attr),
	vfade(nullptr),
	afade(nullptr)
{

}

FadeFilter::~FadeFilter()
{

}

int FadeFilter::fillBuffer(DataSet p)
{
	if (p.type == MEDIATYPE::VIDEO) {
		return pushStream(p, vbuffer_Ctx, vbufferSink_Ctx, MEDIATYPE::VIDEO);
	}
	else {
		return pushStream(p, abuffer_Ctx, abufferSink_Ctx,  MEDIATYPE::AUDIO);
	}
	return -1;
}

int FadeFilter::init_effectfilter(TaskApply& task_)
{
	initVideoFade(task_);
	initAudioFade(task_);
	return 0;
}

int FadeFilter::initVideoFade(TaskApply& )
{
	char params[1024] = {};
	snprintf(params, 1023, "t='in':st = 40:d = 5");
	int ret = avfilter_graph_create_filter(&vfade, avfilter_get_by_name("fade"), "vfadein", params, NULL, video_graph);
	if (ret < 0) {
		std::cout << "fade context create fail " << ret << std::endl;
		return ret;
	}

	ret = avfilter_link(vbuffer_Ctx, 0, vfade, 0);
	if (ret < 0) {
		return ret;
	}

	ret = avfilter_link(vfade, 0, vbufferSink_Ctx, 0);
	if (ret < 0) {
		return ret;
	}

	ret = avfilter_graph_config(video_graph, NULL);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int FadeFilter::initAudioFade(TaskApply& )
{
	char params[1024] = {};
	snprintf(params, 1023, "t='in':st = 40:d = 5");
	int ret = avfilter_graph_create_filter(&afade, avfilter_get_by_name("afade"), "afadein", params, NULL, audio_graph);
	if (ret < 0) {
		std::cout << "fade context create fail " << ret << std::endl;
		return ret;
	}

	ret = avfilter_link(abuffer_Ctx, 0, afade, 0);
	if (ret < 0) {
		return ret;
	}

	ret = avfilter_link(afade, 0, abufferSink_Ctx, 0);
	if (ret < 0) {
		return ret;
	}

	ret = avfilter_graph_config(audio_graph, NULL);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

int FadeFilter::pushVideo(DataSet& p)
{
	AVFrame* frame_src = (AVFrame*)p.srcData;
	int ret = av_buffersrc_add_frame_flags(vbuffer_Ctx, frame_src, 0);
	if (ret < 0) {
		return ret;
	}
	while (true) {
		AVFrame* frame = av_frame_alloc();
		ret = av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
		if (ret < 0) {
			if (ret == AVERROR(EAGAIN))
				return 0;
			else if (ret == AVERROR_EOF) {
				DataSet data;
				data.type = MEDIATYPE::VIDEO;
				data.srcData = nullptr;
				if (link) {
					link->fillBuffer(data);
				}
			}
			else
				av_frame_free(&frame);
			return ret;
		}
		
		DataSet data;
		data.type = MEDIATYPE::VIDEO;
		data.srcData = frame;
		
		if (link) {
			link->fillBuffer(data);
		}

		if (!frame_src)
		{
			DataSet data;
			data.type = MEDIATYPE::VIDEO;
			data.srcData = nullptr;
			if (link) {
				link->fillBuffer(data);
			}

			break;
		}
		av_frame_free(&frame_src);
	}
	av_frame_free(&frame_src);
	return 0;
}

int FadeFilter::pushStream(DataSet& p, AVFilterContext* in, AVFilterContext* out,  MEDIATYPE type)
{
	int ret = 0;
	AVFrame* frame = (AVFrame*)(p.srcData);
	ret = av_buffersrc_add_frame_flags(in, frame, 0);
	if (ret < 0) {
		return -1;
	}

	while (1) {
		AVFrame* oframe = av_frame_alloc();
		ret = av_buffersink_get_frame(out, oframe);
		if (ret < 0) {
			av_frame_free(&frame);
			av_frame_free(&oframe);
			if (AVERROR(EAGAIN) == ret) {
				break;
			}
			else if (AVERROR_EOF == ret) {
				break;
			}
			std::cout << "cant't get frame\n";
				return ret;
		}

		DataSet data;
		data.type = type;
		data.srcData = oframe;
		if (link) {
			if (link->fillBuffer(data) < 0) {
				av_frame_free(&oframe);
			}
		}
		else {
			//			std::cout << "no linkSD,free data\n";
			av_frame_free(&oframe);
		}
	}

	av_frame_free(&frame);
	return 0;
}
