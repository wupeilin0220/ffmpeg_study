#pragma once
#include "Decoder.h"
class DecoderFile :public Decoder
{
public:
	DecoderFile();
	virtual ~DecoderFile();

	virtual int fillBuffer(DataSet) override { return -1; }
	virtual int decoderStream() override;
	virtual int stopStream() override;
	void setLoop(const bool loop = true) { Isloop = loop; }

	// position ms
	virtual int seekPos(int64_t position) override ;
protected:
	int exec();
	int pushStream(MEDIATYPE type, AVFrame* frame);
	int reSetDecode(int64_t& offset_v,int64_t& offset_a);
	
private:
	int SeekCheck(AVFrame* frame,MEDIATYPE type,int64_t time);
private:
	std::thread* handle_decode;
	bool Isloop;
	bool run;
	bool seekFlag;
	int64_t offset;
	
};

