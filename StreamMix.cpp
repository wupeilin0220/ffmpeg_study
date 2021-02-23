#include "StreamMix.h"

StreamMix::StreamMix(const MuxContext& attr):
	audioMixer(nullptr),
	videoMixer(nullptr),
	muxCtx(attr)
{
	
}

StreamMix::~StreamMix()
{
	stopMix();
}

int StreamMix::paramsCheck(TaskApply& task){
	
	if (task.export_stream.videoMix.empty()) 
		return -1;
	
	if(task.export_stream.videoMix.size() == 1){
		auto iter = task.export_stream.videoMix.begin();
		if((*iter).id == 0)
			return -1;

		std::cout << " video_width rect.x  video_height rect.y "<< task.mediaCtx.video_width<< (*iter).rect.x<< task.mediaCtx.video_height<< (*iter).rect.y<<std::endl;
		if((task.mediaCtx.video_width == (*iter).rect.x && task.mediaCtx.video_height == (*iter).rect.y) || (*iter).rect.x <= 0 || (*iter).rect.y <= 0)
			return -1;
		
	}
	
	// 输入的多个windowID都为0
	bool allBlack = false;
	for(auto val : task.export_stream.videoMix){
		allBlack |= val.id;
	}
	if(!allBlack)
		return -1;

	return 0;
}

int StreamMix::initMixFilter(TaskApply& task)
{
	
	int ret = 0;
	// init video/audio mixer
//	std::cout << "Stream time "<< av_gettime(); 
	// 去除vmix中无需mix情况
	if (paramsCheck(task) != -1) {
		videoMixer = new VMix(muxCtx);
		if (videoMixer->init_filter(task) < 0) {
			std::cout << "init VMix fail \n";
			delete videoMixer;
			videoMixer = nullptr;
		}else{
			std::cout << " StreamMix::initMixFilter Video\n";
			videoMixer->linkTo(this);
			ret++;
		}
	}


	if (task.export_stream.audioMix.size() > 0) {
		audioMixer = new AMix(muxCtx);
		if (audioMixer->init_filter(task) < 0) {
			std::cout << "init VMix fail \n";
			delete audioMixer;
			audioMixer = nullptr;
		}else{
			std::cout << " StreamMix::initMixFilter Audio\n";
			audioMixer->linkTo(this);
			ret++;
		}
	}
	
	return ret;
}

void StreamMix::startMix()
{
	if (audioMixer) {
		audioMixer->startMix();
	}

	if (videoMixer) {
		videoMixer->startMix();
	}
}

void StreamMix::stopMix()
{
	if (audioMixer) {
		audioMixer->stopMix();
		delete audioMixer;
		audioMixer = nullptr;
	}

	if (videoMixer) {
		videoMixer->stopMix();
		delete videoMixer;
		videoMixer = nullptr;
	}
}

int StreamMix::BroadData(DataSet& p, int pin)
{
	if (p.type == MEDIATYPE::VIDEO) {
		if (videoMixer)
			return videoMixer->BroadData(p, pin);
	}
	else {
		if (audioMixer)
			return audioMixer->BroadData(p, pin);
	}

	if(link)
		return link->fillBuffer(p);
	else
		return -1;

//	return 0;
}

int StreamMix::fillBuffer(DataSet p)
{
	mutex_link.lock();
	
	if (link && (link->fillBuffer(p) > 0)){
		mutex_link.unlock();
		return -1;
	}
		
	mutex_link.unlock();
	return 0;
}
