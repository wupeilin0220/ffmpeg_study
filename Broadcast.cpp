#include "Broadcast.h"
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include<fcntl.h>
#include <sys/select.h>
#include <memory.h>
#include <cstddef>
extern "C"{
#include <stdlib.h>
#include <time.h>
};
#include <sys/syscall.h>
#define gettid() syscall(__NR_gettid)

Broadcast::Broadcast():
	init(false),
	requestFd(-1),responseFd(-1),statusFd(-1),
	handle_listen(nullptr),timeout(38)
{
	std::function<int(int,int)> fun = std::bind(&Broadcast::statusFeedback,this,std::placeholders::_1,std::placeholders::_2);
	broad.regNetStreamMonitor(fun);

	std::function<int (TaskApply, bool)> over = std::bind(&Broadcast::DownloadOverCB,this,std::placeholders::_1,std::placeholders::_2);
	broad.regDownloadOverCB(over);

	std::function<int (TaskApply, int64_t,int64_t)> progress = std::bind(&Broadcast::DownlaodProgress,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3);
	broad.regDownloadProgress(progress);
}

Broadcast::~Broadcast(){

	stopListen();
}

int Broadcast::initFifo(int argc,char ** argv){
	if(argc < 4){
		std::cout << "fifo's num not suff\n";
		return -1;
	}

	for(int i(1);i<4;i++){
		std::cout<< "fifo " << argv[i]<<std::endl;
	}

	if(init)
		return 0;

	if(access(argv[1],F_OK) == 0){
		requestFd = open(argv[1],O_RDONLY);		
		if(requestFd < 0){
			std::cout << "create reuqest fifo fail\n";
			return requestFd;
		}
	}else{
		return -1;
	}
		
	if(access(argv[2],F_OK) == 0){
		responseFd = open(argv[2],O_WRONLY);		
		if(responseFd < 0){
			close(requestFd);
			std::cout << "create response  fifo fail\n";
			return requestFd;
		}
	}else{
		return -1;
	}

	if(access(argv[3],F_OK) == 0){
		statusFd = open(argv[3],O_WRONLY);		
		if(statusFd < 0){
			close(requestFd);
			close(responseFd);
			std::cout << "create status fifo fail\n";
			return requestFd;
		}
		set_noblocking(statusFd);
	}else{
		return -1;
	}


	init = true;
	std::cout << "create fifo successs\n"<<std::endl;
	return 0;
}


int Broadcast::listenRequest(bool& run){
	constexpr int HEADLEN = 8;
	char p[HEADLEN] = {};
	timeval timeOut;
	timeOut.tv_sec = 0;
	timeOut.tv_usec = 500000;

	fd_set readFD;
	FD_ZERO(&readFD);
	FD_SET(requestFd,&readFD);

	while(run){
		fd_set rSet = readFD;
		timeval tmp = timeOut;
		int ret = select(requestFd + 1,&rSet,NULL,NULL,&tmp);
		if(ret < 0){
			std::cout << "select err process quit\n";
			break;
		}

		execExternTask();
		if(ret == 0){
			continue;
		}
		memset(p,0,HEADLEN+1);
		ret = read(requestFd,p,HEADLEN);
		if(ret != HEADLEN){

			// should read all in pipe,and drop 
			clearFifo(requestFd);
			continue;
		}
		int dataLen = atoi(p);
		if((dataLen > 0) && (dataLen < 16*4096)){ //pipe max size

		}else{
			std::cout << "WARNING: parse head data err,data size 0 or more than "<< 16*4096<<std::endl;
			//do same thing clean pipe
			clearFifo(requestFd);
			continue;
		}

		char *data = new char[dataLen+1];
		ret = read(requestFd,data, dataLen);
		std::cout << "act read_______________"<< ret<<std::endl;
		if(ret == dataLen){
			// send data
			// xxx()
			int64_t sta = av_gettime();	
			JsonReqInfo reqInfo;
			data[ret]= '\0';
			std::cout << makTime() <<" deal json in_____________________________\n" << data<<std::endl;

			std::string backup(data);
			if(json.Request(data,reqInfo) != 0){
				std::cout << "parse err_________________________\n";
				ResponseInfo resInfo;
				resInfo.retCode = ERR_JSONPARAM;
				std::string str = json.Response(backup.c_str(), resInfo);

				char head[9] = {};
				int size = str.size();
				snprintf(head,9,"%08d", size);	
				std::string sendData = head;
				sendData += str;
				ret = write(responseFd,sendData.c_str(),sendData.size());

				delete []data;
				continue;
			}

			
			std::cout << "task size "<< reqInfo.size() <<" ParseTask.....................\n";

			ResponseInfo resInfo;
			broad.TaskParse(reqInfo,resInfo);


			std::cout << "ParseTask Over.....................\n";

			
			std::string str = json.Response(backup.c_str(), resInfo);

			int64_t end =av_gettime() - sta;
			if(end < (timeout*1000*1000)){
				char head[9] = {};
				int size = str.size();
				snprintf(head,9,"%08d", size);	
				std::string sendData = head;
				sendData += str;
			ret = write(responseFd,sendData.c_str(),sendData.size());
				std::cout <<"time use \n"<<(end)/1000.0 <<" write_________________________________________\n "<< ret <<" data: "<<str<<std::endl;
			}else{
				std::cout << "response time out ,drop message______________\n";
			}

		}else{
			//std::cout << "WARNING: read data size not equal header size\n";
		} 

//		std::cout << "act read "<< ret << " data "<<data<<std::endl;
		delete []data;

	}
	std::cout << "listen request fifo is quit\n";
	run = false;
	return 0;
}

int Broadcast::stopListen(){
	//	run = false;
	if(handle_listen){
		if(handle_listen->joinable())
			handle_listen->join();
		delete handle_listen;
		handle_listen = nullptr;
	}
	return 0;
}

int Broadcast::clearFifo(int pipeFd){

	while(1){
		char data[4096]={};
		int ret = read(pipeFd, data,4096);
//		std::cout << "clear data "<< ret << data<<std::endl;
		if(ret <= 0)
			break;
	}
	return 0;
}


int Broadcast::statusRes(const char *data,int len){

	int ret = write(statusFd, data, len);
	return ret;
}

int Broadcast::statusFeedback(int windowID,int status){
	ResponseInfo info;
	info.windowID = windowID;
	info.status = status;
	std::string jsonStr = json.ReportStreamStatus(info);
	char head[9] = {};
	int size = jsonStr.size();
	snprintf(head,9,"%08d", size);	
	std::string sendData = head;
	sendData += jsonStr;
//	std::cout << "stream status:\n" << sendData<<std::endl;
	return statusRes(sendData.c_str(),sendData.size());
}


int Broadcast::setTimeout(int64_t time){	
	timeout = time;
	return 0;
}


std::string Broadcast::makTime(){
	time_t now;
	time(&now);

	struct tm *info = localtime( &now );
	
	std::string timeNow = std::to_string(info->tm_mon + 1) + "-"+ std::to_string(info->tm_mday)+" "+std::to_string(info->tm_hour)+":"+std::to_string(info->tm_min) + ":"+std::to_string(info->tm_sec);

	return timeNow;
}

int Broadcast::set_noblocking(int fd){
	int flags;
	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Broadcast::listen(bool& run){
	if(init){
		if(!handle_listen)
			handle_listen = new std::thread([&](){this->listenRequest(std::ref(run));});
	}
	
}
void Broadcast::GenerateFailRespose(ResponseInfo& res, TaskApply& task){
	res.windowID = task.stream.windowID;
	res.retCode = ERR_DownloadFail;
	res.responseType = 2;
	res.status = 0;
	if(task.getType() == ApplyType::ADDSPACER){  
		res.url = task.stream.urlStream;
	}
	/*else if(task.getType() == ApplyType::ADDSTREAM){
		res.setType(ApplyType::ADDSTREAM);		
	} */
}


int Broadcast::DownloadOverCB(TaskApply task,bool succ){
	std::cout << "download task "<< succ<<std::endl;
	// 重新请求添加任务	
	ResponseInfo res;
	std::string msg;
	JsonReqInfo li;
	// download task fail
	if(!succ){  
		GenerateFailRespose(res,task);
		std::cout << task.reqMsg<<std::endl;
		msg = json.Response(task.reqMsg.c_str(),res);
		goto _end;
	}
	 
	// 如果下成功，将任务放到队列，由listen线程执行
	mutex_extra.lock();
	ExtraTask.push_back(task);
	mutex_extra.unlock();
	return 0;
_end:
	char head[9] = {};
	int size = msg.size();
	snprintf(head,9,"%08d", size);	
	std::string sendData = head;
	sendData += msg;	
	std::cout << sendData<<std::endl;
	return statusRes(sendData.c_str(),sendData.size());
}

int Broadcast::DownlaodProgress(TaskApply task,int64_t total,int64_t cache){	
	int id = 9;
	if(task.getType() == ApplyType::ADDSTREAM)
		id = task.stream.windowID;
	
	std::string msg = json.ReportDownloadProgress(task.reqMsg.c_str(),id,total,cache);
	
	char head[9] = {};
	int size = msg.size();
	snprintf(head,9,"%08d", size);	
	std::string sendData = head;
	sendData += msg;	
//	std::cout << sendData <<std::endl;
	return statusRes(sendData.c_str(),sendData.size());
}

int Broadcast::execExternTask(){
	int i = 0;
	mutex_extra.lock();
	while(!ExtraTask.empty()){
		i++;
		JsonReqInfo reqTask;
		reqTask.push_back(ExtraTask.front());

		ResponseInfo resInfo;
		broad.TaskParse(reqTask,resInfo);
		std::string msg = json.Response(ExtraTask.front().reqMsg.c_str(), resInfo);

		char head[9] = {};
		int size = msg.size();
		snprintf(head,9,"%08d", size);	
		std::string sendData = head;
		sendData += msg;	
		statusRes(sendData.c_str(),sendData.size());
		std::cout << "Broadcast::ExtraTask "<< sendData <<std::endl;
		ExtraTask.pop_front();
	}
	mutex_extra.unlock();	
	return i;
}

