#pragma once
#include <string>
#include <iostream>
#include <vector>
#include "DataStructure.h"

class TaskApply;
typedef std::vector<TaskApply> JsonReqInfo;
/*
*	before get union member £¬call setType() init union member.
*/
class TaskApply
{
public:
	TaskApply();
	TaskApply(ApplyType t);
	TaskApply(const TaskApply& _Right);
	TaskApply& operator=(const TaskApply& _Right);
	~TaskApply();

	void setType(ApplyType t);
	ApplyType getType() const { return type; }
	MuxContext	mediaCtx;
	std::string	reqMsg;
	union {
		Logo			logo;
		Text			text;
		Export			export_stream;
		SrcStream		stream;
		MosaicRect		mosaic;
		std::string		remove_id;   // effect id // spacer url
		RemoveStream	remove_stream;
		Source			source;
		double		    volume;
		AdaptStyle		adstyle;
	};
private:	
	void initUnion(ApplyType t);
	void unInitUnion();
	void copyUnion(const TaskApply& _Right);
private:
	ApplyType	type;
};

