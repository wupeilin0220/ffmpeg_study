//#pragma warning(disable:4996)
#include "DrawText.h"
#include <string>
//#include <direct.h>

int DrawText::textNum = 0;
void string_replace(std::string& s1, const std::string& s2, const std::string& s3)
{
	std::string::size_type pos = 0;
	std::string::size_type a = s2.size();
	std::string::size_type b = s3.size();
	while ((pos = s1.find(s2, pos)) != std::string::npos)
	{
		s1.replace(pos, a, s3);
		pos += b;
	}
}

std::vector<std::string> split(std::string& str, const char* c)
{
	char* cstr, * p;
	std::vector<std::string> res;
	cstr = new char[str.size() + 1];
	strcpy(cstr, str.c_str());
	p = strtok(cstr, c);
	while (p != NULL)
	{
		res.push_back(p);
		p = strtok(NULL, c);
	}
	delete[]cstr;
	cstr = NULL;
	return res;
}

DrawText::DrawText(const MuxContext& attr):
	filterBase(attr),
	m_fontsize(0),
	m_movetype(0),
	m_second(0),
	m_box(0),
	m_boxborderw(0),
	m_fontwidth(150),
	m_speed(7.5),
	m_x(1.0),m_y(1.0),
	m_alpha(1.0),m_fontAlpha(1.0),
	vdrawtext_ctx(nullptr)
{
	strcpy(m_fontcolor,"white");
	strcpy(	m_boxcolor,"white");
	memset(m_text,0,sizeof(m_text));
}


DrawText::~DrawText()
{
	serialize();
	if(vdrawtext_ctx)
		avfilter_free(vdrawtext_ctx);
	std::cout << "DrawText disstruct\n";
}

int DrawText::fillBuffer(DataSet p)
{
	if (p.type == MEDIATYPE::VIDEO) {
		return pushVideo(p);
	}
	else {
		return pushAudio(p);
	}
	return 0;
}

int DrawText::init_effectfilter(TaskApply& task_)
{
	if (!mediaInfo.hasVideo)
		return -1;

	int ret = init_videoEffect(task_);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int DrawText::init_videoEffect(TaskApply& task_)
{

	std::cout << "init DrawText \n";
	if (task_.getType() == ApplyType::TEXT) {
		if (strlen(task_.text.subtitle) > 0) {
			memset(m_text, 0, 512);
			snprintf(m_text, 512, "%s", task_.text.subtitle);

			std::string strtxt = m_text;
			string_replace(strtxt, std::string("\\"), std::string("\\\\"));
			string_replace(strtxt, std::string("#%"), std::string(","));
			string_replace(strtxt, std::string("#@"), std::string("="));
			string_replace(strtxt, std::string("%#"), std::string(";"));
			string_replace(strtxt, std::string("#$"), std::string(" "));
			string_replace(strtxt, std::string("%"), std::string("\\%"));
			string_replace(strtxt, std::string(":"), std::string("\\:"));
			string_replace(strtxt, std::string("\xc2\xa0"), std::string(" "));	//"\xc2\xa0"ÎªÇ°¶Ë"&nbsp;" ¿Õ¸ñÕ¼Î»·û

			snprintf(m_text, 512, "%s", strtxt.c_str());
		}

		char ttfurl[64] = {0};
		if (strlen(task_.text.font) > 0) {
			memset(ttfurl, 0, 64);
			std::string tmp_font(task_.text.font);
			if(tmp_font == "\u9ed1\u4f53"){		//	simhei
				snprintf(ttfurl, 64, "%s", "./fonts/simhei.ttf");
			}
			else if(tmp_font == "\u5b8b\u4f53"){		//	simsun
				snprintf(ttfurl, 64, "%s", "./fonts/simsun.ttc");
			}
			else if(tmp_font == "\u7ad9\u9177\u9ad8\u7aef\u9ed1\u4f53"){	//	zk-gdhei
				snprintf(ttfurl, 64, "%s", "./fonts/zk-gdhei.ttf");
			}
			else if(tmp_font == "\u7ad9\u9177\u9177\u9ed1\u4f53"){		//	zk-khei
				snprintf(ttfurl, 64, "%s", "./fonts/zk-khei.ttf");
			}
			else if(tmp_font == "\u7ad9\u9177\u5feb\u4e50\u4f53"){		//	zk-kl
				snprintf(ttfurl, 64, "%s", "./fonts/zk-kl.ttf");
			}
			else if(tmp_font == "\u7ad9\u9177\u5e86\u79d1\u9ec4\u6cb9\u4f53"){	//	zk-qkhy
				snprintf(ttfurl, 64, "%s", "./fonts/zk-qkhy.ttf");
			}
			else if(tmp_font == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u5e38\u89c4"){	//	¿¿¿¿¿¿¿¿?al-cg
				snprintf(ttfurl, 64, "%s", "./fonts/al-cg.ttf");
			}
			else if(tmp_font == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7c97"){	//	¿¿¿¿¿¿¿¿ al-cu
				snprintf(ttfurl, 64, "%s", "./fonts/al-cu.ttf");
			}
			else if(tmp_font == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7279\u7c97"){	//	¿¿¿¿¿¿¿¿?al-tecu
				snprintf(ttfurl, 64, "%s", "./fonts/al-tecu.ttf");
			}
			else if(tmp_font == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u7ec6"){	//	¿¿¿¿¿¿¿¿ al-xi
				snprintf(ttfurl, 64, "%s", "./fonts/al-xi.ttf");
			}
			else if(tmp_font == "\u963f\u91cc\u5df4\u5df4\u666e\u60e0\u4f53\u4e2d\u7b49"){	//	¿¿¿¿¿¿¿¿?al-zhong
				snprintf(ttfurl, 64, "%s", "./fonts/al-zhong.ttf");
			}
			else{
				std::cout << "can't find front type\n";
			}
		}

		if (strlen(task_.text.color) > 0) {
			snprintf(m_fontcolor, 32, "%s", task_.text.color);
		}

		// ÉèÖÃ±³¾°Í¸Ã÷¶È 0.0~1.0
		if (task_.text.alpha >= 0.0) {
			m_alpha = task_.text.alpha;
		}

		// ÉèÖÃ×ÖÌåÍ¸Ã÷¶È 0.0~1.0
		if (task_.text.fontAlpha >= 0.0) {
			m_fontAlpha = task_.text.fontAlpha;
		}

		if (task_.text.fontSize > 0) {
			m_fontsize = task_.text.fontSize;
		}

		if (task_.text.x > 0.0)
		{
			m_x = task_.text.x;
		}

		if (task_.text.y > 0.0)
		{
			m_y = task_.text.y;
		}

		m_movetype = task_.text.moveType;
		if (m_movetype) {
			if (task_.text.speed > 0.0) {
				m_speed = task_.text.speed;
			}

			if (task_.text.second > 0) {
				m_second = task_.text.second;
			}

			if (task_.text.fontWidth > 0.0)
			{
				m_fontwidth = task_.text.fontWidth;
			}
		}

		m_box = task_.text.box;
		if (m_box) {
			if (task_.text.boxborderw > 0) {
				m_boxborderw = task_.text.boxborderw;
			}

			if (strlen(task_.text.boxcolor) > 0) {
				snprintf(m_boxcolor, 32, "%s", task.text.boxcolor);
			}
		}
		char args[512] = { 0 };

		//	drawtext
		memset(args, 0, 512);
		if (!m_movetype)	//	¾²Ì¬×ÖÄ»
		{

			snprintf(args, 512,"box=%d:boxborderw=%d:boxcolor=%s@%.1f\
				:fontfile=%s:x=%.2f:y=%.2f\
				:fontcolor=%s@%.1f:fontsize=%d:text='%s'",
				m_box, m_boxborderw, m_boxcolor, m_alpha,
				ttfurl, m_x, m_y,
				m_fontcolor, m_fontAlpha, m_fontsize, m_text);
		}
		else if (m_movetype == 1)	//	¹ö¶¯×ÖÄ»
		{
			snprintf(args,512, "box=%d:boxborderw=%d:boxcolor=%s@%.1f\
				:fontfile=%s:x='if(gte(t,0),if(eq(ld(0),0),st(0,t),NAN));if(gte(t,0), (main_w-mod((t-ld(0))*%.2f,main_w+%.2f)), NAN)':y=%.2f\
				:fontcolor=%s@%.1f:fontsize=%d:text='%s'",
				m_box, m_boxborderw, m_boxcolor, m_alpha,
				ttfurl, m_speed, m_fontwidth, m_y,
				m_fontcolor, m_fontAlpha, m_fontsize, m_text);
		}
		std::cout <<"DrawText args " << args<< std::endl;
		
		int ret = avfilter_graph_create_filter(&vdrawtext_ctx, avfilter_get_by_name("drawtext"), "drawtext", args, NULL, video_graph);
		if (ret < 0) {
			std::cout << "drawtext create - %d\n"<< ret<<std::endl;
			return ret;
		}
		// link
		ret = avfilter_link(vbuffer_Ctx, 0, vdrawtext_ctx, 0);
		if (ret < 0) {
			return ret;
		}
		// link
		ret = avfilter_link(vdrawtext_ctx, 0, vbufferSink_Ctx, 0);
		if (ret < 0) {
			return ret;
		}
		ret = avfilter_graph_config(video_graph, NULL);
		if (ret < 0) {
			std::cout << "drawtext filter is failed\n";
			return ret;
		}
	}
	return 0;
}

int DrawText::pushVideo(DataSet& p)
{
	int ret = 0;
	AVFrame* frame = (AVFrame*)(p.srcData);
	ret = av_buffersrc_add_frame_flags(vbuffer_Ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
	if (ret < 0) {
		return ret;
	}
	while (1) {
		AVFrame* v_frame = av_frame_alloc();
		ret = av_buffersink_get_frame(vbufferSink_Ctx, v_frame);
		if (ret < 0) {
			av_frame_free(&frame);
			av_frame_free(&v_frame);
			if (AVERROR(EAGAIN) == ret) {
				return 0;
			}
			else if (AVERROR_EOF == ret) {
				DateSet empty;
				empty.type = MEDIATYPE::VIDEO;
				empty.srcData = nullptr;
				if (link) {
					link->fillBuffer(empty);
				}
				return 0;

			}
			else {
				return ret;
			}
		}

		DataSet data;
		data.type = MEDIATYPE::VIDEO;
		data.srcData = v_frame;

		if (link) {
			ret = link->fillBuffer(data);
			// ·µ»Ø-1£¬ÊÍ·ÅÖ¡
			if (-1 == ret) {
				av_frame_free(&v_frame);
				DateSet empty;
				empty.type = MEDIATYPE::VIDEO;
				empty.srcData = nullptr;
				if (link) {
					link->fillBuffer(empty);
				}
				else{
					av_frame_free(&frame);
				}
				return ret;
			}
		}
	}

	av_frame_free(&frame);
	return 0;
}

int DrawText::pushAudio(DataSet& p)
{
	int ret = 0;
	ret = link->fillBuffer(p);
	if (-1 == ret) {
		DateSet empty;
		empty.type = MEDIATYPE::AUDIO;
		empty.srcData = nullptr;
		if (link) {
			link->fillBuffer(empty);
		}
		return ret;
	}
	return 0;
}

void DrawText::serialize(){
	if(fp){
		mutex_syncFile.lock();
		char textIndex[32]={};
		snprintf(textIndex,32,"\n[text_%d]",textNum);
		fwrite(textIndex,1,strlen(textIndex),fp);

		char param[2048]={};
		snprintf(param,1024,"\nsubtitle=%s\nfont=%s\ncolor=%s\nboxcolor=%s\nfontSize=%d\nsecond=%d\nmoveType=%d\nbox=%d\nboxbroderw=%d\nfontWidth=%f\nspeed=%f\nx=%f\ny=%f\nalpha=%f\nfontAlpha=%f\nid=%s\nlevel=%d\ndelay=0",
		task.text.subtitle,task.text.font,task.text.color,task.text.boxcolor,task.text.fontSize,task.text.second,task.text.moveType,task.text.box,task.text.boxborderw,task.text.fontWidth,task.text.speed,task.text.x,task.text.y,task.text.alpha,task.text.fontAlpha,task.text.id,task.text.level);

		fwrite(param,1,strlen(param),fp);
		mutex_syncFile.unlock();
		textNum++;
		
		mutex_syncFile.unlock();
	}		
}


