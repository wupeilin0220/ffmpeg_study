#include "Mosaic.h"

Mosaic::Mosaic(const MuxContext& attr):
	info(attr),times(0),randnum(0)
{
	
}

Mosaic::~Mosaic(){
	
}

int Mosaic::fillBuffer(DataSet p)
{
	if (link) {
		if (p.type == MEDIATYPE::VIDEO) {
			return dataStream(p);
		}
		else {
			mutex_link.lock();
			int ret = link->fillBuffer(p);
			mutex_link.unlock();
			return ret;
		}
	}
	return -1;
}

int Mosaic::initTask(TaskApply& task_)
{
	if(task_.getType() != ApplyType::MOSAICO){
		return -1;
	}
	int neightbourhood = task_.mosaic.unit_pixel;

	
	int mosaic_offsetX = task_.mosaic.rect.offset_x;
	int mosaic_offsetY = task_.mosaic.rect.offset_y;
	int mosaic_x = task_.mosaic.rect.x;
	int mosaic_y = task_.mosaic.rect.y;
	std::cout << mosaic_offsetX << " "<<mosaic_offsetY << " "<< mosaic_x<<" "<< mosaic_y<<" "<< neightbourhood<<std::endl; 
	if(neightbourhood <=0 || mosaic_x <= 0 || mosaic_y <= 0 ){
		return -1;
	}

	int act_offset_x = mosaic_offsetX,act_offset_y = mosaic_offsetY;
	int act_x = mosaic_x,act_y = mosaic_y;

	if(mosaic_offsetX < 0){
		act_offset_x = 0;
		act_x = mosaic_offsetX + mosaic_x;
	}

	if(mosaic_offsetY < 0){
		act_offset_y = 0;
		act_y = mosaic_offsetY + mosaic_y;
	}

	if(mosaic_offsetX + mosaic_x > info.video_width){
		act_x = info.video_width - act_offset_x;
	}

	if(mosaic_offsetY + mosaic_y > info.video_height){
		act_y = info.video_height - act_offset_y;
	}


//	if(mosaic_offsetX + mosaic_x > info.video_width || mosaic_y + mosaic_offsetY > info.video_height){
//		return -1;
//	}

	task = task_;
	task.mosaic.rect.offset_x = act_offset_x;
	task.mosaic.rect.offset_y = act_offset_y;
	task.mosaic.rect.x = act_x;
	task.mosaic.rect.y = act_y;
	return 0;
}

int Mosaic::dataStream(DataSet& p)
{
	AVFrame* frame = (AVFrame*)p.srcData;
	
	if(frame->width <= 0 || frame->height <= 0 || frame->format != AV_PIX_FMT_YUV420P)
		return -1;

	//马赛克的大小
	int neightbourhood = task.mosaic.unit_pixel;

	
	int mosaic_offsetX = task.mosaic.rect.offset_x;
	int mosaic_offsetY = task.mosaic.rect.offset_y;
	int mosaic_x = task.mosaic.rect.x;
	int mosaic_y = task.mosaic.rect.y;
	int coverlength_x = neightbourhood;
	int coverlength_y = neightbourhood;

//yuv420p
//	times++;
	if(!(times++ % 5))
		srand(av_gettime()%10000);
	for (int lines(0); lines < mosaic_y; ) {
		coverlength_x = neightbourhood;
		if (lines + neightbourhood > mosaic_y) {
			coverlength_y = mosaic_y - lines;
		}

		for (int rows(0); rows < mosaic_x; ) {
			if (rows + neightbourhood > mosaic_x)
				coverlength_x = mosaic_x - rows;
						
		//	randnum = rand()%(neightbourhood-1);
			randnum =  rand() % (neightbourhood - 1);

			uint8_t Y = *(frame->data[0] + ((long)mosaic_offsetY + lines + randnum) * frame->linesize[0] + mosaic_offsetX + rows + randnum);
//			uint8_t Y = *(frame->data[0] + ((long)mosaic_offsetY + lines + randnum) * frame->linesize[0] + mosaic_offsetX + rows + randnum);

	//		uint8_t U = *(frame->data[1] + (mosaic_offsetY/2 + lines/2) * frame->linesize[1] + (mosaic_offsetX + rows + randnum)/2);
//			uint8_t V = *(frame->data[2] + (mosaic_offsetY/2 + lines/2) * frame->linesize[2] + (mosaic_offsetX + rows + randnum)/2);
			uint8_t U = *(frame->data[1] + (mosaic_offsetY + lines)/2*frame->linesize[1] + (mosaic_offsetX + rows + randnum)/2);
			uint8_t V = *(frame->data[2] + (mosaic_offsetY + lines)/2*frame->linesize[2] + (mosaic_offsetX + rows + randnum)/2);
			for (int x(0); x < coverlength_y; x++) {
				memset(frame->data[0] + (mosaic_offsetY + lines + x) * frame->linesize[0] + mosaic_offsetX + rows, Y, coverlength_x);
				if (x % 2) {
					memset((frame->data[1] + (mosaic_offsetY/2 + lines/2 + x/2) * frame->linesize[1] + (mosaic_offsetX + rows)/2), U, coverlength_x/2);
					memset((frame->data[2] + (mosaic_offsetY/2 + lines/2 + x/2) * frame->linesize[2] + (mosaic_offsetX + rows)/2), V, coverlength_x/2);
				}
			}
			rows += neightbourhood;
		}
			
		lines += neightbourhood;
	}

//	std::cout << "Mosaico time use " << (av_gettime() - sT)  << std::endl;
	if (link) {
		return link->fillBuffer(p);
	}

	return -1;
}
