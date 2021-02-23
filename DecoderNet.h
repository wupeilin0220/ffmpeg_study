#pragma once
#include "Decoder.h"


constexpr int Retry = 1;
// test rtmp://58.200.131.2:1935/livetv/hunantv
class DecoderNet:public Decoder
{
public:
	DecoderNet();
	virtual ~DecoderNet();

	virtual int fillBuffer(DataSet) override { return -1; }
	virtual int decoderStream() override;
	virtual int stopStream() override;
	bool IsBroken() { return streamBroken; }
	int reConnected();
	virtual int seekPos(int64_t ) override {return 0;}
	void setStreamBroken(StreamBreak& fb){brokenFun = fb;}
protected:

	virtual	void setInterruptCallback() override;   // ffmpeg interrupt callback
	int exec();
	int64_t framePtsCorrection(AVFrame* frame,MEDIATYPE type,int64_t lastPts);
private:
	void sendBrokenFlag();

private:
	volatile bool  run;
	volatile bool  streamBroken;
	std::thread*   handle_decode;

	// 控制pts的偏移问题
	bool firstFrame_v ; 
	bool firstFrame_a;
	int64_t start_vpts ; 
	int64_t start_apts ;
	int64_t offset_v ;
	int64_t offset_a ;
	std::mutex	mutex_handle;
	StreamBreak	brokenFun;

	int64_t last_correct_vt;
	int64_t last_correct_at;

};



