#include "NetStreamListen.h"

NetStreamListen::NetStreamListen():
	hand_listen(nullptr),
	netStream(), removeLater(),
	run(true)
{

}

NetStreamListen::~NetStreamListen()
{
	StopListen();	
}

int NetStreamListen::ListenNetStream(CallBack call)
{
	if (!hand_listen) {
		hand_listen = new std::thread([&]() {this->listenThread(); });
		reportFun = call;
	}
	return 0;
}

int NetStreamListen::StopListen()
{
	run = false;
	if(hand_listen){
		if(hand_listen->joinable()){
			hand_listen->join();
		}
		delete hand_listen;
		hand_listen = nullptr;
	}
	return 0;
}

int NetStreamListen::Delete(DecoderNet* dec)
{
	if(dec){
		std::cout << "listen: delete net stream "<<std::endl;
		listen_lock.lock();
		removeLater.push_back(dec);
		listen_lock.unlock();
	}
	return 0;
}

int NetStreamListen::Add(DecoderNet* dec)
{
	listen_lock.lock();
	addLater.push_back(dec);
	listen_lock.unlock();
	return 0;
}

int NetStreamListen::listenThread()
{
	while (run){
		listen_lock.lock();
		// remove listen dec
		for (auto iter_remove : removeLater) {
			for (auto iter(netStream.begin()); iter != netStream.end();iter++) {
				if (*iter == iter_remove) {
					std::cout << "delete Decoder Stream "<< *iter<<std::endl;
					delete iter_remove;
					iter_remove = nullptr;
					netStream.erase(iter);
					break;
				}
			}
		}
		removeLater.clear();

		// add listen
		for (auto val : addLater) {
			netStream.push_back(val);
		}
		addLater.clear();
		listen_lock.unlock();


		for (auto dec : netStream) {
			if (dec->IsBroken()) {
				reportFun(dec,2); // is broken 	

				if (dec->reConnected() < 0) {
					reportFun(dec, 2); // is broken 	
				}
				else {
					reportFun(dec, 0); // reconnect 	
				}
			}
			else {
				reportFun(dec, 1); // success
			}
		}
		
		av_usleep(5 * 1000 * 1000);
	}

	std::cout << "NetStreamListen:: quit send stream broken signal\n";
//	for (auto dec : netStream) {
//		reportFun(dec, 2); // is broken 	
//	}
	return 0;
}

void NetStreamListen::StreamIsBroken(int64_t ,DecoderNet* dec){
	std::cout << "first time call stream broken fun\n";
	if(reportFun)
		reportFun(dec,2);
}


