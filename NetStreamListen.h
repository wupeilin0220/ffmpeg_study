#pragma once
#include "DecoderNet.h"
typedef std::function<void(DecoderNet*,int)> CallBack;
class NetStreamListen
{
public:
	NetStreamListen();
	~NetStreamListen();
	int ListenNetStream(CallBack call);
	int StopListen();
	int Delete(DecoderNet* dec);
	int Add(DecoderNet* dec);
	void StreamIsBroken(int64_t time,DecoderNet* dec);
protected:
	int listenThread();
private:
	std::mutex listen_lock;
	std::thread* hand_listen;
	std::list<DecoderNet*> netStream;
	std::list<DecoderNet*> removeLater;
	std::list<DecoderNet*> addLater;
	CallBack reportFun;
	
	volatile bool run;
	
};

