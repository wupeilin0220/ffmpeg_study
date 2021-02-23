#pragma once
#include <iostream>
#include <stdio.h>
#include "DecoderNet.h"
#include "DecoderFile.h"
#include "FFmpegEncoder.h"
#include "FilterMgr.h"
#include "split.h"
#include "AspectRatio.h"
#include "AudioResample.h"
#include "PictureStream.h"
#include "ResourceCreater.h"
#include "NetStreamListen.h"
#include "StreamMix.h"
#include "TaskApply.h"
#define NOSOURCE -1
// add DownloadOver CurrentProgress fun param for report download status
typedef std::function<int(TaskApply,bool)> DownloadOverCB;
typedef std::function<int(TaskApply,int64_t,int64_t)> ProgressCB;


class TaskBroadcast: private Serial
{
public:
	TaskBroadcast();
	~TaskBroadcast();
	int TaskParse(JsonReqInfo& task, ResponseInfo& response);
	void regNetStreamMonitor(StatusCall monitor){feedback = monitor;}
	void regDownloadProgress(ProgressCB progress){ progressCB = progress;}
	void regDownloadOverCB(DownloadOverCB over){ overCB = over;}
private:
	int AddStream(TaskApply& task, ResponseInfo& response);
	int RemoveStream(TaskApply& task, ResponseInfo& response);
	int Create(TaskApply& task, ResponseInfo& response);
	int effectChange(TaskApply& task, ResponseInfo& response);
	int UpdateStream(TaskApply& task, ResponseInfo& response);
	int StopPushStream(TaskApply& task, ResponseInfo& response);
	int EraseAll(TaskApply& task, ResponseInfo& response);
	int AddSource(TaskApply& task, ResponseInfo& response);
	int AddSpaceStream(TaskApply& task, ResponseInfo& response);
	int RemoveDownloadingSpacer(TaskApply& task, ResponseInfo& response);
	int StopSpaceStream(TaskApply& task, ResponseInfo& response);
	int UseSpaceStream(TaskApply& task, ResponseInfo& response);
private:
	int removeMiddel(Link* begin, Link* end);
	int ReportStreamStatus(DecoderNet* obj, int status);
	bool isNetStreamUrl(const std::string& url);
	bool isStreamNormal(std::set<int>& idSet);
	bool isStreamNormal(int id);
private:
	Decoder* dec[MAX_INPUTSTREAM];
	FFmpegEncoder* encPush;
	TaskApply outputTask;
	int		videoIndex;
	int		audioIndex;
	FilterMgr* manager;
	NetStreamListen* listen;
	StatusCall feedback;
	std::string emergencyFile;
	std::vector<std::string> emergencyPic;
	StreamType emerType;
	int64_t intervalTime;
	std::set<int> vmixID; // vmix
	std::set<int> amixID; // amix
	bool  streamStatus[MAX_INPUTSTREAM]; //bool: true stream broken
	StreamMix *mixer;
protected:
	virtual void serialize() final;
private:
	DownloadOverCB	overCB;
	ProgressCB		progressCB;

	ResourceCreater* spacerRes;
	ResourceCreater* streamRes[MAX_INPUTSTREAM];
	std::mutex mutex_addStream;
	std::mutex mutex_spacer;
};

