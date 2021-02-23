#pragma once
#include "DecoderPicture.h"
#include "AspectRatio.h"
#include "Decoder.h"

class PictureStream:public Decoder
{
public:
	PictureStream();
	virtual ~PictureStream();
	std::vector<int> addPicture(std::vector<std::string>& imgSet);
	virtual int fillBuffer(DateSet date) override ;
	virtual int decoderStream() override;
	virtual int init_decoder(const std::string& ,int)override { return -1; }
	virtual int	 stopStream() final;

	int clearPicBuffer();
	int pushHDstream();
	
	int removePicture(std::string name);

	// 修改时需停止输出
	void linkToHD(Link* link) {  // 输出 
		mutex_hd.lock();
		linkHD = link; 
		mutex_hd.unlock();
	} 
	Link* getHDLink() { return linkHD; }


	int setSDProperty(int width = 320, int height = 180);
	int setHDProperty(int width, int height);
	void setImgInterval(int64_t intervalSec) { interval = (intervalSec <= 0) ? interval : intervalSec; }

	const MuxContext& getHDMediaInfo() { return mediaInfoHD; }

	void stopPushHDStream() { sendSDstream = false; }	
	virtual int seekPos(int64_t ) override{ return 0;}

protected:
	int addOnePicture(std::string name);
	virtual  void generateMediainfo() override {}//for SD mediainfo
	int pushStream();
	
	typedef std::pair<AVFrame*, MuxContext>		FRAMEATTR;
	enum DataProperty
	{
		ProcessNone = 0,
		ProcessPre = 1,
		ProcessFormal
	};

	int reScale(int width,int height,std::vector<FRAMEATTR> &image);
private:
	volatile bool			run;
	bool					sendSDstream;
	bool					sendHDstream;
	int64_t					interval;

	DataProperty			processStatus;
	std::thread*			streamsHandle;
	std::mutex				mutex_hd;


	Link* linkHD;
	
	
	MuxContext				mediaInfoHD;
//	MuxContext				mediaInfoSD;

	std::vector<FRAMEATTR>  imgOriginal;
	std::vector<FRAMEATTR>  imgSD;
	std::vector<FRAMEATTR>  imgHD;
	DecoderPicture*			dec_pic;
};

