#pragma once
/*  �������ͼƬ����	*/

#include <vector>
#include <map>
#include "Decoder.h"
extern "C" {
#include <libswscale/swscale.h>
};

//   DataLinker  ͳһ����������ӿڷ�װ

class  DecoderPicture final :public Decoder
{
public:
	DecoderPicture();
	virtual ~DecoderPicture();
	virtual int decoderStream() override;
	virtual int	 stopStream() override { return 0; }
	virtual int fillBuffer(DataSet) override { return -1; }
	virtual int seekPos(int64_t ) override{return -1;}
protected:
	virtual int init_audioDec() override { return 0; } // don't need init audio codec
};
