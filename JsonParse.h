#pragma once
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "TaskApply.h"
#include <functional>
#include <string>
#include <map>
#include <array>
#if _WIN32||_WIN64
#include <objbase.h>
#elif  __unix||__linux||__unix__||__FreeBSD__
#include <uuid/uuid.h>  // gcc -luuid
#endif
const char* GenUUID();


class JsonParse:private Serial
{
public:
	JsonParse();
	~JsonParse();
	int Request(const char* str, JsonReqInfo& task);
	std::string Response(const char* src, ResponseInfo& task);
	std::string ReportStreamStatus( ResponseInfo& reponse);
	std::string ReportDownloadProgress(const char* str,int windowID,int64_t total,int64_t cache);
	
protected:
	// parse request 
	static int Request_AddStream(rapidjson::Document& doc, JsonReqInfo & JsonReq,JsonParse*);
	static int Request_RemoveStream(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_AddSource(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_Create(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_Update(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_Stop(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_Close(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_AddSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_RemoveSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_UseSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);
	static int Request_StopSpacer(rapidjson::Document& doc, JsonReqInfo& JsonReq,JsonParse*);


	

	// make response
	static std::string Reponse_AddStream( rapidjson::Document& doc,  ResponseInfo& reponse);
	static std::string Reponse_RemoveStream(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_AddSource(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_Create(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_Update(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_Stop(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_Close(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_AddSpacer(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_RemoveSpacer(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_UseSpacer(rapidjson::Document& doc, ResponseInfo& reponse);
	static std::string Reponse_StopSpacer(rapidjson::Document& doc, ResponseInfo& reponse);


	static void MakeReponse(rapidjson::Document& doc,rapidjson::Writer<rapidjson::StringBuffer>& writer, ResponseInfo& reponse);
private:
	int parseMosaicObject(rapidjson::Value::MemberIterator& member, JsonReqInfo&  JsonReq);
	int parseLogoObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq);
	int parseTextObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq);
	int parseTimeMarkObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq);
	int parseRemoveObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq);
	int parseStreamObject(rapidjson::Value::MemberIterator& member, JsonReqInfo& JsonReq);
private:
//	bool needRecord;
	std::string instanceID;
	std::string	userID;

	std::map<std::string, std::function<int(rapidjson::Document&, JsonReqInfo&,JsonParse*)>> RequestMap;
	std::map < std::string, std::function < std::string (rapidjson::Document&, ResponseInfo&) >> ReponseMap;
	static std::string guid;

	std::array<std::string,MAX_INPUTSTREAM> reqID;

protected:
	virtual void serialize() final;	
};

