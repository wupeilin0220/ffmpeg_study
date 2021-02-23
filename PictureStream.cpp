// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "PictureStream.h"



PictureStream::PictureStream():
	run(true), sendSDstream(true), sendHDstream(false),
	interval(5000),  processStatus(ProcessNone),
	streamsHandle(nullptr),
	linkHD(nullptr),
	imgOriginal(), imgSD(),imgHD(),dec_pic(nullptr)
{
	dec_pic = new DecoderPicture;
	dec_pic->linkTo(this);
	info.video_height = 180;
	info.video_width = 320;
	info.video_timebase = AVRational{ 1,AV_TIME_BASE };
	info.video_format = AV_PIX_FMT_YUV420P;
	info.video_bits = Config::getConf(CONFPATH)->GetVideoBits();
	info.fps = 25;
	info.hasAudio = false;
	info.hasVideo = true;


	mediaInfoHD.video_timebase = AVRational{ 1,AV_TIME_BASE };
	mediaInfoHD.video_format = AV_PIX_FMT_YUV420P;
	mediaInfoHD.fps = 25;
	mediaInfoHD.video_bits = 2000;
	type = StreamType::PicStream;
	mediaInfoHD.hasAudio = false;
	mediaInfoHD.hasVideo = true;

	type =StreamType::PicStream;
}


PictureStream::~PictureStream()
{
	delete dec_pic;
	stopStream();
	clearPicBuffer();	
}

std::vector<int> PictureStream::addPicture(std::vector<std::string>& imgSet)
{
	int i = -1;
	std::cout << "addpicture size "<< imgSet.size()<<std::endl;
	std::vector<int> fail;	
	for (auto val : imgSet) {
		i++;
		if (addOnePicture(val) < 0)
			fail.push_back(i);
	}
	std::cout << "fail size "<< fail.size()<<std::endl;
	return fail;
}

/* save frame and picture info */
int PictureStream::fillBuffer(DateSet date)
{
	
	std::cout << "get pic:\n"; 
	if (MEDIATYPE::VIDEO == date.type) {
		std::pair<AVFrame*, MuxContext> picinfo;
		picinfo.first = (AVFrame*)date.srcData;

		if (processStatus == ProcessNone) {		
			std::cout << "pic type ProcessNone\n"; 
		 
			picinfo.second = dec_pic->getMediaInfo();
			imgOriginal.push_back(picinfo);
		}
		else if (processStatus == ProcessPre) {
			std::cout << "pic type ProcessPre\n"; 
			picinfo.second = info;
			imgSD.push_back(picinfo);
		}
		else if (processStatus == ProcessFormal) {
			std::cout << "pic type ProcessFormal\n"; 
			picinfo.second = mediaInfoHD;
			imgHD.push_back(picinfo);
		}
		else
			return -1;
	}else
		return -1;

	return 0;
}

int PictureStream::decoderStream()
{
	std::cout << "image set size "<< imgOriginal.size()<<std::endl;
	processStatus = ProcessPre;
	std::cout << "decoderStream info  "<< info.video_width << info.video_height<<std::endl;
	reScale(info.video_width, info.video_height, imgOriginal);
	sendSDstream = true;
	if (!streamsHandle) {
		streamsHandle = new std::thread([&]() {this->pushStream(); });
	}
	return 0;
}

int PictureStream::clearPicBuffer()
{
	for (auto val : imgOriginal) {	
			av_frame_free(&(val.first));
			val.first = nullptr;
	}
	imgOriginal.clear();

	for (auto val : imgSD) {
			av_frame_free(&(val.first));
			val.first = nullptr;
	}
	imgSD.clear();


	for (auto val : imgHD) {
			av_frame_free(&(val.first));
			val.first = nullptr;
	}
	imgHD.clear();

	return 0;
}

int PictureStream::pushHDstream()
{
	for (auto val : imgHD) {
		av_frame_free(&(val.first));
		val.first = nullptr;
	}
	imgHD.clear();
	processStatus = ProcessFormal;
	reScale(mediaInfoHD.video_width, mediaInfoHD.video_height, imgOriginal);
	sendHDstream = true;
	return 0;
}

int PictureStream::addOnePicture(std::string name)
{
	dec_pic->uninit_decoder();
	if (dec_pic->init_decoder(name) >= 0) {
		if (dec_pic->decoderStream() >= 0) {
			return 0;
		}
	}	
	dec_pic->uninit_decoder();
	return -1;
}

int PictureStream::removePicture(std::string )
{
	return -1;
}

int PictureStream::setSDProperty(int width, int height)
{
	if (!sendSDstream)
		return -1;

	if (width > 0 && width <= 1920 && width % 2 == 0
		&& height > 0 && height <= 1920 && height % 2 == 0) {
		info.video_width = width;
		info.video_height = height;
		std::cout << "set PictureStream::setSDProperty width "<< width << " height "<< height<<std::endl; 
		return 0;
	}
	return -1;
}

int PictureStream::setHDProperty(int width, int height)
{
	if (width > 0 && width <= 1920 && width % 2 == 0
		&& height > 0 && height <= 1920 && height % 2 == 0) {
		mediaInfoHD.video_width = width;
		mediaInfoHD.video_height = height;
		return 0;
	}
	return -1;
}

int PictureStream::stopStream()
{
	run = false;
	if (streamsHandle) {
		if (streamsHandle->joinable()) {
			streamsHandle->join();
			delete streamsHandle;
			streamsHandle = nullptr;
		}
	}
	return 0;
}


int PictureStream::pushStream()
{
	//控制发送速度 fps = 25
	int index = 0;
	int size = imgOriginal.size();
	DataSet data;
	data.type = MEDIATYPE::VIDEO;
	int64_t decTime = Decoder::getDecodeTime();
	int64_t startTime = 0;
	int64_t lastTime = av_gettime();
	int64_t fpts = 0;
	int64_t offsetTime = av_gettime() - decTime;
	while (run) {		
		for (; index < size; index++) {
			startTime = av_gettime();
			while (run) {				
				int64_t now = av_gettime();
				if ((now - lastTime) < 40 * 1000) {
					av_usleep(40 * 1000 - (now - lastTime));
				}

				lastTime = av_gettime();
				fpts++;
				int64_t pts = av_rescale_q(fpts, AVRational{ 1,25 }, AVRational{ 1,AV_TIME_BASE })+ offsetTime;
				if (link && sendSDstream) {
					AVFrame* frame = av_frame_alloc();
					frame->width = imgSD[index].first->width;
					frame->height = imgSD[index].first->height;
					frame->format = imgSD[index].first->format;
					av_frame_get_buffer(frame, 0);

					av_frame_copy(frame, imgSD[index].first);

					data.srcData = frame;
					frame->pts = frame->pkt_dts = pts;
			
					if (link->fillBuffer(data) < 0) {
						av_frame_free(&frame);
					}
				}

				mutex_hd.lock();
				if (linkHD && sendHDstream) {
					AVFrame* frame = av_frame_alloc();
					frame->width = imgHD[index].first->width;
					frame->height = imgHD[index].first->height;
					frame->format = imgHD[index].first->format;
					av_frame_get_buffer(frame, 0);

					av_frame_copy(frame, imgHD[index].first);
					data.srcData = frame;
					frame->pts = frame->pkt_dts = pts;
					if (linkHD->fillBuffer(data) == 1) {
						av_frame_free(&frame);
					}
				}
        mutex_hd.unlock();
				if ((av_gettime() - startTime) > ((int64_t)(interval) * 1000 )) {
//					std::cout << "time " << av_gettime() - startTime << "count "<< fpts << std::endl;
					break;
				}
				
			}
			if (!run)
				break;

			if (index == (size - 1))
				index = -1;
		}
		
		
	}
	return 0;
}

int PictureStream::reScale(int width, int height, std::vector<FRAMEATTR>& image)
{
	DataSet data;
	data.type = MEDIATYPE::VIDEO;
	
	for (auto val : image) {

		AspectRatio aspect(val.second);
		TaskApply task(ApplyType::ADAPT);

		task.mediaCtx = dec_pic->getMediaInfo();
		task.mediaCtx.video_width = width;
		task.mediaCtx.video_height = height;
		task.mediaCtx.fps = 0;
		task.adstyle = AdaptStyle::Fill;

		aspect.linkTo(this);
		aspect.init_filter(task);
		
		AVFrame* frame = av_frame_alloc();
		frame->width = val.first->width;
		frame->height = val.first->height;
		frame->format = val.first->format;
		av_frame_get_buffer(frame, 0);

		av_frame_copy(frame, val.first);
		data.srcData = frame;

		aspect.fillBuffer(data);
	}
	
	return 0;
}
