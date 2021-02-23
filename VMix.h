#pragma once
#include "filter.h"
#include "MediaBase.h"
#include "Serial.h"
#include <algorithm>
#include <map>
#include <array>
#include <set>
/*
	视频叠加，要求输入为同一种格式（时间基，视频尺寸）
*/
class VMix :public filterBase,public Linker,private Serial
{
public:
	VMix(const MuxContext& attr);
	virtual ~VMix();
	void	stopMix();
	void	startMix();
	int BroadData(DataSet& p, int pin) ;  // revice inputs data 
	virtual int fillBuffer(DataSet) override { return -1; }
protected:
	
	virtual int init_audioInOutput(TaskApply& ) override { return 0; }
	virtual int init_videoInOutput(TaskApply& )override { return 0; }
	virtual int init_effectfilter(TaskApply& task_) override;
private:
	typedef std::pair<int, AVFilterContext*> VINCTX;
	int pushVideo();
	int ff_BuffInput(int index, AVFilterInOut* chain);
	int64_t syncTime(volatile bool& run);
	int64_t syncTime(int64_t syncT,int index);
	bool streamInterruptDetect(VINCTX& ctx);
private:
	volatile bool run;
	
	AVFrame* blackFrame;

	std::list<VINCTX> vin_Ctx;
	std::thread* mixHandle;
	std::list<AVFrame*> frame_buffer[9];
	std::mutex	mutex_frame[9];
	std::array<int64_t, 9> lastFramePts;
	std::array<int64_t, 9> blackFramePts;
	std::set<int> videoWindowID;
	int64_t	step;
protected:
	virtual void serialize() final;
};

