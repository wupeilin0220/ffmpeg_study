#pragma once
#include "filter.h"
extern "C" {
#include "libavutil/audio_fifo.h"
};
class AudioResample:public filterBase,public Linker
{
public:
	AudioResample(const MuxContext& attr);
	virtual ~AudioResample();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	virtual int  init_videoInOutput(TaskApply&) override { return 0; }
	int pushAudio(DataSet& p);
	int resampleAudio(AVAudioFifo* fifo, AVFrame* frame, AVRational dst_time, int64_t src_pts);
private:
	AVAudioFifo*		audio_fifoBuffer;
	int64_t				resample_inTime;
	TaskApply			task_resample;
};

