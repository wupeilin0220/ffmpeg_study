#include "TaskBroadcast.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <pcrecpp.h>
#define gettid() syscall(__NR_gettid)

int TaskBroadcast::ReportStreamStatus(DecoderNet* obj, int status) {

	for (int i(0); i < MAX_INPUTSTREAM; i++) {
		if (obj == dec[i]) {
			streamStatus[i] = (status == 2);
			feedback(i,status);
			
			if(encPush){
				bool vnormal = isStreamNormal(vmixID);
				encPush->streamBroken(!vnormal,MEDIATYPE::VIDEO);	

				bool anormal = isStreamNormal(amixID);
				encPush->streamBroken(!anormal,MEDIATYPE::AUDIO);	

				if( !vnormal || !anormal){
					manager->resetTimeoffset();
					std::cout << "StreamStatus:: FilterManager need update "<< i <<std::endl;
				}
			
			}
		}
	}

	return 0;
}

TaskBroadcast::TaskBroadcast():
	encPush(nullptr), outputTask(),
	videoIndex(0), audioIndex(0),
	manager(nullptr),listen(nullptr),
	intervalTime(0),mixer(nullptr)
{
	for (int i(0); i < MAX_INPUTSTREAM; i++) {
		dec[i] = nullptr;
		streamRes[i] = nullptr;
		streamStatus[i] = false;
	}
	std::function<int(DecoderNet*, int)> fun = std::bind(&TaskBroadcast::ReportStreamStatus, this, std::placeholders::_1, std::placeholders::_2);
	listen = new NetStreamListen();
	listen->ListenNetStream(fun);
}

TaskBroadcast::~TaskBroadcast(){
	serialize();
	if(listen){
		listen->StopListen();
	}

	TaskApply task;
	ResponseInfo res;
	EraseAll(task,res);

	if(listen){
		delete listen;
		listen = nullptr;
	}

	std::cout << "TaskBroadcast::~TaskBroadcast()\n"; 	
}


int TaskBroadcast::TaskParse(JsonReqInfo& task, ResponseInfo& response)
{
	int ret = Success;
	response.retCode = Success;
	for (auto val : task) {
		switch (val.getType()) {
			case ApplyType::ADDSTREAM:
				ret = AddStream(val, response);
				break;
			case ApplyType::REMOVESTREAM:
				ret = RemoveStream(val, response);
				break;
			case ApplyType::CREATE:
				ret = Create(val, response);
				break;
			case ApplyType::LOGO:			
			case ApplyType::TEXT:
			case ApplyType::REMOVEFFECT:
			case ApplyType::MOSAICO:
			case ApplyType::TIMEMARK:
				ret = effectChange(val, response);
				break;
			case ApplyType::UPDATA:
				ret = UpdateStream(val, response);
				break;
			case ApplyType::STOP:
				ret = StopPushStream(val, response);
				break;
			case ApplyType::CLOSE:
				ret = EraseAll(val, response);
				break;
			case ApplyType::ADDSPACER:
				ret = AddSpaceStream(val, response);
				break;
			case ApplyType::REMOVESPACER:
				ret = RemoveDownloadingSpacer(val,response);
				break;
			case ApplyType::STOPSPACER:
				ret = StopSpaceStream(val, response);
				break;
			case ApplyType::USESPACER:
				ret = UseSpaceStream(val, response);
				break;
			case ApplyType::ADDSOURCE:
				ret = AddSource(val,response);
				break;
			default:
				std::cout << "WARNING: unknow task type" << std::endl;
				//	response.retCode = ERR_JSONPARAM;
				return Success;
		}
	}
	std::cout << "response task over\n";
	return ret;
}

int TaskBroadcast::AddStream(TaskApply& task, ResponseInfo& response)
{
	int ret = 0;
	auto taskInfo = task.stream;
	int index = taskInfo.windowID;
	response.windowID = index;
	std::lock_guard<std::mutex> guid(mutex_addStream);

	// 如果当前路有文件流正在下载，停止并删除
	if(streamRes[index] && !streamRes[index]->m_bready){ 
		std::string url = ResourceCreater::FindSourceUrl(streamRes[index]->m_res_path);
		std::cout << "TaskBroadcast::AddStream window "<< index << " stop downloading and remove\n"; 
		ResourceCreater::RemoveResource(url.c_str(),streamRes[index]);
		streamRes[index] = nullptr;
	}	

	if (dec[index] != nullptr) {

		if(vmixID.find(index) == vmixID.end()  && amixID.find(index) == amixID.end()){
			TaskApply task_tmp(ApplyType::REMOVESTREAM);
			task_tmp.remove_stream.windowID = index;
			ResponseInfo response_tmp;
			RemoveStream(task_tmp,response_tmp);
			if(response_tmp.retCode != Success){
				std::cout << "err can't remove Stream\n";
				response.retCode = response_tmp.retCode;
				return response_tmp.retCode;
			}
		}else{
			response.retCode = ERR_REQUEST;
			return ERR_REQUEST;
		}
	}

	if (taskInfo.urlStream.size() <= 0)
		return ERR_REQUEST;

	if (taskInfo.type == StreamType::PicStream) {
		std::vector<std::string> picAlbum;
		for(auto url: taskInfo.urlStream){
			ResourceCreater* res = ResourceCreater::create_res(url.c_str());
			if(!res)
			{
				response.url.push_back(url);
				continue;
			}
			if(res->SyncDownload() == -1)
			{
//				delete res;
				response.url.push_back(url);	
			}else{
				picAlbum.push_back(res->m_res_path);
			}

		}

		PictureStream* picStream = new PictureStream();
		auto failLoad = picStream->addPicture(picAlbum);
		if (failLoad.size() >= picAlbum.size()) {
			response.retCode = ERR_Decode;
			delete picStream;
			return ERR_Decode;
		}
		picStream->setImgInterval(task.stream.interValTime);

		// picstream not use split and resample audio


		// encode
		TaskApply encPre(ApplyType::CREATE);
		
		encPre.mediaCtx = picStream->getMediaInfo();
		encPre.export_stream.delay= 0;
		FFmpegEncoder* enc = new FFmpegEncoder(encPre.mediaCtx);
		const Config *conf = Config::getConf(CONFPATH);
		if(conf->GetRtmpServer().empty()){
			response.retCode = ERR_Encode_Init;
			return response.retCode;
		}
		int pos = index%(conf->GetRtmpServer().size());
		std::string host = conf->GetRtmpServer()[pos];

		std::string preUrl("rtmp://");
		preUrl += host + "/";
		preUrl += conf->GetSection();

		preUrl += std::to_string(index);
		preUrl += "/Stream-" + std::to_string(gettid()) + std::to_string(av_gettime()%100000);
		std::cout << preUrl << std::endl;
		response.url.push_back(preUrl);
		if ((ret = enc->init_encoder(preUrl, encPre)) < 0) {
			delete picStream;
			delete enc;
			if (ret == -1) {
				response.retCode = ERR_PushPublic;
			}
			else {
				response.retCode = ERR_Encode_Init;
			}
			return response.retCode;
		}
		
		picStream->linkTo(enc);

		enc->startEncode();
		picStream->decoderStream();

		dec[taskInfo.windowID] = picStream;
		response.retCode = Success;
		response.hasAudio = false;
		response.hasVideo = true;
		return Success;

	}
	else if (taskInfo.type == StreamType::FilStream) {	
		//判断是否为网络地址
		if(isNetStreamUrl(taskInfo.urlStream.front())){
			response.retCode = ERR_Decode;
			response.responseType = 2; 
			response.status = 1;
			return ERR_PARAM;
		}

		//如果m3u8地址为点播文件，直接拉流推送
		pcrecpp::RE oPattern1("^https*://.+m3u8$"),oPattern2("^http*://.+m3u8$");
		if (oPattern1.PartialMatch(taskInfo.urlStream.front().c_str()) || oPattern2.PartialMatch(taskInfo.urlStream.front().c_str())){
			DecoderFile* fileStream = new DecoderFile;
			ret = fileStream->init_decoder(taskInfo.urlStream.front());
			if (ret < 0) {
				response.responseType = 1; 
				response.status = 0;
				delete fileStream;
				response.retCode = ERR_Decode;
				return ERR_Decode;
			}		
			dec[index] = fileStream;	
			response.responseType = 2; 
			response.status = 1;
			response.retCode = Success;	
		}else{
			ResourceCreater* res = ResourceCreater::create_res(taskInfo.urlStream.front().c_str());
			if(!res)
			{
				response.retCode = ERR_Download_File;
				return ERR_Download_File;
			}

			auto overFun = std::bind(overCB,task,std::placeholders::_1);
			auto percentFun = std::bind(progressCB,task,std::placeholders::_1,std::placeholders::_2);
			int ret = res->AsyncDownload(overFun,percentFun);
			if(ret == -1)
			{
				//文件地址无法访问
				response.retCode = Success;
				response.responseType = 1; 
				response.status = 0;

				response.hasAudio = false;
				response.hasVideo = false;

				response.retCode = Success;
				ResourceCreater::RemoveResource(taskInfo.urlStream.front().c_str(),res);
				return ERR_Download_File;
			}else if(ret == 0){
				// file is start download 	
				streamRes[index] = res;
				std::cout << "AddStream:: file url check ok\n";
				response.retCode = Success;
				response.responseType = 1; 
				response.status = 1;

				response.hasAudio = false;
				response.hasVideo = false;
				spacerRes = res;
				return Success;

			}else{
				std::cout << "download file\n";
				std::string path = res->m_res_path;

				std::cout << "get file path "<< path<<std::endl;
				DecoderFile* fileStream = new DecoderFile;
				ret = fileStream->init_decoder(path);
				if (ret < 0) {
					response.responseType = 1; 
					response.status = 0;
					delete fileStream;
					response.retCode = ERR_Decode;
					return ERR_Decode;
				}		
				dec[index] = fileStream;	
				response.responseType = 2; 
				response.status = 1;
				response.retCode = Success;
			}
		}
	}
	else if (taskInfo.type == StreamType::NetStream) {
		std::cout << __LINE__<<std::endl;
		if(false == isNetStreamUrl(taskInfo.urlStream.front())){
			std::cout << "check url err____________\n";
			response.retCode = ERR_JSONPARAM;
			return ERR_JSONPARAM;
		}
		DecoderNet* netStream = new DecoderNet;
		ret = netStream->init_decoder(taskInfo.urlStream.front());
		if (ret < 0) {
			delete netStream;
			response.retCode = ERR_Decode;
			return ERR_Decode;
		}
		dec[index] = netStream;
		listen->Add(netStream);
		StreamBreak fun = std::bind(&NetStreamListen::StreamIsBroken,listen,std::placeholders::_1,netStream);
		netStream->setStreamBroken(fun);
	}
	else {
		response.retCode = ERR_JSONPARAM;
		return -1;
	}

	split* sp = new split(dec[index]->getMediaInfo());
	TaskApply sp_task;
	if (sp->init_filter(sp_task) < 0) {
		delete dec[taskInfo.windowID];
		dec[taskInfo.windowID] = nullptr;
		delete sp;
		response.retCode = ERR_Filter_Split;
		return ERR_Filter_Split;
	}
	
	
	const Config *conf = Config::getConf(CONFPATH);
	if(taskInfo.type == StreamType::FilStream && !conf->IsPreviewFile()){
		dec[index]->linkTo(sp);
		dec[index]->decoderStream();
		response.responseType = 2; 
		response.status = 1;
		response.retCode = Success;
		response.hasVideo = dec[index]->getMediaInfo().hasVideo;
		response.hasAudio = dec[index]->getMediaInfo().hasAudio;
		return Success;
	}

	AudioResample* resample = nullptr;
	if (dec[taskInfo.windowID]->getMediaInfo().hasAudio) {
		resample = new AudioResample(dec[taskInfo.windowID]->getMediaInfo());
		TaskApply task_resample(ApplyType::VOLUME);

		task_resample.mediaCtx = dec[taskInfo.windowID]->getMediaInfo();
		task_resample.mediaCtx.audio_channel = 2;
		task_resample.mediaCtx.audio_channel_layout = AV_CH_LAYOUT_STEREO;
		task_resample.mediaCtx.sample = 16000;
		task_resample.mediaCtx.audio_format = AV_SAMPLE_FMT_FLTP;
		task_resample.mediaCtx.audio_bits = 32;//44*2
		task_resample.volume = 2.0;
		ret = resample->init_filter(task_resample);
		if (ret < 0) {
			delete resample;
			resample = nullptr;
			std::cout << "AddStream: create AudioResample fail\n";
		}
	}

	TaskApply encPre(ApplyType::CREATE);

	encPre.mediaCtx = sp->getSDMediaInfo();
	encPre.mediaCtx.video_bits = Config::getConf(CONFPATH)->GetVideoBits();
	encPre.mediaCtx.audio_bits = Config::getConf(CONFPATH)->GetAudioBits();
	encPre.mediaCtx.audio_channel = 2;
	encPre.mediaCtx.audio_channel_layout = AV_CH_LAYOUT_STEREO;
	encPre.mediaCtx.sample = 16000;
	encPre.mediaCtx.audio_format = AV_SAMPLE_FMT_FLTP;
	encPre.mediaCtx.audio_timebase = AVRational{ 1,encPre.mediaCtx.sample };
	encPre.mediaCtx.hasAudio = dec[taskInfo.windowID]->getMediaInfo().hasAudio;
	encPre.mediaCtx.hasVideo = dec[taskInfo.windowID]->getMediaInfo().hasVideo;
	encPre.export_stream.delay = 0;


	FFmpegEncoder* enc = new FFmpegEncoder(encPre.mediaCtx);
	int pos = index%(conf->GetRtmpServer().size());
	std::string host = conf->GetRtmpServer()[pos];
	std::string preUrl("rtmp://");
	std::string httpUrl(conf->GetFlvVisitProtocol());
	httpUrl += "://";

	preUrl += host + ":";
	preUrl += std::to_string(conf->GetRtmpPort());
	preUrl += "/"+  conf->GetSection();	


	httpUrl += host + ":";
	httpUrl += std::to_string(conf->GetHttpPort());
	httpUrl += "/"+ conf->GetSection();
	
	preUrl += std::to_string(index);
	httpUrl += std::to_string(index);

	
	std::string streamName = "/Stream-" + std::to_string(gettid()) + std::to_string(av_gettime()%100000);
	preUrl += streamName;
	httpUrl += streamName + ".flv";
	std::cout << preUrl << std::endl;
	std::cout << httpUrl << std::endl;
	response.url.push_back(preUrl);
	response.url.push_back(httpUrl);

//	preUrl = "rtmp://119.3.245.104:1935/Preview7/Stream1";
	if (enc->init_encoder(preUrl, encPre) < 0) {
		if(dec[taskInfo.windowID]->type == StreamType::NetStream){
			listen->Delete( dynamic_cast<DecoderNet*> (dec[taskInfo.windowID]));
		}else{
			delete dec[taskInfo.windowID];
		}
		dec[taskInfo.windowID] = nullptr;
		delete sp;
		delete resample;
		delete enc;
		response.retCode = ERR_Encode_Init;
		return ERR_Encode_Init;
	}
	

	dec[taskInfo.windowID]->linkTo(sp);

	if (resample) {
		sp->linkToSD(resample);
		resample->linkTo(enc);
	}
	else {
		sp->linkToSD(enc);
	}

	dec[taskInfo.windowID]->decoderStream();
	enc->startEncode();
	response.responseType = 2; 
	response.status = 1;
	response.retCode = Success;
	response.hasVideo = dec[taskInfo.windowID]->getMediaInfo().hasVideo;
	response.hasAudio = dec[taskInfo.windowID]->getMediaInfo().hasAudio;
	return 0;
}

int TaskBroadcast::RemoveStream(TaskApply& task, ResponseInfo& response)
{
	std::cout << "Remove Stream..........................\n";
	response.retCode = Success;
	std::string outputUrl;	
	int index = task.remove_stream.windowID;
	if (index > 0 && index < MAX_INPUTSTREAM) {
		if (amixID.find(index) != amixID.end() || vmixID.find(index) != vmixID.end()) {
			response.retCode = ERR_REQUEST;
			return ERR_REQUEST;
		}
		
		
		if (dec[index] != nullptr) {
			dec[index]->stopStream();	
			if (dec[index]->type != StreamType::PicStream) {
				std::cout << " remove net/file stream\n";
				// file or net  stream
				split* sp = dynamic_cast<split*>(dec[index]->getNext());
				std::vector<Link*> linkSDChain;
				Link* sd = sp->getSDLink();
				while (sd) {
					linkSDChain.push_back(sd);
					if(sd->getNext() == nullptr){
						outputUrl = dynamic_cast<FFmpegEncoder*>(sd)->GetOutputUrl();
					}
					sd = sd->getNext();
				}


				if (dec[index]->type == StreamType::NetStream) {
					dec[index]->linkTo(nullptr);
//					std::dynamic_cast<DecoderNet*>(dec[index])	
					listen->Delete(dynamic_cast<DecoderNet*>(dec[index]));
					dec[index] = nullptr;
				}
				else {
					delete dec[index];
					dec[index] = nullptr;
				}

				for (auto val : linkSDChain) {
					delete val;
				}

				delete sp;

			}
			else {
				// for pic stream   pic->enc  // preview
				dec[index]->stopStream();
				Link* enc = dec[index]->getNext();
				try{
					outputUrl = dynamic_cast<FFmpegEncoder*>(enc)->GetOutputUrl();
					delete dec[index];
					delete enc;
				}catch(std::bad_cast){
					std::cout << "err cast link to Encode\n";		
				}

				dec[index] = nullptr;

			}
			response.retCode = Success;

		}
		else {
			// 如果当前路的文件流正在下载，停止并删除
//			std::cout << "streamRes[index] "<< streamRes[index] << " streamRes[index]->m_bready "<< streamRes[index]->m_bready<<std::endl;
			if(streamRes[index] && (!streamRes[index]->m_bready)){ 
				std::string url = ResourceCreater::FindSourceUrl(streamRes[index]->m_res_path);
				std::cout << "window "<< index <<" url "<< url <<"stop downloading and remove\n"; 
				ResourceCreater::RemoveResource(url.c_str(),streamRes[index]);
				streamRes[index] = nullptr;
			}	
			response.retCode = Success;
		}
		
		response.url.push_back(outputUrl);
		response.windowID = index;
		streamStatus[index] = false;

	}
	return 0;
}

int TaskBroadcast::Create(TaskApply& task, ResponseInfo& response)
{
	int ret = 0;
	std::set<int> tmpVideoId;
	std::set<int> tmpAudioId;

	std::set<Position> tmpVideoInput;
	std::set<AudioInput> tmpAudioInput;
/*********************前置条件检测********************************/
	if(task.export_stream.videoMix.size() > 0){

		for(auto& p:task.export_stream.videoMix){
			if(p.id > 0 && p.id < MAX_INPUTSTREAM){
				tmpVideoId.insert(p.id);
				tmpVideoInput.insert(p);
				std::cout << "videooInput "<< p.id<<std::endl;
			}
		}
	}

	if(task.export_stream.audioMix.size() > 0){

		for(auto& p: task.export_stream.audioMix){
			if(p.id > 0 && p.id < MAX_INPUTSTREAM){
				tmpAudioInput.insert(p);
				tmpAudioId.insert(p.id);
				std::cout << "audioInput "<< p.id<<std::endl;
			}
		}
	}

	//  no output
	if(tmpAudioId.empty() && tmpVideoId.empty()){
		response.retCode = ERR_PARAM;
		return ret;	
	}

	// condition check 
	for(auto val: tmpVideoId){
		if(dec[val] == nullptr || (!dec[val]->getMediaInfo().hasVideo)){
			response.retCode = ERR_PARAM;
			return ret;
		}	
	}

	for(auto val: tmpAudioId){
		if(dec[val] == nullptr || (!dec[val]->getMediaInfo().hasAudio)){
			response.retCode = ERR_PARAM;
			return ret;
		}	
	}

	response.retCode = Success;

/*********************create encoder***************************/
	if (encPush) {
		response.retCode = ERR_Encode_Exist;
		return ERR_Encode_Exist;
	}
	MuxContext media;
	media.video_timebase = AVRational{ 1,AV_TIME_BASE };
	media.audio_timebase = AVRational{ 1,44100 };
	media.hasVideo = true;
	media.hasAudio = true;


	encPush = new FFmpegEncoder(media);
	if(task.mediaCtx.audio_channel == 2)
		task.mediaCtx.audio_channel_layout = AV_CH_LAYOUT_STEREO;
	else 
		task.mediaCtx.audio_channel_layout = AV_CH_LAYOUT_MONO;

	task.mediaCtx.sample = 44100;
	task.mediaCtx.audio_bits = 44 * 2;
	task.mediaCtx.audio_format = AV_SAMPLE_FMT_FLTP;
	task.mediaCtx.video_format = AV_PIX_FMT_YUV420P;
	// set emergency stream
	if(emerType == StreamType::FilStream){
		if(!emergencyFile.empty()){
			ret = encPush->addEmergencyStream(emergencyFile);
			if(ret < 0){
				std::cout << "decode emergency file stream fail\n";	
			}
		}
	}else{
		if(emergencyPic.size() > 0){
			ret = encPush->addEmergencyStream(emergencyPic,intervalTime);	
			if(ret < 0){
				std::cout << "decode emergency picture stream fail\n";
			}
		}
	}

	auto taskCreate = task.export_stream;
	ret = encPush->init_encoder(taskCreate.pushUrl, task);
	if (ret < 0) {
		delete encPush;
		encPush = nullptr;
		if(ret == -1){
			response.retCode = ERR_PushPublic;
		}else
			response.retCode = ERR_Encode_Init;
		return response.retCode;
	}
	encPush->startEncode();
	encPush->setObserve(feedback);

/***********************init filterMgr&StreamMix******************************/
	
	task.mediaCtx.video_timebase = media.video_timebase;
	task.mediaCtx.audio_timebase = media.audio_timebase;
	task.mediaCtx.hasAudio = media.hasAudio;
	task.mediaCtx.hasVideo = media.hasVideo;


	if (nullptr == manager) {	
		manager = new FilterMgr(task.mediaCtx,encPush);
	}

	if(mixer !=nullptr)
		delete mixer;

	mixer = new StreamMix(task.mediaCtx);
	mixer->linkTo(manager);
	
	
/***********************init pre filter*************************/
	
	//	init filter for filter
	for (auto val : tmpAudioInput)
	{
		bool setVideo = false;
		int index = val.id;
		if (tmpVideoId.find(index) != tmpVideoId.end())
		{
			setVideo = true;
		}
		
		// video
		AspectRatio *ratio_new = nullptr;
		if (setVideo)
		{
			if (dec[index]->type != StreamType::PicStream)
			{
				ratio_new = new AspectRatio(dec[index]->getMediaInfo());

				TaskApply task_ratio(ApplyType::ADAPT);
				task_ratio.adstyle = AdaptStyle::Fill;
				task_ratio.mediaCtx = task.mediaCtx;
				ret = ratio_new->init_filter(task_ratio);
				if (ret < 0)
				{
					delete ratio_new;
					ratio_new = nullptr;
					std::cout << "UpdateStream: Warning create new AspectRatio err____________\n";
					response.retCode = ERR_Filter_Vol;
					return ERR_Filter_Adapt;
				}
				std::cout << "create AspectRatio filte_______________r\n";
			}
		}

		//audio
		AudioResample *resample_new = nullptr;
		TaskApply task_vol = task;
		

		if (val.volume >= 0.0 && val.volume <= 2.0)
		{
			task_vol.volume = val.volume;
		}
		else
		{
			task_vol.volume = 1.0;
		}

		resample_new = new AudioResample(dec[index]->getMediaInfo());
		ret = resample_new->init_filter(task_vol);
		if (ret < 0)
		{
			delete resample_new;
			resample_new = nullptr;
			std::cout << "UpdateStream: Warning volume filter init err_____________________\n";
			response.retCode = ERR_Filter_Vol;
			return ERR_Filter_Vol;
		}
		std::cout << "create AudioResample___________\n";

		Linker *last = resample_new;
		if (ratio_new)
		{
			resample_new->linkTo(ratio_new);
			last = ratio_new;
		}
		last->linkTo(mixer->GetPin(val.id - 1));
		split *sp = dynamic_cast<split *>(dec[val.id]->getNext()); // must be file/Net stream
		sp->linkToHD(resample_new, true, setVideo);
	
		if(val.seekPos >= 0){
			dec[index]->seekPos(val.seekPos);
		}
		
/*		if (task.export_stream.seekPos  >= 0)
		{
			dec[index]->seekPos(task.export_stream.seekPos);
		}
		*/
	}

	//需要单独输出video的源
	for (auto val : tmpVideoInput)
	{
			
		int index = val.id;
		if (tmpAudioId.find(index) != tmpAudioId.end())
		{
			continue;
		}

		// video
		AspectRatio *ratio_new = nullptr;
		if (dec[index]->type != StreamType::PicStream)
		{
			ratio_new = new AspectRatio(dec[index]->getMediaInfo());

			TaskApply task_ratio(ApplyType::ADAPT);
			
			task_ratio.adstyle = AdaptStyle::Fill;
			task_ratio.mediaCtx = task.mediaCtx;

			ret = ratio_new->init_filter(task_ratio);
			if (ret < 0)
			{
				delete ratio_new;
				ratio_new = nullptr;
				std::cout << "UpdateStream: Warning create new AspectRatio err____________\n";
				response.retCode = ERR_Filter_Vol;
				return ERR_Filter_Adapt;
			}
			std::cout << "create AspectRatio filte_______________r\n";
		}

		if (ratio_new == nullptr)
		{ // pic stream
			PictureStream *pic = dynamic_cast<PictureStream *>(dec[index]);
			pic->setHDProperty(outputTask.mediaCtx.video_width, outputTask.mediaCtx.video_height);
			pic->linkToHD(mixer->GetPin(index - 1));
			pic->pushHDstream();
		}
		else
		{															// normal stream
			split *sp = dynamic_cast<split *>(dec[index]->getNext()); // must be file/Net stream
			sp->linkToHD(ratio_new, false, true);
			ratio_new->linkTo(mixer->GetPin(index - 1));
		}

		if(val.seekPos >= 0){
			dec[index]->seekPos(val.seekPos);
		}
	/*	if (task.export_stream.seekPos >= 0)
		{
			dec[val]->seekPos(task.export_stream.seekPos);
		}*/
	}


	if(mixer->initMixFilter(task) >  0){
		mixer->startMix();
	}
	outputTask = task;
	encPush->setSendStreamtype((tmpVideoId.size() >0),(tmpAudioId.size()>0));
	amixID = tmpAudioId;
	vmixID = tmpVideoId;

	videoIndex = *vmixID.begin();
	audioIndex = *amixID.begin();
	return 0;
}

int TaskBroadcast::effectChange(TaskApply& task, ResponseInfo& response)
{
	if(task.getType() == ApplyType::LOGO){
		char* filePath = task.logo.name;
		ResourceCreater* res = ResourceCreater::create_res(filePath);
		if(!res)
		{
			return ERR_Download_File;
		}

		if(	res->SyncDownload() == -1)
		{
			//	delete res;
			return ERR_Download_File;
		}
		std::cout << "download file\n";
		memset(filePath,0,URLSIZE);
		strcpy(filePath,res->m_res_path);
		std::cout << "logo file name "<< filePath;

//		std::cout << "get file path "<< filePath <<std::endl;
	}

	if (manager) {
		std::cout << "effectChange " <<std::endl;
		response.retCode = manager->effectChange(task);
	}
	else
		response.retCode = ERR_REQUEST;

	return response.retCode;
}

int TaskBroadcast::UpdateStream(TaskApply& task, ResponseInfo& response)
{
	int ret = 0;
	std::set<int> tmpVideoId;
	std::set<int> tmpAudioId;

	std::set<Position>	tmpVideoInput;
	std::set<AudioInput> tmpAudioInput;

	if(!encPush){
		response.retCode = ERR_REQUEST;
		return ERR_REQUEST;		
	}
		
	//parse task
	if(task.export_stream.videoMix.size() > 0 ){	
		for(auto &p: task.export_stream.videoMix){
			if(p.id > 0 && p.id < MAX_INPUTSTREAM){
				tmpVideoId.insert(p.id);
				tmpVideoInput.insert(p);
			}			
		}
	}
/*	else{  // single video out
		if(task.export_stream.videoWindowID > 0 && task.export_stream.videoWindowID < MAX_INPUTSTREAM){
			tmpVideoId.insert(task.export_stream.videoWindowID);
		}
	}
*/
	if(task.export_stream.audioMix.size() > 0){
		
		for(auto&p: task.export_stream.audioMix){
			if(p.id > 0 && p.id < MAX_INPUTSTREAM){
				tmpAudioInput.insert(p);
				tmpAudioId.insert(p.id);
			}		
		}
	}
/*	else{  // single audio out
		if(task.export_stream.audioWindowID > 0 && task.export_stream.audioWindowID < MAX_INPUTSTREAM){
			AudioInput in;
			in.id = task.export_stream.audioWindowID;
			if (task.export_stream.volume >= 0.0 && task.export_stream.volume <= 2.0)
				in.volume = task.export_stream.volume;
			else{
				in.volume = 1.0;
				std::cout << "Updata vol "<< task.export_stream.volume <<"UpdateStream: Warning volume out of range,ues 1.0\n";
			}
			tmpAudioInput.insert(in);
			tmpAudioId.insert(in.id);
		}
	}
*/
	//  no output id
	if(tmpAudioId.empty() && tmpVideoId.empty()){
		response.retCode = ERR_PARAM;
		return ret;	
	}
	
	// params check 
	for(auto val: tmpVideoId){
		if(dec[val] == nullptr || (!dec[val]->getMediaInfo().hasVideo)){
			response.retCode = ERR_PARAM;
			return ret;
		}		
	}

	for(auto val: tmpAudioInput){
		if(dec[val.id] == nullptr || (!dec[val.id]->getMediaInfo().hasAudio)){
			response.retCode = ERR_PARAM;
			return ret;
		}	
	}

	response.retCode = Success;

	//remove last input	
	// counts input
	std::set<int> removeID = vmixID;
	for(auto val:amixID){
		if(vmixID.find(val) == vmixID.end()){
			removeID.insert(val);
		}
	}
	

	//remove filters
	mixer->stopMix();
	for(auto val:removeID){
		Link* link = nullptr;
		if(dec[val]->type != StreamType::PicStream){
			split * sp = dynamic_cast<split*>(dec[val]->getNext());
			link = sp->getNext();				
			sp->linkToHD(nullptr,false,false);
		}else{
			PictureStream *pic = dynamic_cast<PictureStream*>(dec[val]);
			link = pic->getHDLink();
			pic->stopPushHDStream();
			pic->linkToHD(nullptr);	
		}	
		removeMiddel(link,mixer);
	}
	manager->update();
	task.mediaCtx = outputTask.mediaCtx;
	if(mixer->initMixFilter(task) > 0){
		std::cout << "start mix stream \n";
		mixer->startMix();
	}

//	init filter for filter
	for(auto val: tmpAudioInput){
		bool setVideo = false;
		int index = val.id;
		if(tmpVideoId.find(index) != tmpVideoId.end()){	
			setVideo = true;
		}
			
		// video
		AspectRatio* ratio_new = nullptr;
		if(setVideo){
			if(dec[index]->type != StreamType::PicStream){
				ratio_new = new AspectRatio(dec[index]->getMediaInfo());

				TaskApply task_ratio = outputTask;
				task_ratio.setType(ApplyType::ADAPT);
				task_ratio.adstyle = AdaptStyle::Fill;

				ret = ratio_new->init_filter(task_ratio);
				if (ret < 0) {
					delete ratio_new;
					ratio_new = nullptr;
					std::cout << "UpdateStream: Warning create new AspectRatio err____________\n";
					response.retCode = ERR_Filter_Vol;
					return ERR_Filter_Adapt;
				}
				std::cout << "create AspectRatio filte_______________r\n";
			}
		}

		//audio
		AudioResample* resample_new = nullptr;
		TaskApply task_vol(ApplyType::VOLUME);
		task_vol.mediaCtx = outputTask.mediaCtx;

		if (val.volume >= 0.0 && val.volume <= 2.0) {
			task_vol.volume = val.volume;
		}
		else {
			task_vol.volume = 1.0;
		}

		resample_new = new AudioResample(dec[index]->getMediaInfo());
		ret = resample_new->init_filter(task_vol);
		if (ret < 0) {
			delete resample_new;
			resample_new = nullptr;
			std::cout << "UpdateStream: Warning volume filter init err_____________________\n";
			response.retCode = ERR_Filter_Vol;
			return ERR_Filter_Vol;
		}
		std::cout << "create AudioResample___________\n";
		
		Linker* last = resample_new;
		if(ratio_new){
			resample_new->linkTo(ratio_new);
			last = ratio_new;
		}
		last->linkTo(mixer->GetPin(index-1));
		split* sp =  dynamic_cast<split*>(dec[val.id]->getNext()); // must be file/Net stream 
		sp->linkToHD(resample_new,true,setVideo);
		if(val.seekPos >= 0){
			dec[index]->seekPos(val.seekPos);
		}
/*		if(task.export_stream.seekPos >= 0){
			dec[index]->seekPos(task.export_stream.seekPos);
		}
*/
	}

	//需要单独输出video的源
	for(auto val:tmpVideoInput){
		int index = val.id;
		if(tmpAudioId.find(index) != tmpAudioId.end()){
			continue;
		}
				
		// video
		AspectRatio* ratio_new = nullptr;
		if(dec[index]->type != StreamType::PicStream){
			ratio_new = new AspectRatio(dec[index]->getMediaInfo());

			TaskApply task_ratio(ApplyType::ADAPT);
			task_ratio.mediaCtx = outputTask.mediaCtx;
			task_ratio.adstyle = AdaptStyle::Fill;

			ret = ratio_new->init_filter(task_ratio);
			if (ret < 0) {
				delete ratio_new;
				ratio_new = nullptr;
				std::cout << "UpdateStream: Warning create new AspectRatio err____________\n";
				response.retCode = ERR_Filter_Vol;
				return ERR_Filter_Adapt;
			}
			std::cout << "create AspectRatio filte_______________r\n";
		}

		
		if(ratio_new == nullptr){ // pic stream 
			PictureStream *pic = dynamic_cast<PictureStream *>(dec[index]);
			pic->setHDProperty(outputTask.mediaCtx.video_width,outputTask.mediaCtx.video_height);
			pic->linkToHD(mixer->GetPin(index-1));
			pic->pushHDstream();

		}else{ // normal stream
			split* sp = dynamic_cast<split*>(dec[index]->getNext()); // must be file/Net stream 
			sp->linkToHD(ratio_new,false,true);
			ratio_new->linkTo(mixer->GetPin(index-1));
		}
		if(val.seekPos >= 0){
			dec[index]->seekPos(val.seekPos);
		}
/*		if(task.export_stream.seekPos >= 0){
			dec[val]->seekPos(task.export_stream.seekPos);
		}
		*/
	}
	

	int tmp_aIndex = -1,tmp_vIndex = -1;
	if(tmpVideoId.size() == 1){
		tmp_vIndex = *(tmpVideoId.begin());
	}

	if(tmpAudioId.size() == 1){
		tmp_aIndex = *(tmpAudioId.begin());
	}

	// 多路
	if(isStreamNormal(tmp_aIndex)){	
		encPush->streamBroken(false,MEDIATYPE::AUDIO);
	}	
	if(isStreamNormal(tmp_vIndex)){	
		encPush->streamBroken(false,MEDIATYPE::VIDEO);		
	}	

	//	ts->inputChange(videoIndex != tmp_vIndex,audioIndex != tmp_aIndex);
	audioIndex = tmp_aIndex;
	videoIndex = tmp_vIndex;
	encPush->setSendStreamtype((tmpVideoId.size() >0),(tmpAudioId.size() >0));
	vmixID = tmpVideoId;
	amixID = tmpAudioId;
	return 0;
}

int TaskBroadcast::StopPushStream(TaskApply& , ResponseInfo& response)
{
	std::cout << "StopPushStream...........\n";

	for(auto wid: vmixID){
		Link* begin = nullptr;
		if (dec[wid]->type == StreamType::PicStream) {

			PictureStream* picStream = nullptr;			
			try {
				picStream = dynamic_cast<PictureStream*>(dec[wid]);
				begin = picStream->getHDLink();
				picStream->stopPushHDStream();
				picStream->linkToHD(nullptr);

			}
			catch (std::bad_cast) {

			}
		}
		else {
			split* sp_v = dynamic_cast<split*>(dec[wid]->getNext());
			begin = sp_v->getNext();
			sp_v->linkToHD(nullptr, false, false);
		}
			
		removeMiddel(begin, mixer);
					
	}

	for(auto wid: amixID){
		Link* begin = nullptr;
		if(vmixID.find(wid) != vmixID.end()){
			continue;
		}

		split* sp_a = dynamic_cast<split*>(dec[wid]->getNext());
		begin = sp_a->getNext();
		sp_a->linkToHD(nullptr,false,false);

		removeMiddel(begin, mixer);
	}

	delete mixer;
	delete manager;
	delete encPush;
	mixer = nullptr;
	manager = nullptr;
	encPush = nullptr;
	
	videoIndex = 0;
	audioIndex = 0;
	amixID.clear();
	vmixID.clear();
	response.retCode = Success;

	return 0;
}

int TaskBroadcast::EraseAll(TaskApply& task, ResponseInfo& response)
{
	StopPushStream(task, response);
	for (int i(0); i < MAX_INPUTSTREAM; i++) {
		if (dec[i] != nullptr) {
			TaskApply task_remove(ApplyType::REMOVESTREAM);
			
			task_remove.remove_stream.windowID = i;
			RemoveStream(task_remove, response);
		}
	}
	ResourceCreater::ClearBuff();	
	emergencyFile.clear();
	response.retCode = Success;
	return 0;
}


int TaskBroadcast::removeMiddel(Link* begin, Link* end)
{
	Link* next = begin;
	while (next) {
		Link* tmp = next->getNext();
		if (next != end) {
			delete next;
		}
		else
			break;
		next = tmp;
	}
	return 0;
}

int TaskBroadcast::AddSource(TaskApply& task, ResponseInfo& response){

	for(auto& url: task.source){
		ResourceCreater* res = ResourceCreater::create_res(url.c_str());
		if(!res)
		{
			response.url.push_back(url);
			continue;
		}
		if(res->SyncDownload() == -1)
		{
	//		delete res;
			response.url.push_back(url);	
		}else{
			response.succUrl.push_back(url);
		}	
	}
	response.retCode = Success;
	return 0;
}


static int Interrupt_CB(void* p) {
	int64_t start = *(int64_t*)p;
	int64_t now = av_gettime();
	if ((now - start) > NetTimeout) {
		return 1;
	}
	return 0;
}

bool TaskBroadcast::isNetStreamUrl(const std::string& url){

//	return true;
	std::cout << "check url "<< url<<std::endl;
	pcrecpp::RE oPattern1("^rtmp://"),oPattern2("^rtsp://"),oPattern3("^https*://.+m3u8$"), oPattern5("^https*://"),oPattern6("^ftp://"),oPattern7("^(?:/[^/]*)*/*$"),oPattern8("^http*://.+m3u8$");
	if (oPattern1.PartialMatch(url) || oPattern2.PartialMatch(url))
	{
		return true;
	}else if(oPattern3.PartialMatch(url) || oPattern8.PartialMatch(url)){
		// check m3u8  http-flv is not vod
		
		int ret = 0;
		AVFormatContext* formatContext = avformat_alloc_context();
		if(!formatContext && !(formatContext = avformat_alloc_context())){
			std::cout << "alloc format context fail\n";
			avformat_free_context(formatContext);
			return false;
		}

		int64_t start_time = av_gettime();
		formatContext->interrupt_callback.callback = Interrupt_CB;
		formatContext->interrupt_callback.opaque = &start_time;

		if ((ret = avformat_open_input(&formatContext, url.c_str(), NULL, NULL)) < 0) {
			std::cout << "url check avformat_open_input err " << AVERROR(ret) << std::endl;		
			return false;
		}

		if ((ret = avformat_find_stream_info(formatContext, NULL)) < 0) {
			std::cout << "url check avformat_find_stream_info err " << ret << std::endl;
			return false;
		}

		std::cout << "url  duration "<< formatContext->duration<<std::endl;
		bool isNetStream =  ( AV_NOPTS_VALUE == formatContext->duration) || (formatContext->duration == 0);
		avformat_free_context(formatContext);
		formatContext = NULL;
		return  isNetStream;
	}


	return false;
}

int TaskBroadcast::AddSpaceStream(TaskApply& task, ResponseInfo& response){
	if (task.stream.urlStream.size() <= 0) {
		response.retCode = ERR_JSONPARAM;
		return response.retCode;
	}
	std::lock_guard<std::mutex> guid(mutex_spacer);

	if(task.stream.type != StreamType::FilStream){
	
		std::vector<std::string> picAlbum;
		for(auto& url: task.stream.urlStream){
			ResourceCreater* res = ResourceCreater::create_res(url.c_str());
			if(!res)
			{
				response.url.push_back(url);
				continue;
			}
			if(res->SyncDownload() == -1)
			{
		//		delete res;
				response.url.push_back(url);	
			}else{
				picAlbum.push_back(res->m_res_path);
			}
		}

		response.retCode = Success;
		PictureStream picStream ;
		auto failLoad = picStream.addPicture(picAlbum);
		if (failLoad.size() >= picAlbum.size()) {
			response.retCode = ERR_Decode;
			response.retCode =  ERR_Decode;
		}

		response.hasVideo = true;
		response.hasAudio = false;
		if(!encPush || encPush->addEmergencyStream(picAlbum, task.stream.interValTime) < 0){
				response.retCode = ERR_REQUEST;
				return response.retCode;
		}

		emergencyPic = picAlbum;
		emerType = StreamType::PicStream;
		intervalTime = task.stream.interValTime;
		return response.retCode;
	}else{
		auto space = task.stream;
		response.url.push_back(space.urlStream.front());
		ResourceCreater* res = ResourceCreater::create_res(space.urlStream.front().c_str());
		if(!res)
		{
			response.retCode = ERR_Download_File;
			return ERR_Download_File;
		}

		auto overFun = std::bind(overCB,task,std::placeholders::_1);
		auto percentFun = std::bind(progressCB,task,std::placeholders::_1,std::placeholders::_2);
		int ret = res->AsyncDownload(overFun,percentFun);
		if(ret == -1)
		{
	//		url is error
			response.retCode = Success;
			response.responseType = 1; 
			response.status = 0;

			response.hasAudio = false;
			response.hasVideo = false;
		}else if(ret == 0){

			// file is start download 	
			std::cout << "AddSpaceStream:: file url check ok\n";
			response.retCode = Success;
			response.responseType = 1; 
			response.status = 1;

			response.hasAudio = false;
			response.hasVideo = false;
			spacerRes = res;
		}else{

			// download over  callback
			std::cout << "download file over callback________\n";

			response.responseType = 2; 
			response.status = 1;
			response.retCode = Success;
			std::string path = res->m_res_path;
			DecoderFile fileStream;
			int ret = fileStream.init_decoder(path);

			if(ret < 0){
				response.retCode = ERR_Decode;	
				return response.retCode;
			}
			response.hasAudio = fileStream.getMediaInfo().hasAudio;
			response.hasVideo = fileStream.getMediaInfo().hasVideo;

			if(encPush){
				if(encPush->addEmergencyStream(path) < 0){
					response.retCode = ERR_REQUEST;
					return response.retCode;
				}
			}

			emerType = StreamType::FilStream;
			emergencyFile = path;
		}
	}
	return response.retCode;
}

int TaskBroadcast::RemoveDownloadingSpacer(TaskApply& task, ResponseInfo& response){	
	if(spacerRes){
		ResourceCreater::RemoveResource(task.remove_id,spacerRes);		
		spacerRes = nullptr;
		emergencyFile.clear();
		if(encPush){
			encPush->addEmergencyStream(emergencyFile);
		}
	}	
	response.retCode = Success;
	return response.retCode;
}

int TaskBroadcast::StopSpaceStream(TaskApply&, ResponseInfo& response){
	if(encPush){
		encPush->swicthNormalStream();
		response.retCode = Success;
	}else{
		response.retCode = ERR_REQUEST;
	}
	return response.retCode;
}

int TaskBroadcast::UseSpaceStream(TaskApply& task, ResponseInfo& response){
	response.retCode = Success;
	std::string path = task.source.front();
	if(encPush){
		
		ResourceCreater *res = ResourceCreater::create_res(path.c_str());
		if(!res || res->AsyncDownload(nullptr,nullptr) !=1){
			response.retCode = ERR_NOSPACER;
			return response.retCode;	
		}

		if(encPush->addEmergencyStream(res->m_res_path) < 0){
			response.retCode = ERR_REQUEST;
			return response.retCode;
		}


//		emerType = StreamType::FilStream;
//		emergencyFile = path;

		int ret = encPush->switchEmergencyStream();	
		if(ret < 0){
			response.retCode = ERR_NOSPACER;
		}
	}else{
		response.retCode = ERR_REQUEST;
	}
	return response.retCode;
}

bool TaskBroadcast::isStreamNormal(std::set<int>& idSet){
	bool normal = true;
	if(idSet.empty())
		normal = false;

	for(auto val: idSet){
		normal &= streamStatus[val];
	}
	return !normal;
}

bool TaskBroadcast::isStreamNormal(int id){
	if(id > 0 && id < MAX_INPUTSTREAM)
		return !streamStatus[id];
	return false;
}

void TaskBroadcast::serialize(){
	if(fp){
		mutex_syncFile.lock();
		for (int i(0); i < MAX_INPUTSTREAM; i++) {
			char preview[32]={};
			snprintf(preview,32,"\n[Preview_%d]",i);
			fwrite(preview,1,strlen(preview),fp);
			if (dec[i] != nullptr) {
				char info[1024]={};
				if(dec[i]->type == StreamType::FilStream ){
					std::string str = ResourceCreater::FindSourceUrl(dec[i]->getInputUrl());
					snprintf(info,1024,"\ninputUrl=%s\ntype=1",str.c_str());
				}else if(dec[i]->type == StreamType::NetStream){
					snprintf(info,1024,"\ninputUrl=%s\ntype=0",dec[i]->getInputUrl().c_str());	
				}

				fwrite(info,1,strlen(info),fp);
			}
		}

		fwrite("\n[output]",1,sizeof("\n[output]")-1,fp);
		char Out[128]={};
		snprintf(Out,128,"\nvideoWindowID=%d\naudioWindowID=%d\nvolume=%f",videoIndex,audioIndex,outputTask.export_stream.volume);
		fwrite(Out,1,strlen(Out),fp);
		
		mutex_syncFile.unlock();
	}
}
