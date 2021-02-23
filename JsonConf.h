#pragma once
#include "rapidjson/document.h"
#include <mutex>
#include <fstream>
#include <iostream>
#include <vector>


class Config {
public:
	static const Config* getConf(const std::string& path);

public:
	
	double GetPreviewRation() const{ return Pre_ratio; }
	int GetVideoBits() const { return Pre_VideoBits; }
	int GetAudioChannel() const { return Pre_AudioChannel; }
	int GetAudioSample() const { return Pre_AudioSample; }
	int GetAudioBits() const  { return Pre_AudioBits; }
	int64_t GetStreamChannelExp() const {return streamChannelExp;}
	int GetRtmpPort() const {return rtmpPort;}
	int GetHttpPort() const {return httpPort;}
	bool IsPreviewFile() const {return previewFile;}
	
	
	std::vector<std::string> GetRtmpServer() const {return Pre_Host;}
	std::string GetSection()const {return Pre_Section;}
	std::string GetPreset()const{return preset;}
	std::string GetFlvVisitProtocol() const {return flvProtocol;}
	std::string GetCacheFilePath() const {return TmpFilePath;}
protected:
	Config(const std::string& path):
		previewFile(false),
		Pre_ratio(0.2), Pre_VideoBits(250),
		Pre_AudioChannel(2), Pre_AudioSample(16000), Pre_AudioBits(32),
		rtmpPort(1935),httpPort(443),streamChannelExp(30*1000),
		preset("ultrafast"),flvProtocol("https"),TmpFilePath("/tmp/"){
		std::fstream  fp;
		fp.open(path);
		
		fp.seekg(0,std::ios::end);
		size_t fileSize = fp.tellg();
		std::cout << "file size " << fileSize << std::endl;


		fp.seekg(0,std::ios::beg);
		char* readBuf = new char[fileSize+1];
		memset(readBuf, 0, fileSize + 1);
		fp.read(readBuf, fileSize);
		readBuf[fileSize] = 0;
		std::cout << "read from " << readBuf << std::endl;
		
		

		parseJson(readBuf);
		fp.close();
	}
	
	~Config() {}
	Config(const Config&) = delete;
	Config  operator=(const Config&) = delete;
protected:
	bool previewFile;
	double Pre_ratio;
	int Pre_VideoBits;
	
	int Pre_AudioChannel;
	int Pre_AudioSample;
	int Pre_AudioBits;
	int rtmpPort;
	int httpPort;

	int64_t streamChannelExp;

	std::vector<std::string> Pre_Host;
	std::string Pre_Section;
	std::string preset;
	std::string flvProtocol;
	std::string	TmpFilePath;

	int parseJson(const char *json);
private:
	static std::mutex mutex_safe;
	static Config* cof;
};


