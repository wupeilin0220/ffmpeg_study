#pragma once
#include "filter.h"
 
#include <vector>
#include <stdlib.h>


class DrawText :
	public filterBase, public Linker,private Serial
{
public:
	DrawText(const MuxContext& attr);
	virtual ~DrawText();
	virtual int fillBuffer(DataSet p) override;
protected:
	virtual int init_effectfilter(TaskApply& task_) override;
	virtual int init_videoEffect(TaskApply& task_);
	int pushVideo(DataSet& p);
	int pushAudio(DataSet& p);

	virtual void serialize() final;
protected:
	int		m_fontsize;
	int		m_movetype;
	int		m_second;
	int		m_box;
	int		m_boxborderw;
	double	m_fontwidth;
	double	m_speed;
	double	m_x;
	double	m_y;
	double  m_alpha;
	double	m_fontAlpha;
	char	m_fontcolor[32];
	char	m_boxcolor[32];
	char	m_text[512];
	AVFilterContext* vdrawtext_ctx;
	static int textNum;
};

