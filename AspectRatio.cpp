#include "AspectRatio.h"



AspectRatio::AspectRatio(const MuxContext& attr):filterBase(attr),
	needAdapt(false)
{

}


AspectRatio::~AspectRatio()
{
	std::cout << "_______AspectRatio distruct______\n";
}

int AspectRatio::fillBuffer(DataSet p)
{
	if (p.type == MEDIATYPE::VIDEO) {
		if (needAdapt)
			return pushVideo(p);
		else {
			if (link)
				return link->fillBuffer(p);
		}
			
	}
	else {
		if (link) {
			link->fillBuffer(p);
		}
	}
	return 0;
}

int AspectRatio::init_effectfilter(TaskApply& task_)
{
	if (!mediaInfo.hasVideo) {
		std::cout << "AspectRatio: use not contain video info source_________\n";
		return -1;
	}

	if (task_.getType() != ApplyType::ADAPT || task_.mediaCtx.video_width <= 0 || task_.mediaCtx.video_height <= 0)
		return -1;

	int dst_w = 0,dst_h = 0;


		needAdapt = true;
		Adapt(task_, dst_w, dst_h);
//	}

	
	char args[1024] = {};
	if (task_.adstyle == AdaptStyle::Fill) {
		int offset_w = (task_.mediaCtx.video_width - dst_w) / 2;
		int offset_h = (task_.mediaCtx.video_height - dst_h) / 2;
		if (offset_w % 2)
			offset_w += 1;
		if (offset_h % 2)
			offset_h += 1;
		if (task_.mediaCtx.fps > 0) {
				snprintf(args, 1024, "[vin0]scale=%d:%d,pad=%d:%d:%d:%d,format=pix_fmts=yuv420p,fps=fps=25[vout0]",
					dst_w, dst_h, task_.mediaCtx.video_width, task_.mediaCtx.video_height, offset_w, offset_h);
		}
		else {
				snprintf(args, 1024, "[vin0]scale=%d:%d,pad=%d:%d:%d:%d,format=pix_fmts=yuv420p[vout0]",
					dst_w, dst_h, task_.mediaCtx.video_width, task_.mediaCtx.video_height, offset_w, offset_h);
		}
	}
	else {
			if (task_.mediaCtx.fps > 0) {
				snprintf(args, 1024, "[vin0]scale=%d:%d,crop=%d:%d:%d:%d,format=pix_fmts=yuv420p,fps=fps=25[vout0]",
					dst_w, dst_h, task_.mediaCtx.video_width, task_.mediaCtx.video_height, (dst_w - task_.mediaCtx.video_width) / 2, (dst_h - task_.mediaCtx.video_height) / 2);
			}
			else {
				snprintf(args, 1024, "[vin0]scale=%d:%d,crop=%d:%d:%d:%d,format=pix_fmts=yuv420p[vout0]",
					dst_w, dst_h, task_.mediaCtx.video_width, task_.mediaCtx.video_height, (dst_w - task_.mediaCtx.video_width) / 2, (dst_h - task_.mediaCtx.video_height) / 2);
			}
	}
	std::cout << "AspectRatio filter param "<< args<<std::endl;
	AVFilterInOut* in = avfilter_inout_alloc();
	AVFilterInOut* out = avfilter_inout_alloc();
	
	in->filter_ctx = vbuffer_Ctx;
	in->name = av_strdup("vin0");
	in->next = NULL;
	in->pad_idx = 0;

	out->filter_ctx = vbufferSink_Ctx;
	out->name = av_strdup("vout0");
	out->next = NULL;
	out->pad_idx = 0;

	int ret = avfilter_graph_parse_ptr(video_graph, args, &out, &in, NULL);
	if (ret < 0) {
		goto end;
	}
	ret = avfilter_graph_config(video_graph, NULL);
end:
	avfilter_inout_free(&in);
	avfilter_inout_free(&out);
	return ret;
}

void AspectRatio::Adapt(const TaskApply& task_, int& dst_w, int& dst_h)
{
	float ratio_dst = task_.mediaCtx.video_width / 1.0 / task_.mediaCtx.video_height;
	float ratio_src = mediaInfo.video_width / 1.0 / mediaInfo.video_height;	
	if (AdaptStyle::Fill == task_.adstyle) {
		if (ratio_src > ratio_dst) {
			dst_w = task_.mediaCtx.video_width;
			dst_h = dst_w /1.0/ratio_src;
			
		}
		else {
			dst_h = task_.mediaCtx.video_height;
			dst_w = dst_h * ratio_src;
			
		}
	}
	else { // AdaptStyle::Crop
		if (ratio_src > ratio_dst) {
			dst_h = task_.mediaCtx.video_height;
			dst_w = dst_h * ratio_src;
		}
		else {
			dst_w = task_.mediaCtx.video_width;
			dst_h = dst_w / 1.0 / ratio_src;
		}
	}

	// 分辨率应为偶数
	if (dst_w % 2)
		dst_w += 1;

	if (dst_h % 2)
		dst_h += 1;
}

int AspectRatio::pushVideo(DataSet& p)
{
	AVFrame* frame_src = (AVFrame*)p.srcData;
	int ret = av_buffersrc_add_frame_flags(vbuffer_Ctx, frame_src, 0/*AV_BUFFERSRC_FLAG_KEEP_REF*/);	
	if (ret < 0) {
		return ret;
	}


	while (true) {
		AVFrame* frame = av_frame_alloc();
		ret = av_buffersink_get_frame_flags(vbufferSink_Ctx, frame, 0);
		if (ret < 0) {
			av_frame_free(&frame_src);
			av_frame_free(&frame);
			if (ret == AVERROR(EAGAIN))
				return 0;
			else if (ret == AVERROR_EOF) {
				DataSet data;
				data.type = MEDIATYPE::VIDEO;
				data.srcData = nullptr;
				if (link) {
					link->fillBuffer(data);
				}
			}
			return ret;
		}

		frame->pts = av_rescale_q(frame->pts, AVRational{ 1,25 }, AVRational{ 1,AV_TIME_BASE });
		DataSet data;
		data.type = MEDIATYPE::VIDEO;
		data.srcData = frame;
		if (link) {
			link->fillBuffer(data);
		}else{
			av_frame_free(&frame);	
		}
		

	}

	av_frame_free(&frame_src);
	return 0;
}

