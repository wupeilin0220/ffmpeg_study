#include "DataStructure.h"
#include <mutex>
#include <iostream>
#include "logoFilter.h"
#include "DrawText.h"
#include "TimeMark.h"
#include "FFmpegEncoder.h"
#include "Mosaic.h"
#include <vector>


#pragma once
class EffectSet{
public:
	std::string remove_id;
	int level;
	Linker* ef;
};

class FilterMgr:public Linker
{
public:
	FilterMgr(MuxContext& input,Linker* end);
	virtual ~FilterMgr();

	virtual int fillBuffer(DataSet data) override;
	int effectChange(TaskApply& task);
	
	//set frame pts offset from last frame
	void update();
	void resetTimeoffset(){Update_VP = false; Update_AP = false; videoOffset = 0; audioOffset = 0;}
private:
	int addLogoEffect(TaskApply& task);
	int addTextEffect(TaskApply& task);
	int addMosaicEffect(TaskApply& task);
	int addTimeMarkEffect(TaskApply& task);
	int removeEffect(TaskApply& task);
	void addToEffect(char *id,Linker* next);
	void addToEffect(EffectSet& filter);

//	int64_t PMAX(int64_t a, int64_t b) { return (a > b) ? a : b; }
protected:
	std::vector<std::pair<std::string, Linker*>> effectChain;
	std::list<EffectSet> Chain;
	Linker*  chainHead;
	const Linker* chainEnd;
	const MuxContext Context;
	bool first;
private:
	int64_t baseVideoPts;
	int64_t	baseAudioPts;
	int64_t videoOffset;
	int64_t audioOffset;
	double baseTime;

	std::atomic<bool> Update_VP,Update_AP;
};

