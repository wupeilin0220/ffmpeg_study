#pragma once
#include "filter.h"
class split:public filterBase,public Link{
public:
	split(const MuxContext& attr);
	virtual ~split();
	virtual int fillBuffer(DataSet p) override; 
	inline void linkToHD(Link *link,bool pushAudio,bool pushVideo){
		mutex_hd.lock();
		linkHD = link;
		sendAudio = pushAudio;
		sendVideo = pushVideo;
		mutex_hd.unlock();
	}
	inline void linkToSD(Link *link){linkSD = link;}
	const MuxContext& getSDMediaInfo() { return outMedia; }
	virtual Link* getNext() override { return nullptr; };
	Link* getSDLink() { return linkSD; }
	Link* getHDLink() { return linkHD; }
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	int init_videoEffect(TaskApply&);
	int init_audioEffect(TaskApply&);
	int pushStream(DataSet& p,AVFilterContext *in,AVFilterContext* out_pre,AVFilterContext* out_from,MEDIATYPE type,bool push);
private:
	Link*	linkHD;
	Link* 	linkSD;
	AVFilterContext*	vbufferSink_CtxSplit;
	AVFilterContext* 	abufferSink_CtxSplit;
	bool	sendAudio;
	bool	sendVideo;
	MuxContext	outMedia;
	std::mutex			mutex_hd;
};
