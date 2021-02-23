#pragma once
#include "filter.h"
//ʹ�ô���ʵ��������ĸ�ʽ��
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

