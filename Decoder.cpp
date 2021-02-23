// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "Decoder.h"
#include <algorithm>
#include <string>
#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

static const struct {
//	const char* name;
	int         nb_channels;
	uint64_t     layout;
} channel_layout_map[] = {
	{ 1,  AV_CH_LAYOUT_MONO },
	{ 2,  AV_CH_LAYOUT_STEREO },
	{ 3,  AV_CH_LAYOUT_2POINT1 },
	{ 3,  AV_CH_LAYOUT_SURROUND },
	{ 3,  AV_CH_LAYOUT_2_1 },
	{ 4,  AV_CH_LAYOUT_4POINT0 },
	{ 4,  AV_CH_LAYOUT_QUAD },
	{ 4,  AV_CH_LAYOUT_2_2 },
	{ 4,  AV_CH_LAYOUT_3POINT1 },
	{ 5,  AV_CH_LAYOUT_5POINT0_BACK },
	{ 5,  AV_CH_LAYOUT_5POINT0 },
	{ 5,  AV_CH_LAYOUT_4POINT1 },
	{ 6,  AV_CH_LAYOUT_5POINT1_BACK },
	{ 6,  AV_CH_LAYOUT_5POINT1 },
	{ 6,  AV_CH_LAYOUT_6POINT0 },
	{ 6,  AV_CH_LAYOUT_6POINT0_FRONT },
	{ 6,  AV_CH_LAYOUT_HEXAGONAL },
	{ 7,  AV_CH_LAYOUT_6POINT1 },
	{ 7,  AV_CH_LAYOUT_6POINT1_BACK },
	{ 7,  AV_CH_LAYOUT_6POINT1_FRONT },
	{ 7,  AV_CH_LAYOUT_7POINT0 },
	{ 7,  AV_CH_LAYOUT_7POINT0_FRONT },
	{ 8,  AV_CH_LAYOUT_7POINT1 },
	{ 8,  AV_CH_LAYOUT_7POINT1_WIDE_BACK },
	{ 8,  AV_CH_LAYOUT_7POINT1_WIDE },
	{ 8,  AV_CH_LAYOUT_OCTAGONAL },
	{ 16, AV_CH_LAYOUT_HEXADECAGONAL },
	{ 2,  AV_CH_LAYOUT_STEREO_DOWNMIX, },
};

Decoder::Decoder():
	type(StreamType::None),
	formatContext(NULL), video_codecCtx(NULL), audio_codecCtx(NULL), dict(nullptr), 
	video_index(-1), audio_index(-1),
	info(),
	start_time(0),
	dec_time(0),
	inputUrl()
{
	nowVideoPts = 0;
	getDecodeTime();			
}


Decoder::~Decoder()
{
	av_dict_free(&dict);
	uninit_decoder();
	std::cout << "Decoder is distruct_________\n";
}

int Decoder::init_decoder(const std::string& url,int )
{
	int ret = 0;
	if (url.empty()) {
		return -1;
	}
	std::cout <<"input url " <<url<<std::endl;
	formatContext = avformat_alloc_context();
	if(!formatContext && !(formatContext = avformat_alloc_context())){
		std::cout << "alloc format context fail\n";
		return -1;
	}

//	setOpenOption();		// rtmp can't set timeout option   
	setInterruptCallback();
//	formatContext->max_analyze_duration = 8;
//	formatContext->probesize = 50 * 1024 * 1024;
//	int retryTimes = retry;
	start_time = av_gettime();
	if ((ret = avformat_open_input(&formatContext, url.c_str(), NULL, &dict)) < 0) {
		std::cout << "avformat_open_input err " << AVERROR(ret) << std::endl;		
		avformat_free_context(formatContext);
		formatContext = NULL;
		return ret;
	}

	if ((ret = avformat_find_stream_info(formatContext, NULL)) < 0) {
		std::cout << "avformat_find_stream_info err " << ret << std::endl;
		avformat_free_context(formatContext);
		formatContext = NULL;
		return ret;
	}

	ret = 2;	
	if (init_videoDec() < 0) {
		ret--;
	}

	if (init_audioDec() < 0) {
		ret--;
	}
	inputUrl = url;
	auto p = url.find_last_of('.');
	if(std::string::npos != p){
		if((url.c_str()+p) == std::string(".mp3") || (url.c_str()+p) == std::string(".wav")
			||(url.c_str()+p) == std::string(".flac") || (url.c_str()+p) == std::string(".ogg")||(url.c_str()+p) == std::string(".wma")){
			video_index = -1;	
			info.hasVideo = false;	
		}
	}
	generateMediainfo();
		
	return (ret - ((formatContext->nb_streams)>2?2:formatContext->nb_streams));
}

void Decoder::setOpenOption(void)
{
	// set time out
	av_dict_set(&dict, "timeout", "10", 0);
}



int Decoder::init_videoDec()
{
	
	AVCodec* codec_v = NULL;
	start_time = av_gettime();
	int ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec_v, 0);
	if (ret < 0) {
		std::cout << "av_find_best_stream err " << AVERROR(ret) << std::endl;
		if (AVERROR_DECODER_NOT_FOUND == ret) {
			std::cout << "video AVERROR_DECODER_NOT_FOUND \n";
		}
	}
	
	else
		video_index = ret;

	if (video_index >= 0) {
		video_codecCtx = avcodec_alloc_context3(codec_v);
		if (!video_codecCtx) {
			std::cout << "video avcodec_alloc_context3 err\n";
		}

		avcodec_parameters_to_context(video_codecCtx, formatContext->streams[video_index]->codecpar);
//		video_codecCtx->time_base = formatContext->streams[video_index]->codec->time_base;
		ret = avcodec_open2(video_codecCtx, codec_v, NULL);
		if (ret < 0) {
			std::cout << "video avcodec_open2 err " << ret << std::endl;
			return ret;
		}
	}
	return ret;
}

int Decoder::init_audioDec()
{	
	AVCodec* codec_a = NULL;
	int ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec_a, 0);
	if (ret < 0) {
		std::cout << "av_find_best_stream err " << ret << std::endl;
	}
	else
		audio_index = ret;

	if (audio_index >= 0) {
		audio_codecCtx = avcodec_alloc_context3(codec_a);
		if (!audio_codecCtx) {
			std::cout << "audio avcodec_alloc_context3 err\n";
		}
		
		
		avcodec_parameters_to_context(audio_codecCtx, formatContext->streams[audio_index]->codecpar);

		ret = avcodec_open2(audio_codecCtx, codec_a, NULL);
		if (ret < 0) {
			std::cout << "audio avcodec_open2 err " << ret << std::endl;
			return ret;
		}
	}
	return ret;
}


void Decoder::uninit_decoder()
{
//
//	stopStream();
	if (video_codecCtx) {
		avcodec_free_context(&video_codecCtx);
		video_codecCtx = NULL;
	}

	if (audio_codecCtx) {
		avcodec_free_context(&audio_codecCtx);
		audio_codecCtx = NULL;
	}

	if (formatContext) {
		avformat_free_context(formatContext);
		formatContext = NULL;
	}
	video_index = -1;
	audio_index = -1;
}

void Decoder::generateMediainfo()
{
	info.hasVideo = false;
	// video
	if (video_index >= 0) {
		info.video_width = video_codecCtx->width;
		info.video_height = video_codecCtx->height;
		info.video_format = (int)video_codecCtx->pix_fmt;
		info.video_timebase = formatContext->streams[video_index]->time_base;
		info.video_bits = video_codecCtx->bit_rate;

		AVRational tmp = formatContext->streams[video_index]->avg_frame_rate;
		if (tmp.num <= 0 || tmp.den <= 0) {
			info.fps = formatContext->streams[video_index]->r_frame_rate.num
				/ formatContext->streams[video_index]->r_frame_rate.den;
		}
		else {
			info.fps = tmp.num / tmp.den;
		}
		info.hasVideo = true;
		std::cout <<inputUrl << "source has video width "<< info.video_width << " height " << info.video_height<<std::endl;
	}
	
	info.hasAudio = false;
	//audio
	if (audio_index >= 0) {
		std::cout<< inputUrl << " source has audio sample "<< audio_codecCtx->sample_fmt<< " channel "<< audio_codecCtx->channels<<std::endl;
		info.audio_channel = audio_codecCtx->channels;
		// wma channel_layout correction 
		bool checkChannel = false;
		for (unsigned int i(0); i < ARRAY_ELEMS(channel_layout_map); i++) {
			if (info.audio_channel == channel_layout_map[i].nb_channels &&
					audio_codecCtx->channel_layout == channel_layout_map[i].layout) {
				checkChannel = true;
				break;
			}
		}

		if (checkChannel) {
			info.audio_channel_layout = audio_codecCtx->channel_layout;
		}
		else {
			info.audio_channel_layout = channel_layout_map[info.audio_channel - 1].layout;
		}
		info.sample = audio_codecCtx->sample_rate;
		info.audio_format = audio_codecCtx->sample_fmt;
		info.audio_timebase = formatContext->streams[audio_index]->time_base;
		info.hasAudio = true;
	}	
}
