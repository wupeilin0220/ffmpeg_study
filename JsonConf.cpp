#include "JsonConf.h"

Config* Config::cof = nullptr;
std::mutex Config::mutex_safe;
int Config::parseJson(const char* json)
{
	rapidjson::Document doc;
	doc.Parse(json);
	auto mediaSec = doc.FindMember("MediaInfo");
	if (mediaSec != doc.MemberEnd()) {

		auto ratioSec = mediaSec->value.FindMember("previewRatio");
		if(ratioSec != mediaSec->value.MemberEnd()){
			Pre_ratio = ratioSec->value.GetDouble();	
		}

		auto VideoBitsSec = mediaSec->value.FindMember("videoBits");
		if (VideoBitsSec != mediaSec->value.MemberEnd()) {
			Pre_VideoBits = VideoBitsSec->value.GetInt();
		}


		auto audioSampleSec = mediaSec->value.FindMember("audioSample");
		if (audioSampleSec != mediaSec->value.MemberEnd()) {
			Pre_AudioSample = audioSampleSec->value.GetInt();
		}

		auto audioChannelSec = mediaSec->value.FindMember("audioChannel");
		if (audioChannelSec != mediaSec->value.MemberEnd()) {
			Pre_AudioChannel = audioChannelSec->value.GetInt();
		}

		auto audioBitsSec = mediaSec->value.FindMember("audioBits");
		if (audioBitsSec != mediaSec->value.MemberEnd()) {
			Pre_AudioBits = audioBitsSec->value.GetInt();
		}

		auto expSec = mediaSec->value.FindMember("streamChannelExp");
		if(expSec != mediaSec->value.MemberEnd()){
			streamChannelExp = expSec->value.GetInt64();
		}

		auto presetSec = mediaSec->value.FindMember("preset"); 
		if(presetSec != mediaSec->value.MemberEnd()){
			preset = presetSec->value.GetString(); 
		}

	}

	auto RtmpServerSec = doc.FindMember("RtmpServer");
	if (RtmpServerSec != doc.MemberEnd()) {
		auto hostSec = RtmpServerSec->value.FindMember("host");
		if (hostSec != RtmpServerSec->value.MemberEnd()) {
			for(auto& val:hostSec->value.GetArray()){
				Pre_Host.push_back(val.GetString());
			}
		}

		auto sectionSec = RtmpServerSec->value.FindMember("section");
		if (sectionSec != RtmpServerSec->value.MemberEnd()) {
			Pre_Section = sectionSec->value.GetString();
		}

		auto RtmpSec = RtmpServerSec->value.FindMember("rtmpPort"); 
		if(RtmpSec != RtmpServerSec->value.MemberEnd()){
			rtmpPort = RtmpSec->value.GetInt();
			if(rtmpPort <= 0)
			{
				std::cout << "JsonConf:: parse config rtmp port error "<< rtmpPort<< " set default 1935\n";

				rtmpPort = 1935;
			}
		}

		auto HttpSec = RtmpServerSec->value.FindMember("httpPort");
		if(HttpSec != RtmpServerSec->value.MemberEnd()){
			httpPort = HttpSec->value.GetInt();
			if(httpPort <= 0){
				std::cout << "JsonConf:: parse config http-flv port error "<< rtmpPort<< " set default 80\n";
				httpPort = 80;
				
			}
		}
	}

	auto protocol = doc.FindMember("FlvVisit");
	if(protocol != doc.MemberEnd() && protocol->value.IsString()){
		flvProtocol = protocol->value.GetString();
	}
	
	auto FileVod = doc.FindMember("PreviewFile");
	if(FileVod != doc.MemberEnd() && FileVod->value.IsBool()){
		previewFile = FileVod ->value.GetBool();
	}
	
	auto tmpPath = doc.FindMember("CachePath");
	if(tmpPath != doc.MemberEnd() && tmpPath->value.IsString()){
		TmpFilePath = tmpPath->value.GetString();	
	}
	return 0;
}



const Config* Config::getConf(const std::string& path) {
	mutex_safe.lock();
	if (!cof) {
		cof = new Config(path);
	}
	mutex_safe.unlock();
	return cof;
}
