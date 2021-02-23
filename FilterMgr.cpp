#include "FilterMgr.h"
#include <algorithm>
#include <iterator>

using namespace std;
FilterMgr::FilterMgr(MuxContext& input, Linker* end):
	effectChain(), chainHead(end), chainEnd(end),
	Context(input),first(true),
	baseVideoPts(-1),baseAudioPts(-1),videoOffset(0),audioOffset(0),
	baseTime(0.0),
	Update_VP(false),Update_AP(false)
{
//	chainHead = const_cast<Linker*>(end);
}

FilterMgr::~FilterMgr()
{
	for (auto val : Chain) {
		delete val.ef;
	}

	std::cout << "FilterMgr::~FilterMgr distruct__________\n";
}

int FilterMgr::fillBuffer(DataSet data)
{

	if(first){
		first = false;
		std::cout << "get first time "<<av_gettime()<<std::endl; 
	}

	
	if (data.type == MEDIATYPE::VIDEO) {
		if(Update_VP){
			std::cout << "base time pts "<< baseTime/av_q2d(Context.video_timebase) << " new pts "<< ((AVFrame*)(data.srcData))->pts;
			videoOffset = ((AVFrame*)(data.srcData))->pts - baseTime/av_q2d(Context.video_timebase);
			Update_VP = false;
			std::cout << "FilterMgr video offset "<< videoOffset <<std::endl;
		}

		((AVFrame*)(data.srcData))->pts += videoOffset;
		baseVideoPts = ((AVFrame*)(data.srcData))->pts;
	}
	else {
		if(Update_AP){
			std::cout << "base time pts "<< baseTime/av_q2d(Context.audio_timebase) << " new pts "<< ((AVFrame*)(data.srcData))->pts;
			audioOffset = ((AVFrame*)(data.srcData))->pts - baseTime/av_q2d(Context.audio_timebase);
			std::cout << "FilterMgr audio offset "<< audioOffset<<std::endl;
			Update_AP =	false;
		}

		((AVFrame*)(data.srcData))->pts += audioOffset;
		baseAudioPts = ((AVFrame*)(data.srcData))->pts;
	}
	
	mutex_link.lock();
	chainHead->fillBuffer(data);
	mutex_link.unlock();
	return 0;
}

int FilterMgr::effectChange(TaskApply& task)
{
	int ret = Success;
	switch (task.getType()) {
	case ApplyType::LOGO:
		ret = addLogoEffect(task);
		break;

	case ApplyType::TEXT:
		ret = addTextEffect(task);
		break;

	case ApplyType::REMOVEFFECT:
		ret = removeEffect(task);
		break;

	case ApplyType::MOSAICO:
		ret = addMosaicEffect(task);
		break;
	case ApplyType::TIMEMARK:
		ret = addTimeMarkEffect(task);
		break;

	default:
		return ERR_PARAM;
	}
	return ret;
}

int FilterMgr::addLogoEffect(TaskApply& task)
{
	logoFilter* logo = new logoFilter(Context);
	int ret = logo->init_filter(task);
	if (ret < 0) {
		std::cout << "err init logo filter \n";
		delete logo;
		return ERR_Filter_logo;
	}
		


	EffectSet efSet;
	efSet.ef = logo;
	efSet.remove_id = task.logo.id;
	efSet.level = task.logo.level;
	
	addToEffect(efSet); 

	std::cout << "add Logo Effect\n";
	return Success;
}

int FilterMgr::addTextEffect(TaskApply& task)
{
	std::cout << "add Text effect\n";
	DrawText* draw = new DrawText(Context);
	int ret = draw->init_filter(task);
	if (ret < 0) {
		delete draw;
		std::cout << "init draw Text filter err\n";
		return ERR_Filter_Text;
	}

	
	EffectSet efSet;
	efSet.ef = draw;
	efSet.remove_id = task.text.id;
	efSet.level = task.text.level;

	addToEffect(efSet);
	return Success;
}

int FilterMgr::addMosaicEffect(TaskApply& task){
	Mosaic *mos = new Mosaic(Context);
	if(mos->initTask(task) < 0){
		return ERR_PARAM;
	}		

	EffectSet efSet;
	efSet.ef = mos;
	efSet.remove_id = task.mosaic.id;
	efSet.level = task.mosaic.level;
	
	addToEffect(efSet); 

	std::cout << "add Mosaic Effect\n";
	return Success;

}

int FilterMgr::addTimeMarkEffect(TaskApply& task){
	std::cout << "add TimeMark efect\n";

	TimeMark *markT = new TimeMark(Context);
	if(markT->init_filter(task)){
		return ERR_PARAM;
	}

	EffectSet efSet;
	efSet.ef = markT;
	efSet.remove_id = task.text.id;
	efSet.level = task.text.level;

	addToEffect(efSet);
	return Success;
}

int FilterMgr::removeEffect(TaskApply& task)
{
	mutex_link.lock();
	std::string target = task.remove_id;
	auto iter = std::find_if(Chain.begin(), Chain.end(), [&target](EffectSet& it) {
			return it.remove_id == target;});
	
	if(iter == Chain.end()){
		std::cout << "removeEffect:: can't find remove target \n";
	}else{
		auto pre = iter;
		if(iter != Chain.begin()){
			iter--;
			iter->ef->linkTo(pre->ef->getNext());
		}

		delete pre->ef;
		Chain.erase(pre);
		
		if(!Chain.empty()){
			chainHead = Chain.front().ef;
		}else{
			chainHead = const_cast<Linker*>(chainEnd);
		}
	}
	

	mutex_link.unlock();
	
	
	return Success;
}

void FilterMgr::addToEffect(char* id, Linker* next)
{
	mutex_link.lock();
	
	if (effectChain.size() > 0) {
		effectChain.back().second->linkTo(next);
	}

	effectChain.push_back(std::make_pair(id, next));
	next->linkTo(const_cast<Linker*>(chainEnd));
	
	chainHead = effectChain.front().second;

	mutex_link.unlock();
}

void FilterMgr::addToEffect(EffectSet& filter){
	mutex_link.lock();
	if(Chain.empty()){
		Chain.push_back(filter);
	}else{
		auto inIt = Chain.end();
		for(auto it(Chain.begin());it != Chain.end();it++){
			if(it->level > filter.level){
				inIt = it;
				filter.ef->linkTo(inIt->ef); // link next 
				break;
			}
		}
		
		auto pos = Chain.insert(inIt,filter);
		if(pos != Chain.begin()){
			pos--;
			pos->ef->linkTo(filter.ef);  // link preview
		}
	}	
	
	Chain.back().ef->linkTo(const_cast<Linker*>(chainEnd));
	chainHead = Chain.front().ef;
	mutex_link.unlock();
}

// 输入的流的时间戳并还不是完全同步，每次换源时都会调用这个函数进行偏移同步
// 由于画中画功能暂时屏蔽

void FilterMgr::update(){
	return ;
/*	double tv =0.0, ta = 0.0;
	if(Context.hasVideo){
		Update_VP = true;
		tv = baseVideoPts * av_q2d(Context.video_timebase);
	}

	if(Context.hasAudio){
		Update_AP = true;
		ta = baseAudioPts * av_q2d(Context.audio_timebase);
	}

	baseTime = (ta > tv)?ta:tv;
	std::cout << "FilterMgr:: video time "<< tv<< " audio time "<< ta << std::endl; 
	*/
}
