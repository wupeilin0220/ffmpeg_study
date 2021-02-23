#pragma once
//#include <sys/type.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include "JsonParse.h"
#include "Decoder.h"
#include "TaskBroadcast.h"
#include <list>
class Broadcast{
public:
	Broadcast();
	~Broadcast();
	int initFifo(int argc,char ** argv);
	int resRequest(char *data,int len);
	int statusRes(const char *data,int len);
	int stopListen();
	int setTimeout(int64_t time);
	void listen(bool& run);
protected:
	int clearFifo(int pipeFD);
	int statusFeedback(int windowID,int status);
	std::string makTime();
	int set_noblocking(int fd);
	int listenRequest(bool& run);
private:
	bool init;
	int requestFd,responseFd,statusFd;
	std::thread* handle_listen;
	JsonParse json;
	TaskBroadcast broad;
	int64_t timeout;

public:
	int DownloadOverCB(TaskApply task,bool succ);
	int DownlaodProgress(TaskApply task,int64_t total,int64_t cache);
	void GenerateFailRespose(ResponseInfo& res,TaskApply& task);
	
private:
	int execExternTask();
	std::list<TaskApply> ExtraTask;
	std::mutex mutex_extra;
};
