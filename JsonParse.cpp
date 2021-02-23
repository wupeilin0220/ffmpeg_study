#include "JsonParse.h"
#include "string.h"
#include <iostream>
#include <array>
std::string JsonParse::guid = GenUUID();
JsonParse::JsonParse()
{

	RequestMap.insert(std::make_pair("addStream", Request_AddStream));
	RequestMap.insert(std::make_pair("removeStream", Request_RemoveStream));
	RequestMap.insert(std::make_pair("addSource", Request_AddSource));
	RequestMap.insert(std::make_pair("create", Request_Create));
	RequestMap.insert(std::make_pair("update", Request_Update));
	RequestMap.insert(std::make_pair("stop", Request_Stop));
	RequestMap.insert(std::make_pair("close", Request_Close));
	RequestMap.insert(std::make_pair("addSpacer", Request_AddSpacer));
	RequestMap.insert(std::make_pair("removeSpacer", Request_RemoveSpacer));
	RequestMap.insert(std::make_pair("useSpacer", Request_UseSpacer));
	RequestMap.insert(std::make_pair("stopSpacer", Request_StopSpacer));


	ReponseMap.insert(std::make_pair("addStream", Reponse_AddStream));
	ReponseMap.insert(std::make_pair("removeStream", Reponse_RemoveStream));
	ReponseMap.insert(std::make_pair("addSource", Reponse_AddSource));
	ReponseMap.insert(std::make_pair("create", Reponse_Create));
	ReponseMap.insert(std::make_pair("update", Reponse_Update));
	ReponseMap.insert(std::make_pair("stop", Reponse_Stop));
	ReponseMap.insert(std::make_pair("close", Reponse_Close));
	ReponseMap.insert(std::make_pair("addSpacer", Reponse_AddSpacer));
	ReponseMap.insert(std::make_pair("removeSpacer", Reponse_RemoveSpacer));
	ReponseMap.insert(std::make_pair("useSpacer", Reponse_UseSpacer));
	ReponseMap.insert(std::make_pair("stopSpacer", Reponse_StopSpacer));

}

JsonParse::~JsonParse()
{
	serialize();
}

int JsonParse::Request(const char* str, JsonReqInfo& task)
{
	rapidjson::StringStream json(str);
	rapidjson::Document doc;
	doc.ParseStream(json);

	if (doc.IsObject() == false)
		return ERR_JSONPARAM;  

	auto msg = doc.FindMember("messageType");
	if (msg == doc.MemberEnd())
		return ERR_JSONPARAM;
	std::string msgType = msg->value.GetString();

	auto process = RequestMap.find(msgType);//
	if (process == std::end(RequestMap))
		return ERR_TASKTYPE;
	// need copy data ??? 

	instanceID = doc.FindMember("instanceID")->value.GetString();
	userID = doc.FindMember("userID")->value.GetString();
	
	int parseRet = process->second(doc, task, this);
	if(task.size() > 0)
		task.front().reqMsg = str;
//	task.reqMsg = str;
	return parseRet;
}

std::string JsonParse::Response(const char* src, ResponseInfo& task)
{
//	std::cout << "respponse "<< src<<std::endl;
	rapidjson::StringStream json(src);
	rapidjson::Document doc;
	doc.ParseStream(json);

	if (doc.IsObject() == false)
		return std::string(); 

	auto msg = doc.FindMember("messageType");
	if (msg == doc.MemberEnd())
		return std::string();
	std::string msgType = msg->value.GetString();

	auto process = ReponseMap.find(msgType);//
	if (process == std::end(ReponseMap))
		return std::string();
	// need copy data ??? 

	doc.RemoveMember("data");
	doc.RemoveMember("pipe");
	return process->second(doc, task);
}

std::string JsonParse::ReportStreamStatus(ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	writer.StartObject();

	writer.Key("processID");
	writer.Int(-1);

	writer.Key("pipe");
	writer.String("");

	writer.Key("taskType");
	writer.String("");

	writer.Key("instanceID");
	writer.String(instanceID.c_str());

	writer.Key("messagetype");
	writer.String("streamStatus");

	writer.Key("requestID");
	writer.String(reqID.at(reponse.windowID).c_str());

	writer.Key("userID");
	writer.String(userID.c_str());

	writer.Key("data");
	writer.StartObject();

	writer.Key("windowID");
	writer.Int(reponse.windowID);


	writer.Key("status");
	writer.Int(reponse.status);

	writer.EndObject();
	writer.EndObject();

	writer.Flush();
	return std::string(resStr.GetString());
	//	}
}


std::string JsonParse::ReportDownloadProgress(const char *p, int windowID,int64_t total,int64_t cache){
//	std::cout << "call json \n"<< p<<std::endl;
	rapidjson::StringStream json(p);
	rapidjson::Document doc;
	doc.ParseStream(json);
	std::string reqID = "Unkonw requestID";
	std::string url = "";
	if (doc.IsObject() != false){
		auto reqidSec = doc.FindMember("requestID");
		if (reqidSec != doc.MemberEnd()) {
			reqID = reqidSec->value.GetString();	
		}
	}

	auto dataObj = doc.FindMember("data");
	if (dataObj != doc.MemberEnd()){
		auto fileUrl = dataObj->value.FindMember("inputUrl");
		if(fileUrl != dataObj->value.MemberEnd()){
			url = fileUrl->value.GetString();
		}
	}
	
	

	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	writer.StartObject();

	writer.Key("processID");
	writer.Int(-1);

	writer.Key("pipe");
	writer.String("");

	writer.Key("taskType");
	writer.String("");

	writer.Key("instanceID");
	writer.String(instanceID.c_str());

	writer.Key("messagetype");
	writer.String("downloadProgress");

	writer.Key("requestID");
	writer.String(reqID.c_str());

	writer.Key("userID");
	writer.String(userID.c_str());

	writer.Key("data");
	writer.StartObject();

	

	writer.Key("windowID");
	writer.Int(windowID);

	writer.Key("fileUrl");
	writer.String(url.c_str());

	writer.Key("total");
	writer.Int64(total);

	writer.Key("cache");
	writer.Int64(cache);

	writer.EndObject();
	writer.EndObject();

	writer.Flush();
	return std::string(resStr.GetString());
}


int JsonParse::Request_AddStream(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse* jsp)
{

	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;
	TaskApply apply(ApplyType::ADDSTREAM);

	apply.stream.windowID = dataObj->value.FindMember("windowID")->value.GetInt();


	auto streamType = dataObj->value.FindMember("type");
	if (streamType->value.GetInt() == 3) {
		apply.stream.type = StreamType::PicStream;
		apply.stream.interValTime = dataObj->value.FindMember("intervalTime")->value.GetInt64();

		auto arrSec = dataObj->value.FindMember("picUrls");
		if(arrSec == dataObj->value.MemberEnd())
			return ERR_JSONPARAM;

		auto arrUrl = arrSec->value.GetArray();
		for (rapidjson::SizeType i = 0; i < arrUrl.Size(); i++) {
			apply.stream.urlStream.push_back(arrUrl[i].GetString());
		}
	}
	else {
		if (streamType->value.GetInt() == 0) {
			apply.stream.type = StreamType::NetStream;
			
		}
		else if(streamType->value.GetInt() == 1 || streamType->value.GetInt() == 2) {
			apply.stream.type = StreamType::FilStream;

		}else 
			return ERR_JSONPARAM;

		auto val = dataObj->value.FindMember("inputUrl");
		if (val != dataObj->value.MemberEnd()) {
			apply.stream.urlStream.push_back(val->value.GetString());
		}
		else
			return ERR_JSONPARAM;
	}
	
	auto reqidSec = doc.FindMember("requestID");
	if (reqidSec != doc.MemberEnd()) {
		try {
			jsp->reqID.at(apply.stream.windowID).assign(reqidSec->value.GetString());
		}
		catch (std::out_of_range) {
			return ERR_JSONPARAM;
		}
	}

	JsonReq.push_back(apply);

	return 0;
}

int JsonParse::Request_RemoveStream(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse*)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;

	TaskApply apply(ApplyType::REMOVESTREAM);

	auto val = dataObj->value.FindMember("windowID");
	if (val != dataObj->value.MemberEnd())
		apply.remove_stream.windowID = val->value.GetInt();
	
	auto urlSec = dataObj->value.FindMember("previewUrl");
	if (urlSec != dataObj->value.MemberEnd() && urlSec->value.GetStringLength() > 0)
		strncpy(apply.remove_stream.url, urlSec->value.GetString(),sizeof(apply.remove_stream.url)-1);
	
	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_AddSource(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse*)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;

	TaskApply apply(ApplyType::ADDSOURCE);
	auto arr = dataObj->value.FindMember("fileUrl");
	if (arr->value.Empty()) {
		return ERR_JSONPARAM;
	}

	for (auto& val : arr->value.GetArray()) {
		apply.source.push_back(val.GetString());
	}
	
	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_Update(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse* jsp)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;

	
	auto mosaicSec = dataObj->value.FindMember("mosaic");
	if (mosaicSec != dataObj->value.MemberEnd())
	{
		auto removeSec = mosaicSec->value.FindMember("remove");
		if (removeSec == mosaicSec->value.MemberEnd() || jsp->parseRemoveObject(removeSec, JsonReq) < 0) {			
				return ERR_JSONPARAM;
		}

		auto mosaicAdd = mosaicSec->value.FindMember("add");
		if (mosaicAdd != mosaicSec->value.MemberEnd() && !mosaicAdd->value.Empty()){
			if( jsp->parseMosaicObject(mosaicAdd, JsonReq) < 0) {			
				return ERR_JSONPARAM;
			}
		}
	}

	auto logoSection = dataObj->value.FindMember("logo");
	if (logoSection != dataObj->value.MemberEnd()) {
		auto removeSec = logoSection->value.FindMember("remove");
		if (removeSec == logoSection->value.MemberEnd() || jsp->parseRemoveObject(removeSec, JsonReq) < 0) {	
				return ERR_JSONPARAM;			
		}

		auto logoAdd = logoSection->value.FindMember("add");
		if (logoAdd != logoSection->value.MemberEnd()&& !logoAdd->value.Empty()){
			if(jsp->parseLogoObject(logoAdd, JsonReq) < 0) {			
				return ERR_JSONPARAM;
			}
		}

	}

	// text
	auto textSection = dataObj->value.FindMember("text");
	if (textSection != dataObj->value.MemberEnd()) {
		auto removeSec = textSection->value.FindMember("remove");
		if (removeSec == textSection->value.MemberEnd() || jsp->parseRemoveObject(removeSec,JsonReq) < 0) {	
			return ERR_JSONPARAM;
		}


		auto textAdd = textSection->value.FindMember("add");
		if (textAdd == textSection->value.MemberEnd() || !textAdd->value.Empty()){
			if(jsp->parseTextObject(textAdd,JsonReq) < 0) {
				return ERR_JSONPARAM;
			}
		}
	}

	// time mark
	auto timeSection = dataObj->value.FindMember("timemark");
	if (timeSection != dataObj->value.MemberEnd()) {
		auto removeSec = timeSection->value.FindMember("remove");
		if (removeSec == timeSection->value.MemberEnd() || jsp->parseRemoveObject(removeSec,JsonReq) < 0) {	
			return ERR_JSONPARAM;
		}


		auto timeAdd = timeSection->value.FindMember("add");
		if (timeAdd == timeSection->value.MemberEnd() || !timeAdd->value.Empty()){
			if(jsp->parseTimeMarkObject(timeAdd,JsonReq) < 0) {
				return ERR_JSONPARAM;
			}
		}
	}

	// stream
	auto streamSection = dataObj->value.FindMember("stream");
	if (streamSection != dataObj->value.MemberEnd()) {
		TaskApply apply(ApplyType::UPDATA);
		JsonReq.push_back(apply);

		if (jsp->parseStreamObject(streamSection, JsonReq) < 0) {
			return ERR_JSONPARAM;
		}
	}

	return 0;
}

int JsonParse::Request_Stop(rapidjson::Document&, JsonReqInfo& JsonReq, JsonParse*)
{
	TaskApply apply(ApplyType::STOP);
	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_Close(rapidjson::Document&, JsonReqInfo& JsonReq, JsonParse* jsp)
{
	TaskApply apply(ApplyType::CLOSE);
	JsonReq.push_back(apply);

	for (int i(0); i < MAX_INPUTSTREAM; i++) {
		jsp->reqID[i].assign("");
	}
	return 0;
}

int JsonParse::Request_AddSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse*)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;
	TaskApply apply(ApplyType::ADDSPACER);
	
	auto streamType = dataObj->value.FindMember("type");
	if (streamType->value.GetInt() == 1) {
		apply.stream.type = StreamType::FilStream;	
		apply.stream.urlStream.push_back(dataObj->value.FindMember("inputUrl")->value.GetString());
		
	}
	else if (streamType->value.GetInt() == 2) {
		apply.stream.type = StreamType::PicStream;
		apply.stream.interValTime = dataObj->value.FindMember("intervalTime")->value.GetInt64();  // dai ding


		auto arrUrl = dataObj->value.FindMember("picUrls")->value.GetArray();
		for (rapidjson::SizeType i = 0; i < arrUrl.Size(); i++) {			
			apply.stream.urlStream.push_back(arrUrl[i].GetString());

		}
	}
	else
		return ERR_JSONPARAM;

	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_RemoveSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*){
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;	
	TaskApply apply(ApplyType::REMOVESPACER);

	auto streamType = dataObj->value.FindMember("fileUrl");
	if(streamType == dataObj->value.MemberEnd()){
		return ERR_JSONPARAM;		
	}
	apply.remove_id = streamType->value.GetString();
//	std::cout << "remove url "<< apply.remove_id<<std::endl;;
	JsonReq.push_back(apply);
	return 0;
}


int JsonParse::Request_UseSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse*)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;	
	TaskApply apply(ApplyType::USESPACER);

	auto streamUrl = dataObj->value.FindMember("fileUrl");
	if(streamUrl == dataObj->value.MemberEnd())
		return ERR_JSONPARAM;

	std::string url = streamUrl->value.GetString();
	apply.source.push_back(url);
	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_StopSpacer(rapidjson::Document&, JsonReqInfo& JsonReq, JsonParse*)
{
	TaskApply apply(ApplyType::STOPSPACER);
	JsonReq.push_back(apply);
	return 0;
}

int JsonParse::Request_Create(rapidjson::Document& doc, JsonReqInfo& JsonReq, JsonParse* jsp)
{
	auto dataObj = doc.FindMember("data");
	if (dataObj == doc.MemberEnd())
		return ERR_JSONPARAM;

	// daobo atribute
	
	TaskApply apply(ApplyType::CREATE);

	auto formatSection = dataObj->value.FindMember("outputFormat");
	if (formatSection != dataObj->value.MemberEnd() ) {
		
		strncpy(apply.export_stream.pushUrl, formatSection->value.FindMember("pushUrl")->value.GetArray()[0].GetString(), sizeof(apply.export_stream.pushUrl) - 1);
		
		apply.mediaCtx.video_width = formatSection->value.FindMember("width")->value.GetInt();
		apply.mediaCtx.video_height = formatSection->value.FindMember("height")->value.GetInt();
		apply.mediaCtx.fps = formatSection->value.FindMember("fps")->value.GetInt();
		apply.mediaCtx.video_bits = formatSection->value.FindMember("bitrate")->value.GetInt();
		apply.mediaCtx.audio_channel = formatSection->value.FindMember("channel")->value.GetInt();
		
		//ratio not use,it depand output width,height 
		apply.export_stream.delay = formatSection->value.FindMember("delay")->value.GetInt();
	}

	JsonReq.push_back(apply);
	if (jsp->parseStreamObject(dataObj, JsonReq) < 0) {
		return ERR_JSONPARAM;
	}


	auto mosaicSec = dataObj->value.FindMember("mosaic");
	if ( mosaicSec != dataObj->value.MemberEnd() && !mosaicSec->value.Empty()){
		if(jsp->parseMosaicObject(mosaicSec, JsonReq) < 0)
		{
			return ERR_JSONPARAM;
		}
	}

	std::cout << "lparse ___________________\n";


	//filter section for logo


	auto logoSection = dataObj->value.FindMember("logo");
	if (logoSection != dataObj->value.MemberEnd() && !logoSection->value.Empty()){
		if(jsp->parseLogoObject(logoSection, JsonReq) < 0) {
			return ERR_JSONPARAM;
		}
	}

	//filter text
	auto textSection = dataObj->value.FindMember("text");
	if (textSection != dataObj->value.MemberEnd() && !textSection->value.Empty()){ 
		if(jsp->parseTextObject(textSection, JsonReq) < 0) {					
			return ERR_JSONPARAM;
		}
	}
	// time mark
	auto timeSection = dataObj->value.FindMember("timemark");
	if (timeSection != dataObj->value.MemberEnd()) {
		if(jsp->parseTimeMarkObject(timeSection,JsonReq) < 0) {
			return ERR_JSONPARAM;
		}
	}


	// pip doesn't use in this version 
	auto reqidSec = doc.FindMember("requestID");
	if (reqidSec != doc.MemberEnd()) {
		jsp->reqID.at(0).assign(reqidSec->value.GetString());
	}

	return 0;
}

std::string JsonParse::Reponse_AddStream(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);


	rapidjson::Value userArea(rapidjson::kObjectType);
	if (reponse.url.size() > 1){
		userArea.AddMember("previewUrl", rapidjson::StringRef(reponse.url[0].c_str()), doc.GetAllocator());
		userArea.AddMember("httpUrl", rapidjson::StringRef(reponse.url[1].c_str()), doc.GetAllocator());
	}else{
		userArea.AddMember("previewUrl", rapidjson::StringRef(""), doc.GetAllocator());
		userArea.AddMember("httpUrl", rapidjson::StringRef(""), doc.GetAllocator());
	}

	userArea.AddMember("windowID", reponse.windowID, doc.GetAllocator());
	userArea.AddMember("executeId", rapidjson::StringRef(guid.c_str()), doc.GetAllocator());
	userArea.AddMember("hasVideo", reponse.hasVideo, doc.GetAllocator());
	userArea.AddMember("hasAudio", reponse.hasAudio, doc.GetAllocator());

	// download filestream source
	userArea.AddMember("type", reponse.responseType, doc.GetAllocator());
	userArea.AddMember("status", reponse.status, doc.GetAllocator());


	doc.AddMember(rapidjson::StringRef("data"), userArea, doc.GetAllocator());

	MakeReponse(doc, writer, reponse);
	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_RemoveStream(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	rapidjson::Value userArea(rapidjson::kObjectType);

	if (!reponse.url.empty())
		userArea.AddMember("previewUrl", rapidjson::StringRef(reponse.url[0].c_str()), doc.GetAllocator());
	else
		userArea.AddMember("previewUrl", rapidjson::StringRef(""), doc.GetAllocator());

	userArea.AddMember("windowID", reponse.windowID, doc.GetAllocator());
	doc.AddMember(rapidjson::StringRef("data"), userArea, doc.GetAllocator());


	MakeReponse(doc, writer, reponse);
	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_AddSource(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	rapidjson::Value userArea(rapidjson::kObjectType);


	rapidjson::Value Arr(rapidjson::kArrayType);
	for (auto iter : reponse.url) {
		Arr.PushBack(rapidjson::StringRef(iter.c_str()), doc.GetAllocator());
	}
	userArea.AddMember("failUrl", Arr, doc.GetAllocator());

	rapidjson::Value Arrsucc(rapidjson::kArrayType);
	for (auto iter : reponse.succUrl) {
		Arrsucc.PushBack(rapidjson::StringRef(iter.c_str()), doc.GetAllocator());
	}
	userArea.AddMember("succUrl", Arrsucc, doc.GetAllocator());
	doc.AddMember(rapidjson::StringRef("data"), userArea, doc.GetAllocator());

	MakeReponse(doc, writer, reponse);
	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_Create(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	rapidjson::Value userArea(rapidjson::kObjectType);

	if (reponse.url.size() > 0)
		userArea.AddMember("previewUrl", rapidjson::StringRef(reponse.url[0].c_str()), doc.GetAllocator());
	else
		userArea.AddMember("previewUrl", rapidjson::StringRef(""), doc.GetAllocator());

	userArea.AddMember("executeId", rapidjson::StringRef(guid.c_str()), doc.GetAllocator());
	doc.AddMember(rapidjson::StringRef("data"), userArea, doc.GetAllocator());

	MakeReponse(doc, writer, reponse);
	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_Update(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_Stop(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	MakeReponse(doc, writer, reponse);
	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_Close(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);
	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_AddSpacer(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	rapidjson::Value userArea(rapidjson::kObjectType);
	userArea.AddMember("hasVideo", reponse.hasVideo, doc.GetAllocator());
	userArea.AddMember("hasAudio", reponse.hasAudio, doc.GetAllocator());
	userArea.AddMember("type",reponse.responseType,doc.GetAllocator());
	userArea.AddMember("status",reponse.status,doc.GetAllocator());

	std::cout << "fileUrl "<< reponse.url.size()<<std::endl; 
	if(!reponse.url.empty()){
		userArea.AddMember("spaceUrl",rapidjson::StringRef(reponse.url[0].c_str()),doc.GetAllocator());
	}
	else{
		userArea.AddMember("spaceUrl",rapidjson::StringRef(" "),doc.GetAllocator());		
	}
	doc.AddMember(rapidjson::StringRef("data"), userArea, doc.GetAllocator());


	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_RemoveSpacer(rapidjson::Document& doc, ResponseInfo& reponse){
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());	
}

std::string JsonParse::Reponse_UseSpacer(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());
}

std::string JsonParse::Reponse_StopSpacer(rapidjson::Document& doc, ResponseInfo& reponse)
{
	rapidjson::StringBuffer resStr;
	rapidjson::Writer<rapidjson::StringBuffer> writer(resStr);

	MakeReponse(doc, writer, reponse);

	return std::string(resStr.GetString());
}

void JsonParse::MakeReponse(rapidjson::Document& doc, rapidjson::Writer<rapidjson::StringBuffer>& writer, ResponseInfo& reponse)
{
	doc.AddMember("code", reponse.retCode, doc.GetAllocator());
	doc.AddMember("message", "", doc.GetAllocator());


	doc.Accept(writer);
}

int JsonParse::parseRemoveObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq)
{
	if (!member->value.GetArray().Empty())
	{
		for (auto& val : member->value.GetArray())
		{
			TaskApply removeTask(ApplyType::REMOVEFFECT);			
			removeTask.remove_id = val.GetString();		
			JsonReq.push_back(removeTask);
		}		
	}

	return 0;
}

int JsonParse::parseStreamObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq)
{
	
	if (JsonReq.empty())
		return -1;
	
	// stream type need set befor use
	TaskApply& stream = JsonReq.back();
	
	// check  video Mix param
	auto videoMixSec = member->value.FindMember("videoMix");
	if (videoMixSec != member->value.MemberEnd() && (videoMixSec->value.GetArray().Size() > 0)) {
//		stream.export_stream.videoWindowID = -1;
		
		for (auto& val : videoMixSec->value.GetArray()) {
			
			Position window;
			window.id = val.FindMember("videoWindowID")->value.GetInt();
			window.level = val.FindMember("level")->value.GetInt();
			window.rect.x = (val.FindMember("size")->value.GetArray())[0].GetInt();
			window.rect.y = (val.FindMember("size")->value.GetArray())[1].GetInt();
			window.rect.offset_x = (val.FindMember("offset")->value.GetArray())[0].GetInt();
			window.rect.offset_y = (val.FindMember("offset")->value.GetArray())[1].GetInt();
			window.seekPos = -1;
			if(val.FindMember("seekPos") != val.MemberEnd() && val.FindMember("seekPos")->value.IsInt64()){
				window.seekPos = val.FindMember("seekPos")->value.GetInt64();					
			}

			stream.export_stream.videoMix.push_back(window);
			
		}
	}
/*	else { // use video WindowID
		auto videoSec = member->value.FindMember("videoWindowID");
		if (videoSec != member->value.MemberEnd()) {
			std::cout << " use videoWindosID \n";
			stream.export_stream.videoWindowID  = videoSec->value.GetInt();
		}
		else {
			return -1;
		}
		
		return -1;
	}
*/
	// check audio Mix param
	auto audioMixSec = member->value.FindMember("audioMix");
	if (audioMixSec != member->value.MemberEnd() && (audioMixSec->value.GetArray().Size() > 0)) {
//		stream.export_stream.audioWindowID = -1;
	
		for (auto& val : audioMixSec->value.GetArray()) {
			AudioInput ain;			
			ain.id = val.FindMember("audioWindowID")->value.GetInt();
			ain.volume = val.FindMember("volume")->value.GetDouble();
			ain.seekPos = -1;
			if(val.FindMember("seekPos") != val.MemberEnd() && val.FindMember("seekPos")->value.IsInt64()){
				ain.seekPos = val.FindMember("seekPos")->value.GetInt64();					
			}

			stream.export_stream.audioMix.push_back(ain);
		}
	}
/*	else {
		auto audioSec = member->value.FindMember("audioWindowID");
		if (audioSec != member->value.MemberEnd()) {
			stream.export_stream.audioWindowID = audioSec->value.GetInt();
		}
		else {
			return -1;
		}
		
		return -1;
	}*/

	auto vol = member->value.FindMember("volume");
	if (vol != member->value.MemberEnd())
		stream.export_stream.volume = vol->value.GetDouble();
	else {
		stream.export_stream.volume = 1.0;
		std::cout << "not find volume use default 1.0\n";
	}

/*	v2.0.8删除此处seekPos字段，不再解析
	auto seek = member->value.FindMember("seekPos");
	if (seek != member->value.MemberEnd()) {
		stream.export_stream.seekPos = seek->value.GetDouble();
		std::cout << "parse seek pos " << stream.export_stream.seekPos << std::endl;
	}
	else {
		stream.export_stream.seekPos = -1;
		std::cout << "not find seekPos set default\n";
	}
*/
	return 0;
}

int JsonParse::parseMosaicObject(rapidjson::Value::MemberIterator& member, JsonReqInfo&  JsonReq) {
	

	//add
//	auto mosaicAdd = mosaicSec->value.FindMember("add");  "add "  "mosaic"
	if (!member->value.GetArray().Empty())
	{
		for (auto& mos : member->value.GetArray())
		{
			TaskApply apply(ApplyType::MOSAICO);

			strncpy(apply.mosaic.id, mos.FindMember("mosaicId")->value.GetString(),sizeof(apply.mosaic.id)-1);
			apply.mosaic.rect.x = (mos.FindMember("size")->value.GetArray())[0].GetInt();
			apply.mosaic.rect.y = (mos.FindMember("size")->value.GetArray())[1].GetInt();
			apply.mosaic.rect.offset_x = (mos.FindMember("offset")->value.GetArray())[0].GetInt();
			apply.mosaic.rect.offset_y = (mos.FindMember("offset")->value.GetArray())[1].GetInt();
			apply.mosaic.unit_pixel = mos.FindMember("unitPixel")->value.GetInt();
			apply.mosaic.level = mos.FindMember("level")->value.GetInt();

			JsonReq.push_back(apply);
		}
	}

	return 0;
}

int JsonParse::parseLogoObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq) {

	if (!member->value.Empty()) {
		
		for (auto& logo : member->value.GetArray()) {
			TaskApply apply(ApplyType::LOGO);

			strncpy(apply.logo.name, logo.FindMember("file")->value.GetString(),sizeof(apply.logo.name)-1);
			strncpy(apply.logo.id, logo.FindMember("picId")->value.GetString(),sizeof(apply.logo.id));
			apply.logo.rect.x = (logo.FindMember("size")->value.GetArray())[0].GetInt();
			apply.logo.rect.y = (logo.FindMember("size")->value.GetArray())[1].GetInt();
			// alpha and level no use
			apply.logo.rect.offset_x = (logo.FindMember("offset")->value.GetArray())[0].GetInt();
			apply.logo.rect.offset_y = (logo.FindMember("offset")->value.GetArray())[1].GetInt();
			apply.logo.Alpha = logo.FindMember("Alpha")->value.GetDouble();
			apply.logo.level = logo.FindMember("level")->value.GetInt();

			JsonReq.push_back(apply);
		}
	}

	return 0;
}

int JsonParse::parseTextObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq) {

	if (!member->value.Empty()) {
		for (auto& text : member->value.GetArray()) {
			TaskApply apply(ApplyType::TEXT);

			strncpy(apply.text.subtitle, text.FindMember("text")->value.GetString(),sizeof(apply.text.subtitle)-1);
			strncpy(apply.text.id, text.FindMember("textId")->value.GetString(), sizeof(apply.text.id) - 1);
			strncpy(apply.text.font, text.FindMember("front")->value.GetString(), sizeof(apply.text.font) - 1);
			strncpy(apply.text.color, text.FindMember("color")->value.GetString(),sizeof(apply.text.color) - 1);

			if (text.FindMember("fontWidth") != text.MemberEnd()) {
				apply.text.fontWidth = text.FindMember("fontWidth")->value.GetDouble();
			}
			else {
				std::cout << "JsonParse: not find fontWidth \n";
				apply.text.fontWidth = 0.0;
			}
			

			if (text.FindMember("alpha") != text.MemberEnd()) {
				apply.text.alpha = text.FindMember("alpha")->value.GetDouble();
			}
			else {
				std::cout << "JsonParse: not find font alpha \n";
				apply.text.alpha = 0.0;
			}

			if (text.FindMember("fontAlpha") != text.MemberEnd()) {
				apply.text.fontAlpha = text.FindMember("fontAlpha")->value.GetDouble();
			}
			else {
				std::cout << "JsonParse: not find font alpha \n";
				apply.text.alpha = 0.0;
			}
		
			apply.text.x = text.FindMember("x")->value.GetDouble();
			apply.text.y = text.FindMember("y")->value.GetDouble();
			apply.text.speed = text.FindMember("speed")->value.GetDouble();
			apply.text.moveType = text.FindMember("style")->value.GetInt();
			apply.text.second = text.FindMember("delay")->value.GetInt();
			apply.text.fontSize = text.FindMember("frontSize")->value.GetInt();

			apply.text.level = text.FindMember("level")->value.GetInt();

			std::cout << "get front size " << apply.text.fontSize << std::endl;

			if (text.FindMember("box") != text.MemberEnd())
				apply.text.box = text.FindMember("box")->value.GetInt();

			if (text.FindMember("boxborderw") != text.MemberEnd())
				apply.text.boxborderw = text.FindMember("boxborderw")->value.GetInt();

			if (text.FindMember("boxcolor") != text.MemberEnd())
				strncpy(apply.text.boxcolor, text.FindMember("boxcolor")->value.GetString(), sizeof(apply.text.boxcolor)-1);

			JsonReq.push_back(apply);
		}
	}
	return 0;
}

int JsonParse::parseTimeMarkObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq) {

	if (!member->value.Empty()) {
		for (auto& text : member->value.GetArray()) {
			TaskApply apply(ApplyType::TIMEMARK);

		//	strncpy(apply.text.subtitle, text.FindMember("text")->value.GetString(),sizeof(apply.text.subtitle)-1);
			strncpy(apply.text.id, text.FindMember("textId")->value.GetString(), sizeof(apply.text.id) - 1);

			if (text.FindMember("front") != text.MemberEnd()) {
				strncpy(apply.text.font, text.FindMember("front")->value.GetString(), sizeof(apply.text.font) - 1);
			}
			else {
				std::cout << "JsonParse: not find font param \n";
				strncpy(apply.text.font, "\u9ed1\u4f53", sizeof("\u9ed1\u4f53") - 1); // simhei
			}

			strncpy(apply.text.color, text.FindMember("color")->value.GetString(),sizeof(apply.text.color) - 1);
	
			if (text.FindMember("fontAlpha") != text.MemberEnd()) {
				apply.text.fontAlpha = text.FindMember("fontAlpha")->value.GetDouble();
			}
			else {
				std::cout << "JsonParse: not find font alpha \n";
				apply.text.alpha = 1.0;
			}
		
			apply.text.x = text.FindMember("x")->value.GetDouble();
			apply.text.y = text.FindMember("y")->value.GetDouble();
			apply.text.fontSize = text.FindMember("frontSize")->value.GetInt();

			if (text.FindMember("delay") != text.MemberEnd()) {
				apply.text.delay = text.FindMember("delay")->value.GetInt(); // ms	
			}else{
				apply.text.delay = 0;
			}


			apply.text.level = text.FindMember("level")->value.GetInt();

			std::cout << "get front size " << apply.text.fontSize << std::endl;


			JsonReq.push_back(apply);
		}
	}
	return 0;
}



void JsonParse::serialize(){
	if(fp){	
		mutex_syncFile.lock();
		fwrite("\n[JsonInfo]",1,sizeof("\n[JsonInfo]")-1,fp);
		if(!instanceID.empty())	{
			char insID[256]={};
			snprintf(insID,256,"\ninstanceID=%s",instanceID.c_str());
			fwrite(insID,1,strlen(insID),fp);
		}

		if(!userID.empty())	{
			char uID[256]={};
			snprintf(uID,256,"\nuserID=%s",userID.c_str());
			fwrite(uID,1,strlen(uID),fp);
		}

		mutex_syncFile.unlock();
	}
}

const char* GenUUID() {

#if _WIN32||_WIN64
	static char MGUID[64] = {};
	GUID guid;
	if (CoCreateGuid(&guid)) {
		return nullptr;
	}
	sprintf_s(MGUID, sizeof(MGUID), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);
#elif  __unix||__linux||__unix__||__FreeBSD__
	static char MGUID[128] = {};
	uuid_t guid;
	uuid_generate(guid);
	snprintf(MGUID, 64, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X%02X%02X",
		guid[0], guid[1], guid[2], guid[3],
		guid[4], guid[5],
		guid[6], guid[7],
		guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
#endif
	return MGUID;
}
