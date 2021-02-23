#include "split.h"

split::split(const MuxContext& attr):
	filterBase(attr),
	linkHD(nullptr),linkSD(nullptr),
	vbufferSink_CtxSplit(nullptr),abufferSink_CtxSplit(nullptr),
	sendAudio(false), sendVideo(false), outMedia(attr)
{
	
	outMedia.video_format = AV_PIX_FMT_YUV420P;

	
	int des_height = 180;//PREVIEW_RATIO * mediaInfo.video_height;
	int des_width = 320;//PREVIEW_RATIO * mediaInfo.video_width;
	const Config *conf = Config::getConf(CONFPATH);
	double ratio = conf->GetPreviewRation();
	if(mediaInfo.video_height >= 720 || mediaInfo.video_width >= 720){
		des_height = ratio * mediaInfo.video_height;
		des_width = ratio * mediaInfo.video_width;				
	}else{
		des_height = 0.5 * mediaInfo.video_height;
		des_width = 0.5 * mediaInfo.video_width;				

	}

	if(des_height % 2)
		des_height +=1;
	if(des_width % 2)
		des_width +=1;
	outMedia.video_height  = des_height;
	outMedia.video_width = des_width;

	outMedia.hasAudio = attr.hasAudio;
	outMedia.hasVideo = attr.hasVideo;
	std::cout << "split height "<< outMedia.video_height<< "width "<< outMedia.video_width <<std::endl;
}

split::~split(){
	if(vbufferSink_CtxSplit){
		avfilter_free(vbufferSink_CtxSplit);
		vbufferSink_CtxSplit = nullptr;
	}

	if(abufferSink_CtxSplit){
		avfilter_free(abufferSink_CtxSplit);
		abufferSink_CtxSplit = nullptr;
	}
	std::cout << "filter split distruct_______________\n";
}

int split::fillBuffer(DataSet p){
	if(p.type == MEDIATYPE::VIDEO){
		return pushStream(p,vbuffer_Ctx,vbufferSink_Ctx,vbufferSink_CtxSplit,MEDIATYPE::VIDEO,sendVideo);
	}else{
		return pushStream(p,abuffer_Ctx,abufferSink_Ctx,abufferSink_CtxSplit,MEDIATYPE::AUDIO,sendAudio);
	}
	return -1;
}


int split::init_videoEffect(TaskApply&){
	if(avfilter_graph_create_filter(&vbufferSink_CtxSplit,avfilter_get_by_name("buffersink"),"voutSp",NULL,NULL,video_graph)<0){
		std::cout << "can't create split video filter\n";
		return -1;
	}

	char args[128]={};
	snprintf(args,128,"[vin0]split=[a][voutSp];[a]scale=%d:%d,format=pix_fmts=yuv420p[vout0]", outMedia.video_width, outMedia.video_height);
	AVFilterInOut* in_buffer = avfilter_inout_alloc();
	AVFilterInOut* out_Formal = avfilter_inout_alloc();
	AVFilterInOut* out_Preview = avfilter_inout_alloc();
	
	std::cout << "split______________________ "<< args<<std::endl;
	in_buffer->name = av_strdup("vin0");
	in_buffer->filter_ctx = vbuffer_Ctx;
	in_buffer->next = NULL;
	in_buffer->pad_idx = 0;

	out_Preview->name = av_strdup("vout0");
	out_Preview->filter_ctx = vbufferSink_Ctx;
	out_Preview->next = out_Formal;
	out_Preview->pad_idx = 0;


	out_Formal->name = av_strdup("voutSp");
	out_Formal->filter_ctx = vbufferSink_CtxSplit;
	out_Formal->next = NULL;
	out_Formal->pad_idx = 0;

	
	int ret = avfilter_graph_parse_ptr(video_graph, args,&out_Preview,&in_buffer, NULL);
	if(ret < 0){
		std::cout << "err video graph_parse_ptr "<<ret <<std::endl;
		goto end;
	}

	ret = avfilter_graph_config(video_graph, NULL);
	if(ret < 0){
		std::cout << "check graph err "<<ret <<std::endl;
	}
end:
	avfilter_inout_free(&in_buffer);
	avfilter_inout_free(&out_Preview);
	return ret;

}

int split::init_audioEffect(TaskApply&){
	
	if(avfilter_graph_create_filter(&abufferSink_CtxSplit,avfilter_get_by_name("abuffersink"),"aoutSp",NULL,NULL,audio_graph)<0){
		std::cout << "can't create split audio filter\n";
		return -1;
	}

	char args[128]={};
	snprintf(args,128,"[ain0]asplit=2[aout0][aoutSp]");
	AVFilterInOut* in_buffer = avfilter_inout_alloc();
	AVFilterInOut* out_Formal = avfilter_inout_alloc();
	AVFilterInOut* out_Preview = avfilter_inout_alloc();

	in_buffer->name = av_strdup("ain0");
	in_buffer->filter_ctx = abuffer_Ctx;
	in_buffer->next = NULL;
	in_buffer->pad_idx = 0;

	out_Preview->name = av_strdup("aout0");
	out_Preview->filter_ctx = abufferSink_Ctx;
	out_Preview->next = out_Formal;
	out_Preview->pad_idx = 0;


	out_Formal->name = av_strdup("aoutSp");
	out_Formal->filter_ctx = abufferSink_CtxSplit;
	out_Formal->next = NULL;
	out_Formal->pad_idx = 0;

	
	int ret = avfilter_graph_parse_ptr(audio_graph, args, &out_Preview, &in_buffer, NULL);
	if(ret < 0){
		std::cout << "err video graph_parse_ptr "<<ret <<std::endl;
		goto end;
	}

	ret = avfilter_graph_config(audio_graph, NULL);
	if(ret < 0){
		std::cout << "check graph err "<<ret <<std::endl;
	}
end:
	avfilter_inout_free(&in_buffer);
	avfilter_inout_free(&out_Preview);
	return ret;
}

int split::init_effectfilter(TaskApply& task_){
	int ret = 0;
	std::cout << "split src hasviceo "<< mediaInfo.hasVideo<<" hasAudio " <<mediaInfo.hasAudio<<std::endl;
	if (mediaInfo.hasVideo) {
		ret = init_videoEffect(task_);
		if (ret < 0) {
			return ret;
		}
	}
	if (mediaInfo.hasAudio) {
		ret = init_audioEffect(task_);
	
	}
	return ret;
}
	
int split::pushStream(DataSet& p,AVFilterContext *in,AVFilterContext* out_pre,AVFilterContext* out_from,MEDIATYPE type, bool push){

	int ret = 0;
	AVFrame* frame = (AVFrame*)(p.srcData);
	ret = av_buffersrc_add_frame_flags(in, frame, 0);
	if(ret < 0){
		return -1;	
	}

	while(1){
		AVFrame *frame_sd = av_frame_alloc();
		ret = av_buffersink_get_frame(out_pre, frame_sd);
		if(ret < 0){
			av_frame_free(&frame);
			av_frame_free(&frame_sd);
			if (AVERROR(EAGAIN) == ret){
					break;
			}
			else if (AVERROR_EOF == ret) {
					break;
			}
			
			return ret;
		}

		DataSet data;
		data.type = type;
		data.srcData = frame_sd;
		if(linkSD){
			if(linkSD ->fillBuffer(data) < 0){
				av_frame_free(&frame_sd);	
			}
		}else{
			av_frame_free(&frame_sd);	
		}
	}

	while(1){
		AVFrame *frame_hd = av_frame_alloc();
		ret = av_buffersink_get_frame(out_from, frame_hd);
		if(ret < 0){
			av_frame_free(&frame_hd);
			if (AVERROR(EAGAIN) == ret){
					break;
			}
			else if (AVERROR_EOF == ret) {
				std::cout<< "split get AVERROR_EOF\n"; 
				break;
			}else 
				return ret;
		}
		DataSet data;
		data.type = type;
		data.srcData = frame_hd;

		mutex_hd.lock();
		if(linkHD && push){
			if(linkHD ->fillBuffer(data) < 0)
			{
				av_frame_free(&frame_hd);
			}
		}else{
			av_frame_free(&frame_hd);	
		}	
		mutex_hd.unlock();
	}
	av_frame_free(&frame);
	return 0;	
}
