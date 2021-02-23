
#include <sys/types.h>
#include <signal.h>
#include "Broadcast.h"
#include "ResourceCreater.h"
#include "TimeMark.h"
#include "FadeFilter.h"
static TaskBroadcast broad;


static bool run = true;
static std::string SerialFileName;
void singnalAct(int sigNum) {
	std::cout << "get signal " << sigNum << std::endl;
	Serial::setSerialFileName(SerialFileName);
	run = false;
}
/*
int testTimeMark(){
	DecoderFile fileStream;
	int ret = fileStream.init_decoder("fish.mp4",1);
	if (ret < 0) {
		std::cout << "Decoder File error ret " << ret << std::endl;
		return ret;
	}

	TaskApply task_mark(ApplyType::TEXT);
	task_mark.text.fontSize = 20;
	task_mark.text.fontAlpha = 0.3;
	task_mark.text.x = 30;
	task_mark.text.y = 30;
	task_mark.text.delay = 300000;
	strcpy(task_mark.text.font,"\u9ed1\u4f53");
	strcpy(task_mark.text.color,"0xFFFFFF");

	TimeMark tmark(fileStream.getMediaInfo());
	ret = tmark.init_filter(task_mark);
	std::cout << "init filter " << ret <<std::endl;

	TaskApply task_enc(ApplyType::CREATE); 
	task_enc.mediaCtx = fileStream.getMediaInfo();
	task_enc.mediaCtx.video_bits = 500;
	task_enc.mediaCtx.audio_bits = 88;
	
	FFmpegEncoder enc(task_enc.mediaCtx);
	ret = enc.init_encoder("rtmp://119.3.245.104:1935/Preview7/Stream1", task_enc);

	fileStream.linkTo(&tmark);
	tmark.linkTo(&enc);

	fileStream.decoderStream();
	enc.startEncode();

	av_usleep(1000 * 1000 * 1000);
	fileStream.stopStream();
	enc.stopPush();

	return ret;
}



void testFade(){
	DecoderFile Net;
//	int ret = Net.init_decoder("http://192.168.2.215:12138/fish.m3u8");
	int ret = Net.init_decoder("http://192.168.2.215:12138/J.Fla-I_Don%27t_Care_split.flv");
	
	TaskApply task;
	FadeFilter fadefilter(Net.getMediaInfo());
	fadefilter.init_filter(task);


	TaskApply task_enc(ApplyType::CREATE); 
	task_enc.mediaCtx = Net.getMediaInfo();
	task_enc.mediaCtx.video_bits = 500;
	task_enc.mediaCtx.audio_bits = 88;

	FFmpegEncoder enc(task_enc.mediaCtx);
	enc.init_encoder("rtmp://192.168.2.34:1935/Preview7/Stream1", task_enc);
	Net.linkTo(&enc);


	Net.decoderStream();
	enc.startEncode();

//	av_usleep(40 * 1000 * 1000);

//	Net.linkTo(&fadefilter);
//	fadefilter.linkTo(&enc);

	for (int i(0); i < 10; i++) {
		av_usleep(1000 * 1000 * 1000);
	}
}
*/

int main(int argc, char** argv) {
//	av_log_set_level(AV_LOG_DEBUG);
//	testTimeMark();
//	return 0;
	
//	testFade();
//	return 0;
	if (argc <= 3) {
		std::cout << "param is too less\n";
		return -1;
	}
	std::cout << "Program Compiled "<<__TIME__ << "  "<< __DATE__<<std::endl;

	const Config *conf = Config::getConf(CONFPATH);
	if(	CreateDir(conf->GetCacheFilePath()) != 0){
		std::cout << "tmp cache file dir can't create "<< conf->GetCacheFilePath()<<std::endl;
		return -1;
	}
	
	std::string name = argv[1];
	if (name.find_last_of('/') != std::string::npos) {
		SerialFileName = name.c_str() + name.find_last_of('/') + 1;
		SerialFileName += ".ini";
	}
	else {
		SerialFileName = name;
	}
	std::cout << SerialFileName << std::endl;
	if (signal(SIGTERM, singnalAct) == SIG_ERR) {
		std::cout << "err with signal \n";
	}
	avformat_network_init();

	Broadcast* broad = new Broadcast;
	int ret = broad->initFifo(argc, argv);
	if (ret < 0) {
		std::cout << "Broadcast err\n";
		return ret;
	}
	broad->listen(std::ref(run));
	broad->stopListen();
	delete broad;
	Serial::stopSerialize();
	std::cout << "daobo is quit\n";
	return 0;
}
