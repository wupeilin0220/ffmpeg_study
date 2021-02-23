#pragma once
#include <string>
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "MediaBase.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
};


#include "DecoderFile.h"
#include "PictureStream.h"
#include "AspectRatio.h"
#include "AudioResample.h"
// encoder need flush
typedef std::function<int(int,int)>  StatusCall;

class FFmpegEncoder:public Linker,private Serial
{
	std::queue<DateSet>					avframeData;
	std::mutex							mutex_avframe;

	std::queue<AVPacket*>				avpackData_v;
	std::mutex							mutex_avpack_v;

	std::queue<AVPacket*>				avpackData_a;
	std::mutex							mutex_avpack_a;

	std::queue<AVPacket*>				avpackData_a_Sos;
	std::mutex							mutex_avpack_aSos;

	std::queue<AVPacket*>				avpackData_v_Sos;
	std::mutex							mutex_avpack_vSos;

	std::queue<DateSet>					avframeData_Sos;
	std::mutex							mutex_avframe_Sos;

public:

	FFmpegEncoder(const MuxContext& media);
	
	virtual ~FFmpegEncoder();
	int init_encoder(std::string url,TaskApply& task);
	int uninit_encoder();
	virtual int fillBuffer(DateSet date) override;
	int	startEncode();  
	int stopPush();
	int clearFramebuffer();
	int clearPacketbuffer();
	virtual Link* getNext() override { return nullptr; };
	void setObserve(StatusCall fun){funCallback = fun;}
	std::string GetOutputUrl(){return outputUrl;}
public:
	int addEmergencyStream(std::string url);
	int addEmergencyStream(std::vector<std::string> url,int64_t interval);
	int removeEmergencyStream();
	int switchEmergencyStream();
	int swicthNormalStream();
	void setSendStreamtype(bool hasVideo,bool hasAudio){sendVideo = hasVideo;sendAudio = hasAudio;}
	
public:
//	void flushEncode(){	needFlush_v = true; needFlush_a  = true;} 
	int streamBroken(bool broken,MEDIATYPE type);

protected:
	int init_encoder_video(TaskApply& task);
	int init_encoder_audio(TaskApply& task);
	int testAddress(std::string& url,AVFormatContext *outFmt);

	int StreamPush();
	int SendData(int64_t tbs,AVStream *stream,std::queue<AVPacket*>& QueuePack,std::mutex& mux,int64_t &packPts,int64_t& lastT);
	int dropPacket(int64_t tbs,AVStream *stream,std::queue<AVPacket*>& QueuePack,std::mutex& mux);
	int StreamDelay(int64_t delayMs);
protected:
	int init_Emergency();
	int remove_Emergency();
	int reinit_Emergency();
	void stopEncodeEmergency();
	void startEncodeEmergency();
private:
	int encodeVideo(AVFrame* frame);
	int encodeAudio(AVFrame* frame);
	
	int encodeVideo_Sos(AVFrame* frame);
	int encodeAudio_Sos(AVFrame* frame);

	int init_sosencoder_video(TaskApply& task);
	int init_sosencoder_audio(TaskApply& task);

protected:
	int exec();
	int emergencyEncode();
	void replacePacket(AVPacket **pack_replace, std::queue<AVPacket *>& dataQue, std::mutex& mutexQue);
	int syncPacket();
	int streamInfoCheck(int64_t vTime,int64_t aTime,int& lastStatus);
	void StreamStatusMonitor(int& netStatus,int64_t& videoT,int64_t& audioT);
	int64_t emcPacketCorrect(int64_t baseTime);
protected:
	virtual void serialize() override;
private:
	AVFormatContext*	outFmtCtx;
	AVCodecContext*		video_codecCtx;
	AVCodecContext*		audio_codecCtx;

	

	int					video_index;
	int					audio_index;
	std::thread*		hand_encode;
	std::thread*		hand_pushStream;
	volatile bool		run;
	const MuxContext	srcInfo;
	std::string 		outputUrl;
	TaskApply			taskEnc;
	int64_t				frameCount;
	int64_t				timeCount;

	//emergency stream
	volatile bool		sos;
	volatile bool		sos_getPacket;
	volatile bool		sendVideo;  // for free emergency data ,delete after
	volatile bool		sendAudio;
	int64_t				offset_v;    // if has delay time, src frame's pts - offset_x  
	int64_t				offset_a;    

	std::condition_variable cv;  	
	std::mutex			cv_mutex;
	bool				getPacket;


	Decoder*		decSos;
	AVCodecContext*		video_codecCtx_Emergency;
	AVCodecContext*		audio_codecCtx_Emergency;
	std::thread*		handEmergency_encode;
	std::atomic<int64_t>		sos_videoOffset;
	std::atomic<int64_t>		sos_audioOffset;
//	bool				key_flag_sos;
//	bool				key_flag_normal;
private:
	class EmergencyLink :public Linker {
	public:
		EmergencyLink(FFmpegEncoder* user):enc(user){
				
		}
		EmergencyLink(const EmergencyLink& _Right)
		{
			if(&_Right != this){
				enc = _Right.enc;
			}	
		}

		EmergencyLink& operator=(const EmergencyLink& _Right){
			if(&_Right != this){
				enc = _Right.enc;
			}	
			return *this;	
		}

		virtual ~EmergencyLink() {}
		virtual int fillBuffer(DataSet p) override;
	private:
		FFmpegEncoder* enc;
	};

	EmergencyLink* emclink;
	volatile bool	needDrop_v;	
	volatile bool	needDrop_a;	
	volatile bool	ssBroken;
	int64_t			absTime;
	int64_t			streamBrokenTime;

	StatusCall	funCallback;
	bool		sendtime;
};

