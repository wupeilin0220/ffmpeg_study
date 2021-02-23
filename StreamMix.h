#pragma once
#include "AMix.h"
#include "VMix.h"

// 需要将所有的输入转换成统一的格式输入
class StreamMix:public eightPinLink,public Linker
{
public:
	StreamMix(const MuxContext& attr);
	virtual ~StreamMix();
	int initMixFilter(TaskApply& task);
	void startMix();
	void stopMix();
protected:
	int paramsCheck(TaskApply& task);
	virtual int BroadData(DataSet& p, int pin) override;
private:
	virtual int fillBuffer(DataSet p) override;
private:
	AMix* audioMixer;
	VMix* videoMixer;
	MuxContext muxCtx;
};

