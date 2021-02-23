#pragma once
#include <iostream>
#include <mutex>

class Serial{
public:
	static bool setSerialFileName(const std::string& name) {
		path = name;
		fp = fopen(name.c_str(),"wb+");
		if(fp == nullptr){
			return false;
		}
		return true;
	}
	
	static bool stopSerialize(){
		if(fp){
			mutex_syncFile.lock();
			fclose(fp);
			fp = nullptr;
			mutex_syncFile.unlock();
			std::cout << "~Serial()______________________\n"; 

		}
		return true;
	}

	virtual ~Serial() {
	}
protected:
//	Serial() {}
//	Serial& operator=(const Serial&) = delete;
//	Serial(const Serial&) = delete;
	virtual void serialize() {}
protected:
	static std::string path;
	static std::mutex mutex_syncFile;
	static FILE* fp;
};

