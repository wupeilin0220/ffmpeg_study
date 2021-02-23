#pragma once
#include "filter.h"
#include "MediaBase.h"
class Mosaic:public Linker
{
public:
	Mosaic(const MuxContext& attr);
	virtual ~Mosaic();
	virtual int fillBuffer(DataSet p) override;
	int initTask(TaskApply& task_);
protected:
	int dataStream(DataSet& p);

	TaskApply task;
private:
	const MuxContext info;
	int times;	
	int randnum;
};

