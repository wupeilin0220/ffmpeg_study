#pragma once
#include "filter.h"
#include "MediaBase.h"

class TimeMark: public filterBase,public Linker{
public:
	TimeMark(const MuxContext& attr);
	virtual ~TimeMark();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	virtual int init_audioInOutput(TaskApply& ) override{return 0;}
	int pushVideo(DataSet& p);
	char * convertFontfileName(const std::string&  unicode,char *utf);
private:
	AVFilterContext* textCtx;
};
