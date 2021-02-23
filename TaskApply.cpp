#include "TaskApply.h"

TaskApply::TaskApply():
	mediaCtx(),
	type(ApplyType::NONE)
{

}

TaskApply::TaskApply(ApplyType t):
	mediaCtx(),
	type(ApplyType::NONE)
{
	initUnion(t);
}

TaskApply::TaskApply(const TaskApply& _Right):
	type(ApplyType::NONE)
{
	initUnion(_Right.getType());
	copyUnion(_Right);
}

TaskApply& TaskApply::operator=(const TaskApply& _Right)
{
	if (&_Right != this) {
		setType(_Right.getType());
		copyUnion(_Right);
	}
	return *this;
}

TaskApply::~TaskApply()
{
	unInitUnion();
}

void TaskApply::setType(ApplyType t)
{
	if (t != type) {
		unInitUnion();
		initUnion(t);
	}
}

void TaskApply::initUnion(ApplyType t)
{
	if (t == type) {
		return;
	}

	switch (t)
	{
	case ApplyType::LOGO:
		new(&logo)	Logo();
		break;
	case ApplyType::TEXT:
	case ApplyType::TIMEMARK:
		new(&text) Text();
		break;

	case ApplyType::CREATE:
	case ApplyType::UPDATA:
		new(&export_stream) Export();
		break;

	case ApplyType::ADDSTREAM:
	case ApplyType::ADDSPACER:
		new(&stream) SrcStream();
		break;

	case ApplyType::MOSAICO:
		new(&mosaic) MosaicoRect();
		break;

	case ApplyType::REMOVEFFECT:
	case ApplyType::REMOVESPACER:
		new(&remove_id) std::string();
		break;

	case ApplyType::REMOVESTREAM:
		new(&remove_stream) RemoveStream();
		break;

	case ApplyType::ADDSOURCE:
	case ApplyType::USESPACER:
		new(&source) Source();
		break;
		
	case ApplyType::ADAPT:
		new(&adstyle) AdaptStyle;
		adstyle = AdaptStyle::Fill;
		break;

	default:
		break;
	}
	type = t;
}

void TaskApply::unInitUnion()
{
	switch (type)
	{
	case ApplyType::LOGO:
		logo.~Logo();
		break;

	case ApplyType::TEXT:
	case ApplyType::TIMEMARK:
		text.~Text();
		break;

	case ApplyType::CREATE:
	case ApplyType::UPDATA:
		export_stream.~Export();  
		break;

	case ApplyType::ADDSPACER:
	case ApplyType::ADDSTREAM:
		stream.~SrcStream();
		break;

	case ApplyType::MOSAICO:
		mosaic.~MosaicRect();
		break;

	case ApplyType::REMOVEFFECT:
	case ApplyType::REMOVESPACER:
		remove_id.~basic_string();
		break;

	case ApplyType::REMOVESTREAM:
		remove_stream.~RemoveStream();
		break;

	case ApplyType::ADDSOURCE:
	case ApplyType::USESPACER:
		source.~Source();
		break;

	default:
		break;
	}
	type = ApplyType::NONE;
}

void TaskApply::copyUnion(const TaskApply& _Right)
{
	reqMsg = _Right.reqMsg;
	mediaCtx = _Right.mediaCtx;
	switch (type)
	{
	case ApplyType::LOGO:
		logo = _Right.logo;
		break;

	case ApplyType::TEXT:
	case ApplyType::TIMEMARK:
		text = _Right.text;
		break;

	case ApplyType::CREATE:
	case ApplyType::UPDATA:
		export_stream = _Right.export_stream;
		break;

	case ApplyType::ADDSTREAM:
	case ApplyType::ADDSPACER:
		stream = _Right.stream;
		break;

	case ApplyType::MOSAICO:
		mosaic = _Right.mosaic;
		break;

	case ApplyType::REMOVEFFECT:
	case ApplyType::REMOVESPACER:
		remove_id = _Right.remove_id;
		break;

	case ApplyType::REMOVESTREAM:
		remove_stream = _Right.remove_stream;
		break;

	case ApplyType::ADDSOURCE:
	case ApplyType::USESPACER:
		source = _Right.source;
		break;
	case ApplyType::VOLUME:
		volume = _Right.volume;
		break;

	case ApplyType::ADAPT:
		adstyle = _Right.adstyle;
		break;

	case ApplyType::NONE:
	case ApplyType::CLOSE:
	case ApplyType::STOPSPACER:
	case ApplyType::STOP:
//	case ApplyType::USESPACER:
		break;
	default:
	break;
	}
}
