#include "TimeMark.h"
#include "Decoder.h"

TimeMark::TimeMark(const MuxContext& attr):
	filterBase(attr)
{
	textCtx = nullptr;
}

TimeMark::~TimeMark(){
	
}

int TimeMark::fillBuffer(DataSet p){
	if(!link)
		return -1;
	
	if(p.type == MEDIATYPE::AUDIO){
		return link->fillBuffer(p);
	}		
	
	return pushVideo(p);
}

int TimeMark::pushVideo(DataSet& p){
	AVFrame* frame_src = (AVFrame*)p.srcData;
	int ret = av_buffersrc_add_frame_flags(vbuffer_Ctx, frame_src, 0/*AV_BUFFERSRC_FLAG_KEEP_REF*/);
	if (ret < 0) {
		av_frame_free(&frame_src);
		return ret;
	}
	

	while (true) {
		AVFrame* frame = av_frame_alloc();
		ret = av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
		if (ret < 0 ) {
			av_frame_free(&frame);
			av_frame_free(&frame_src);
			if (ret == AVERROR(EAGAIN))
				return 0;
			else if (ret == AVERROR_EOF) {
				DataSet data;
				data.type = MEDIATYPE::VIDEO;
				data.srcData = nullptr;
				if (link) {
					link->fillBuffer(data);
				}			
			}else
				av_frame_free(&frame);
			return ret;
		}
		DataSet data;
		data.type = MEDIATYPE::VIDEO;
		data.srcData = frame;
		if (link) {
			if(link->fillBuffer(data) < 0){
				av_frame_free(&frame);		
			}
		}else{
			av_frame_free(&frame);	
		}

	}
	av_frame_free(&frame_src);
	return 0;
}

char *TimeMark::convertFontfileName(const std::string&  unicode,char *utf){
	if(unicode.empty())
		return nullptr;

	if(unicode == "\u9ed1\u4f53"){		//	simhei
		snprintf(utf, 64, "%s", "./fonts/simhei.ttf");
	}
	else if(unicode == "\u5b8b\u4f53"){		//	simsun
		snprintf(utf, 64, "%s", "./fonts/simsun.ttc");
	}
	else if(unicode == "\u7ad9\u9177\u9ad8\u7aef\u9ed1\u4f53"){	//	zk-gdhei
		snprintf(utf, 64, "%s", "./fonts/zk-gdhei.ttf");
	}
	else if(unicode == "\u7ad9\u9177\u9177\u9ed1\u4f53"){		//	zk-khei
		snprintf(utf, 64, "%s", "./fonts/zk-khei.ttf");
	}
	else if(unicode == "\u7ad9\u9177\u5feb\u4e50\u4f53"){		//	zk-kl
		snprintf(utf, 64, "%s", "./fonts/zk-kl.ttf");
	}
	else if(unicode == "\u7ad9\u9177\u5e86\u79d1\u9ec4\u6cb9\u4f53"){	//	zk-qkhy
		snprintf(utf, 64, "%s", "./fonts/zk-qkhy.ttf");
	}
	else if(unicode == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u5e38\u89c4"){	//	¿¿¿¿¿¿¿¿?al-cg
		snprintf(utf, 64, "%s", "./fonts/al-cg.ttf");
	}
	else if(unicode == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7c97"){	//	¿¿¿¿¿¿¿¿ al-cu
		snprintf(utf, 64, "%s", "./fonts/al-cu.ttf");
	}
	else if(unicode == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7279\u7c97"){	//	¿¿¿¿¿¿¿¿?al-tecu
		snprintf(utf, 64, "%s", "./fonts/al-tecu.ttf");
	}
	else if(unicode == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7ec6"){	//	¿¿¿¿¿¿¿¿ al-xi
		snprintf(utf, 64, "%s", "./fonts/al-xi.ttf");
	}
	else if(unicode == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u4e2d\u7b49"){	//	¿¿¿¿¿¿¿¿?al-zhong
		snprintf(utf, 64, "%s", "./fonts/al-zhong.ttf");
	}
	else{
		std::cout << "can't find front type\n";
	}
	return utf;
}

int TimeMark::init_effectfilter(TaskApply& task_){
	int64_t  pts = Decoder::getDecodeTime();
	if(!mediaInfo.hasVideo)
		return -1;
	
	char param[1024]={};
	char utf[64]={};
	auto& textInfo = task_.text;
	if(convertFontfileName(std::string(textInfo.font),utf) == nullptr)
		return -1;

	if(textInfo.delay == 0){
		snprintf(param,1023,"fontfile='%s':fontsize=%d:x=%lf:y=%lf:fontcolor=%s@%lf:text='%%{localtime:\\%%H\\:%%M\\:%%S}'",
				utf,textInfo.fontSize,textInfo.x,textInfo.y,textInfo.color,textInfo.fontAlpha);
	}else{
		int64_t tmppts =  pts + textInfo.delay*1000;
		snprintf(param,1023,"expansion=strftime:basetime=%ld:fontfile='%s':fontsize=%d:x=%lf:y=%lf:fontcolor=%s@%lf:text='%%H:%%M:%%S'",
	//	snprintf(param,1023,"expansion=strftime:basetime=1514736000000000:fontfile='%s':fontsize=%d:x=%lf:y=%lf:fontcolor=%s@%lf:text='%%Y:%%m:%%d  %%H:%%M:%%S'",
				tmppts,utf,textInfo.fontSize,textInfo.x,textInfo.y,textInfo.color,textInfo.fontAlpha);
	}

	std::cout << "time mark param "<< param<<std::endl;
	const AVFilter* ft = avfilter_get_by_name("drawtext");
	int ret = avfilter_graph_create_filter(&textCtx,ft,"drawtime",param,NULL,video_graph);
	if(ret < 0 && avfilter_graph_create_filter(&textCtx,ft,"drawtime",param,NULL,video_graph) < 0){
		std::cout << "TimeMark:: create drawtext filter err "<< ret <<std::endl;
		return -1;
	}
	

	ret = avfilter_link(vbuffer_Ctx, 0, textCtx, 0);
	if (ret < 0) {
		return ret;
	}
	// link
	ret = avfilter_link(textCtx, 0, vbufferSink_Ctx, 0);
	if (ret < 0) {
		return ret;
	}
	ret = avfilter_graph_config(video_graph, NULL);
	if (ret < 0) {
		std::cout << "drawtext filter is failed\n";
		return ret;
	}

	ret = avfilter_graph_config(video_graph, NULL);
	std::cout << "avfilter_graph_config "<< ret <<std::endl;
	return ret;
}




