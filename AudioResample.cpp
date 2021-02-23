#include "AudioResample.h"

AudioResample::AudioResample(const MuxContext& attr):
	filterBase(attr), audio_fifoBuffer(nullptr), resample_inTime(0),
	task_resample()
{

}

AudioResample::~AudioResample()
{
	if (audio_fifoBuffer) {
		av_audio_fifo_free(audio_fifoBuffer);
		audio_fifoBuffer = nullptr;
	}
	std::cout << "AudioResample disruct_________________\n";
}

int AudioResample::fillBuffer(DataSet p)
{
	if (link) {
		if (p.type == MEDIATYPE::AUDIO) {
			return pushAudio(p);
		}
		else {
			return link->fillBuffer(p);
		}
	}else
		return -1;
}

int AudioResample::init_effectfilter(TaskApply& task_)
{
	if (!mediaInfo.hasAudio) {
		std::cout << "AudioResample: use not contain audio info source_________\n";
		return -1;
	}
	char layout[64] = {};
	av_get_channel_layout_string(layout, 64, task_.mediaCtx.audio_channel, task_.mediaCtx.audio_channel_layout);
	char args[512] = {};
	snprintf(args, 512, "[ain0]aformat=sample_fmts=%s:channel_layouts=%s:sample_rates=%d,volume=volume=%lf[aout0]",
		av_get_sample_fmt_name((AVSampleFormat)task_.mediaCtx.audio_format), layout, task_.mediaCtx.sample,task.volume);
	std::cout << "AudioResample args "<< args << "input source time base" << mediaInfo.audio_timebase.den <<std::endl;
	AVFilterInOut* in = avfilter_inout_alloc();
	AVFilterInOut* out = avfilter_inout_alloc();
	
	in->filter_ctx = abuffer_Ctx;
	in->name = av_strdup("ain0");
	in->next = NULL;
	in->pad_idx = 0;

	out->filter_ctx = abufferSink_Ctx;
	out->name = av_strdup("aout0");
	out->next = NULL;
	out->pad_idx = 0;

	int ret = avfilter_graph_parse_ptr(audio_graph, args, &out, &in, NULL);
	if (ret < 0) {
		goto end;
	}
	ret = avfilter_graph_config(audio_graph, NULL);
	if (ret < 0) {
		goto end;
	}

	av_buffersink_set_frame_size(abufferSink_Ctx, 1024);


	task_resample = task_;
end:
	avfilter_inout_free(&in);
	avfilter_inout_free(&out);
	return ret;
}

int AudioResample::pushAudio(DataSet& p)
{
	AVFrame* frame = (AVFrame*)p.srcData;
	if (frame) {
		resample_inTime = av_rescale_q(frame->pts + frame->pkt_duration, mediaInfo.audio_timebase, AVRational{ 1,AV_TIME_BASE });
	}
	else {
		int ret = av_audio_fifo_size(audio_fifoBuffer);
		resample_inTime += av_rescale_q(ret, mediaInfo.audio_timebase, AVRational{ 1,AV_TIME_BASE });
	}


	int ret = av_buffersrc_add_frame_flags(abuffer_Ctx, frame, 0);
	if (ret < 0) {
		av_frame_free(&frame);
		return ret;
	}

	while (1) {
		AVFrame* a_frame = av_frame_alloc();
		ret = av_buffersink_get_frame(abufferSink_Ctx, a_frame);
		if (ret < 0)
		{
			av_frame_free(&frame);
			av_frame_free(&a_frame);
			if (AVERROR(EAGAIN) == ret) {
				break;
			}
			else if (AVERROR_EOF == ret) {
				break;
			}
			else {
				return ret;
			}
		}

		
		if(task.mediaCtx.sample == mediaInfo.sample && task.mediaCtx.audio_channel == mediaInfo.audio_channel){
					a_frame->pts = av_rescale_q(a_frame->pts, mediaInfo.audio_timebase, AVRational{ 1,task.mediaCtx.sample });
		}

		DataSet data;
		data.type = MEDIATYPE::AUDIO;
		data.srcData = a_frame;

		if (link) {
			if (link->fillBuffer(data) < 0) {
				av_frame_free(&a_frame);
			}
		}else
			av_frame_free(&a_frame);

	}


	av_frame_free(&frame);
	return 0;
}

int AudioResample::resampleAudio(AVAudioFifo* fifo, AVFrame* , AVRational dst_time, int64_t src_pts)
{
	static const int AAC_SAMPLE_SIZE = 1024;

	while (av_audio_fifo_size(fifo) >= AAC_SAMPLE_SIZE) {
		AVFrame* aac = av_frame_alloc();

		aac->channels = task_resample.mediaCtx.audio_channel;
		aac->nb_samples = AAC_SAMPLE_SIZE;
		aac->format = task_resample.mediaCtx.audio_format;
		int ret = av_frame_get_buffer(aac,0);

		if(ret < 0 && av_frame_get_buffer(aac,0) < 0){
			av_frame_free(&aac);
			std::cout << "AudioResample get_buffer err "<<ret << std::endl;
			return -1;
		}

		ret = av_audio_fifo_read(fifo, (void**)aac->data, aac->nb_samples);
		if (ret < AAC_SAMPLE_SIZE) {
			av_frame_free(&aac);
			return 0;
		}

		int64_t rset = av_rescale_q(av_audio_fifo_size(fifo), AVRational{ 1, task.mediaCtx.sample }, AVRational{ 1,AV_TIME_BASE });
		aac->pts = av_rescale_q(src_pts - rset , AVRational{ 1,AV_TIME_BASE }, dst_time) ;
		DataSet data;
		data.type = MEDIATYPE::AUDIO;
		data.srcData = aac;

		if (link) {
			link->fillBuffer(data);
		}
	}
	return 0;
}
