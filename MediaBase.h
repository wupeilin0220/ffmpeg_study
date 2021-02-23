#pragma once
#include <thread>
#include <iostream>
#include <future>
#include <map>
#include <list>
#include <set>
#include "JsonConf.h"
#include "TaskApply.h"

enum class MEDIATYPE {
	AUDIO = 0,
	VIDEO
};
typedef struct DataSet{
	MEDIATYPE type;
	void* srcData;
}DateSet;


class Link {
public:	
	virtual ~Link() {}
	virtual int fillBuffer(DataSet p) = 0;
	virtual Link* getNext()=0;
//	virtual Link* getNext_Second()=0;
	
};

class Linker :public Link {
public:
	Linker() :link(nullptr){}
	virtual void linkTo(Link* dst) {
		mutex_link.lock();
		link = dst;
		mutex_link.unlock();
	}
	
	virtual ~Linker(){}
	virtual Link* getNext() override { return link; };

protected:
	std::mutex mutex_link;
	Link* link;
};

class broadCast {
public:
	virtual int BroadData(DataSet& p, int pin) = 0;
};

class Pin :public Linker{
public:
	Pin(int id, broadCast*dst) :pinNum(id),broad(dst) {}

	virtual int fillBuffer(DataSet p)  override {
		return (broad) ? broad->BroadData(p,pinNum) :  -1;
	}
	virtual ~Pin(){}
public:
	const int pinNum;
	broadCast* broad;
};

class eightPinLink:protected  broadCast{	
public:	
	eightPinLink():pins()
	{
		for (int i(0); i < PINS; i++) {
			pins[i] = new Pin(i, this);
		}
	}

	Pin* GetPin(int num) {
		Pin* p = nullptr;
		if (num >= 0 && num < PINS) {
			p = pins[num];
		}
		return p;
	}	

	virtual ~eightPinLink() {
		for (int i(0); i < PINS; i++) {
			delete pins[i];
			pins[i] = nullptr;
		}
	}

protected:		
//	Link* link;
	Pin* pins[PINS];
//	std::mutex mutex_link;
};



