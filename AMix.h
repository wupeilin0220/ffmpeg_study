#pragma once
#include "filter.h"
#include "MediaBase.h"
#include <algorithm>
#include <map>
#include <array>
#include <set>
#include "Serial.h"
/*
	音频混合滤镜，要求输入数据格式相同（音频格式，采样率，时间基，声道）
*/
class AMix:public filterBase,public Linker,private Serial
{
public:
	AMix(const MuxContext& attr);
	virtual ~AMix();
	void stopMix();
	void startMix();
	virtual int fillBuffer(DataSet) override { return -1; }
	int BroadData(DataSet& p, int pin);  // revice inputs data 
protected:
	
	virtual int init_audioInOutput(TaskApply& ) override { return 0; }
	virtual int init_videoInOutput(TaskApply& )override { return 0; }
	virtual int init_effectfilter(TaskApply& task_) override;

private:
	typedef std::pair<int, AVFilterContext*> AINCTX;
	int pushAudio();
	int64_t syncTime(volatile bool& run);
	int64_t syncTime(int64_t syncT,int index);
	AVFrame* makeSilenceFrame(TaskApply& task);
	bool streamInterruptDetect(AINCTX& ctx);

private:
	volatile bool run;
//	AVFrame* frame_list[8]; // max input 
	AVFrame* muteFrame;

	std::list<std::pair<int, AVFilterContext*>> ain_Ctx;
	std::thread* mixHandle;
	std::list<AVFrame*> frame_buffer[8];
	std::mutex	mutex_frame[8];
	std::array<int64_t, 8> lastFramePts;
	std::array<int64_t, 8> muteFramePts;
	std::set<int> audioWindowID;
	int64_t stepPts;
protected:
	void serialize() final;
};

