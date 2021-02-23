#pragma once
#include "filter.h"
//使用此类实现输出流的格式化
class AspectRatio:public filterBase,public Linker
{
public:
	AspectRatio(const MuxContext& attr);
	virtual ~AspectRatio();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_audioInOutput(TaskApply&) override { return 0; }
	virtual int init_effectfilter(TaskApply& task_) override;
	void		Adapt(const TaskApply& task_, int& dst_w, int& dst_h);

	int pushVideo(DataSet& p);
	bool needAdapt;
};

