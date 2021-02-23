#pragma once
#include "filter.h"
class logoFilter:public filterBase,public Linker,private Serial
{
public:
	logoFilter(const MuxContext& attr);
	virtual ~logoFilter();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	virtual int init_audioInOutput(TaskApply& ) override { return 0; }
	
	int pushVideo(DataSet& p);
	virtual void serialize() final;
protected:
	AVFilterContext*			vbuffer_movie;
	bool first;		

	static int logoNum;
}; 

