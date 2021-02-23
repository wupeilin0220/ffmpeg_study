#pragma once
#include "filter.h"
#include "MediaBase.h"
class FadeFilter :public filterBase,public Linker
{
public:
	FadeFilter(const MuxContext& attr);
	virtual ~FadeFilter();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	int initVideoFade(TaskApply& task_);
	int initAudioFade(TaskApply& task_);
	int pushVideo(DataSet& p);
	int pushStream(DataSet& p, AVFilterContext* in, AVFilterContext* out, MEDIATYPE type);
private:
	AVFilterContext* vfade;
	AVFilterContext* afade;
};

