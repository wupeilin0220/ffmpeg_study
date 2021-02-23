CC = g++
LINK = $(CC)
CFLAGS = -std=c++11 -W -Wall -O2 -g
INCLS = -I /usr/local/ffmpeg/include/ -I /usr/local/x264/include -I /usr/include/
LIBS = -L /usr/local/ffmpeg/lib/ -L /usr/local/x264/lib -L /usr/lib64/
PROJECT = daobo
$(PROJECT): daobo.o\
			Decoder.o DecoderNet.o DecoderFile.o DecoderPicture.o PictureStream.o\
			filter.o AspectRatio.o logoFilter.o DrawText.o split.o AudioResample.o Mosaic.o TimeMark.o FadeFilter.o\
			FilterMgr.o FFmpegEncoder.o NetStreamListen.o VMix.o AMix.o StreamMix.o\
			Broadcast.o JsonParse.o TaskBroadcast.o\
			ResourceCreater.o JsonConf.o Serial.o TaskApply.o 
			$(LINK) -o $(PROJECT) $(LIBS) \
	daobo.o Decoder.o DecoderNet.o DecoderFile.o DecoderPicture.o PictureStream.o filter.o  AspectRatio.o  FadeFilter.o \
	logoFilter.o DrawText.o split.o AudioResample.o Mosaic.o TimeMark.o TaskBroadcast.o FFmpegEncoder.o Broadcast.o JsonParse.o \
	ResourceCreater.o FilterMgr.o NetStreamListen.o JsonConf.o VMix.o AMix.o StreamMix.o Serial.o  TaskApply.o \
	-lpthread -lavformat -lavcodec -lavfilter -lavutil -lswscale -lswresample -luuid -lcurl -lpcrecpp

daobo.o:daobo.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o daobo.o daobo.cpp
FadeFilter.o:FadeFilter.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o FadeFilter.o FadeFilter.cpp
Decoder.o:Decoder.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o Decoder.o Decoder.cpp
DecoderNet.o:DecoderNet.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o DecoderNet.o DecoderNet.cpp
DecoderFile.o:DecoderFile.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o DecoderFile.o DecoderFile.cpp
DecoderPicture.o:DecoderPicture.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o DecoderPicture.o DecoderPicture.cpp
PictureStream.o:PictureStream.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o PictureStream.o PictureStream.cpp
filter.o:filter.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o filter.o filter.cpp
AspectRatio.o:AspectRatio.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o AspectRatio.o AspectRatio.cpp
logoFilter.o:logoFilter.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o logoFilter.o logoFilter.cpp
DrawText.o:DrawText.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o DrawText.o DrawText.cpp
split.o:split.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o split.o split.cpp
AudioResample.o:AudioResample.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o AudioResample.o AudioResample.cpp
FFmpegEncoder.o:FFmpegEncoder.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o FFmpegEncoder.o FFmpegEncoder.cpp
Broadcast.o:Broadcast.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o Broadcast.o Broadcast.cpp
JsonParse.o:JsonParse.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o JsonParse.o JsonParse.cpp
TaskBroadcast.o:TaskBroadcast.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o TaskBroadcast.o TaskBroadcast.cpp
ResourceCreater.o:ResourceCreater.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o ResourceCreater.o ResourceCreater.cpp
NetStreamListen.o:NetStreamListen.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o NetStreamListen.o NetStreamListen.cpp
FilterMgr.o:FilterMgr.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o FilterMgr.o FilterMgr.cpp
StreamMix.o:StreamMix.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o StreamMix.o StreamMix.cpp
VMix.o:VMix.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o VMix.o VMix.cpp
AMix.o:AMix.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o AMix.o AMix.cpp
JsonConf.o:JsonConf.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o JsonConf.o JsonConf.cpp
Serial.o:Serial.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o Serial.o Serial.cpp
Mosaic.o:Mosaic.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o Mosaic.o Mosaic.cpp
TaskApply.o:TaskApply.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o TaskApply.o TaskApply.cpp
TimeMark.o:TimeMark.cpp
	$(CC) -c $(CFLAGS) $(INCLS) -o TimeMark.o TimeMark.cpp

clean:
	rm -rf *.o
	rm -rf daobo
