#pragma once
#ifndef  DATASTRUCTURE_H
#define  DATASTRUCTURE_H
extern "C" {
#include <libavutil/rational.h>
};
#include <vector>
#include "Serial.h"
#include "rapidjson/document.h"
#include <functional>

constexpr auto URLSIZE = 512;
constexpr int64_t NetTimeout = 12 * 1000 * 1000;
constexpr auto PINS = 8;
constexpr auto MAX_INPUTSTREAM = 9;
constexpr auto	INTERRUPTTIME = 3 * 1000 * 1000;
constexpr auto CONFPATH = "conf.txt";

typedef std::vector<std::string> Source;
typedef std::function<void(int64_t)> StreamBreak;

enum class AdaptStyle {
	Crop = 0,
	Fill
};

typedef struct Rect {
	int x;
	int y;
	int offset_x;
	int offset_y;
}Rect;

typedef struct Position {
	bool operator<(const Position& _Right) const {
		if(id ==  _Right.id)
			return false;
		return id < _Right.id;
	}
	int id;  // input stream's pin
	int level;
	int64_t seekPos;
	Rect rect;
}Position;

typedef struct Logo {
	Logo():rect() {
		memset(name, 0, URLSIZE);
		memset(id, 0, URLSIZE);
		Alpha = 1;
		level = 0;
	}

	char	name[URLSIZE];
	int		level;
	char	id[URLSIZE];
	double  Alpha;
	Rect	rect;
}Logo;

typedef struct MosaicRect {
	int		unit_pixel;
	int     level;
	char	id[URLSIZE];
	Rect	rect;
}MosaicoRect;

typedef struct AudioInput {
	int id;  // input channel
	double volume;
	int64_t seekPos;
	bool operator<(const AudioInput& left) const {
		if(id == left.id)
			return false;
		return (id < left.id);
	}
}AudioInput;

enum class StreamType {
	NetStream = 0,
	FilStream,
	PicStream,
	None
};

typedef struct SrcStream {
	SrcStream() :
		type(StreamType::None), interValTime(0), windowID(-1), effect(1), urlStream(), seekPos(-1) {

	}
	StreamType					type;
	int64_t						interValTime;
	int							windowID;
	int							effect;
	std::vector<std::string>	urlStream;
	int64_t						seekPos;
}SrcStream;

typedef struct Text {
	Text():fontSize(0),fontWidth(0),second(0),moveType(0),box(0),boxborderw(0),speed(-1),
	x(0),y(0),alpha(0),fontAlpha(0),level(0),delay(0)
	{
		memset(subtitle, 0, URLSIZE);
		memset(id, 0, URLSIZE);
		memset(font, 0, 32);
		memset(color, 0, 32);
		memset(boxcolor, 0, 32);
	}

	char	subtitle[URLSIZE];
	char	font[32];
	char	color[32];	// RGBA  0-255 格式0xFFFFFF
	char	boxcolor[32];	//	字幕背景颜色
	int		fontSize;
	double	fontWidth;
	int		second;		//	字幕出现时间
	int		moveType;	//	跑马灯
	int     box;		//	是否加边框
	int		boxborderw;	//	边框宽度（文字背景）
	double	speed;		//	字幕速度 default 7.5
	double	x;
	double	y;
	double  alpha;		//	字幕透明度
	double  fontAlpha;
	char	id[URLSIZE];
	int     level;
	int		delay;
}Text;

typedef struct Export {
	Export() :
		delay(0), seekPos(-1), volume(1.0), videoWindowID(-1), audioWindowID(-1),
		audioMix(),videoMix()
	{
		memset(pushUrl, 0, URLSIZE);
	}
	~Export() {
		std::cout << "Export destruct\n";
	}
	int64_t		delay;
	double	seekPos;
	double	volume;
	int		videoWindowID;
	int		audioWindowID;
	char	pushUrl[URLSIZE];
	std::vector<AudioInput> audioMix;
	std::vector<Position> videoMix;
}Export;

typedef struct RemoveStream {
	RemoveStream() :windowID(-1) {
		memset(url, 0, URLSIZE);
	}
	char	url[URLSIZE];
	int		windowID;
}RemoveStream;

typedef struct MuxContext {
	bool		hasVideo;
	int			video_width;
	int			video_height;
	int			fps;
	int64_t		video_bits;
	int			video_format;	//	value is ffmepg AV_PIX_FMT_XXX
	AVRational	video_timebase;

	bool		hasAudio;
	int			sample;
	int			audio_bits;
	int			audio_channel;
	int64_t		audio_channel_layout;
	int			audio_format;	//	value is ffmepg AV_PIX_FMT_XXX	
	AVRational	audio_timebase;

	MuxContext() :
		hasVideo(false), video_width(0), video_height(0), fps(0), video_bits(0), video_format(0),
		hasAudio(false), sample(0), audio_bits(0), audio_channel(0), audio_channel_layout(0), audio_format(0)
	{
	}

}MuxContext;

enum class ApplyType {
	NONE,
	LOGO,
	TEXT,
	TIMEMARK,     // union text
	CREATE,
	ADDSTREAM,	  // union stream
	MOSAICO,
	REMOVEFFECT,		  //  union std::string
	ADDSPACER,    //  union stream
	REMOVESPACER, // union remove_id
	REMOVESTREAM, // remove_stream;
	ADDSOURCE,	  // union source
	UPDATA,		  // export_stream		
	STOP,
	CLOSE,
	STOPSPACER,
	USESPACER,	 // source
	VOLUME,		//volume
	ADAPT		
};



typedef class ResponseInfo {
public:
	ResponseInfo():
		msgType(),
		retCode(0),windowID(-1),status(0),
		responseType(0),url(),
		hasVideo(false),hasAudio(false)
	{
	
	}
	~ResponseInfo() {

	}
	std::string msgType;
	int retCode;
	int windowID;
	int status;
	int responseType;  // for  recored download file type 
	std::vector<std::string> url;
	std::vector<std::string> succUrl;
	bool	hasVideo;
	bool	hasAudio;
}ResponseInfo;

#define Success				1000
#define ERR_PARAM			3001
#define	ERR_TIMEOUT			3002

#define ERR_JSONPARAM		1001
#define	ERR_TASKTYPE		1002
#define	ERR_REQUEST			1003
#define	ERR_REQUEST_OBJECT	1004

#define	ERR_Decode			2000
#define	ERR_Filter_Split	2001
#define	ERR_Filter_Vol		2002
#define	ERR_Filter_logo		2003
#define	ERR_Filter_Text		2009
#define	ERR_Encode_Init		2004
#define	ERR_Encode_Exist	2005
#define	ERR_Filter_Adapt	2006
#define ERR_ReponseTask		2007
#define ERR_PushPublic		2008
#define ERR_DownloadFail	2010


#define ERR_Download_File	4001
#define ERR_NOSPACER	4002


#define	ERR_NotImplemented  5000
#endif // ! DATASTRUCTURE_H
