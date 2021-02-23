#pragma once
#include <string>
#include <iostream>
#include "MediaBase.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/channel_layout.h>
};




class Decoder: public Linker
{
public:
	StreamType				type;

	Decoder();
	virtual ~Decoder();
	
	/*	if return < 0 you should check	by getMediaInfo()	*/
	virtual int init_decoder(const std::string& url,int retry = 2);
	void uninit_decoder();
	
	const MuxContext& getMediaInfo() { return info; }	
	virtual int  decoderStream() = 0;
	virtual int	 stopStream() = 0;
	static int64_t getDecodeTime() {
		static int64_t time = av_gettime();
		return time;
	}
	double getVideoNow() {return nowVideoPts;}
	virtual int seekPos(int64_t position) = 0;
	std::string getInputUrl(){return inputUrl;}
protected:
	virtual void setOpenOption(void);			
	virtual	void setInterruptCallback() {}   // ffmpeg interrupt callback
	virtual void generateMediainfo();		
	

	virtual int init_videoDec();
	virtual int init_audioDec();
	double nowVideoPts;	
	
protected:
	AVFormatContext*	formatContext;
	AVCodecContext*		video_codecCtx;
	AVCodecContext*		audio_codecCtx;
	AVDictionary*		dict;

	int						video_index;
	int						audio_index;
	MuxContext				info;
	int64_t					start_time;		// record ffmpeg IO require time for caculate timeout
	int64_t					dec_time;		// start decode time
	std::string				inputUrl;
	
private:
	
};

