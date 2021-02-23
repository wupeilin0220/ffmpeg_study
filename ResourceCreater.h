/*
 *  ResourceCreater.h
 *
 *  Created on: 2016?12?16?
 *  Author    : cdvcloud
 */

#ifndef RESOURCECREATOR_H_
#define RESOURCECREATOR_H_

#include <thread>
#include <string>
#include <map>
#include <stdint.h>
#include <curl/multi.h>
#include <algorithm>
#include <functional>
#include <mutex>
#define MAX_RES_PATH 1024

typedef std::function<int(bool)> DownloadOver;
typedef std::function<int(int64_t,int64_t)> CurrentProgress;

int remove_dir(std::string& dir);
int CreateDir(std::string path);

typedef enum ResourceType {
	LOCALFILE = 0,
	NETWORKFILE = 1,
	NETWORKSTREAM = 2,
} ResourceType;

class FileResource;

class ResourceCreater {
public:
	char m_res_path[MAX_RES_PATH];
	volatile bool m_bvalid;
	volatile bool m_bready;
	ResourceType  m_res_type;
	char m_res_url[MAX_RES_PATH];
	std::string getFilePath(){return m_filePath;}
public:
	ResourceCreater();
	virtual ~ResourceCreater();
	static ResourceCreater* create_res(const char* url);
	static std::string FindSourceUrl(const std::string& path);
	static int RemoveResource(const std::string &url,ResourceCreater* removeRes);
	static int getNetFileInfo(const char* url, double& size);
	static int removeFile(const std::string& file);
	static int ClearBuff();
	
	virtual int AsyncDownload(DownloadOver over, CurrentProgress percent)=0;
	virtual int SyncDownload() = 0;
	virtual int stop() = 0;
	virtual int valid() = 0;
protected:
	virtual int exec() = 0;
	std::thread* hand_download;
	static std::multimap<std::string, FileResource*> m_file_res;
	static std::string m_filePath;
	static std::mutex mutex_res;
	std::string  CachePath;
};

class FileResource : public ResourceCreater
{
public:
	char m_res_name[MAX_RES_PATH];
	char m_res_exp[10];
	volatile bool m_blocal;
	volatile bool m_bdownloading;
	volatile bool m_bstopdownload;
	double	  m_res_totalsize;
	volatile bool m_bhasdownload;

private:
	void Init();

public:
	FileResource(const char* url);
	virtual ~FileResource();
	void initeasy(CURLM *cm, FILE *fl);
	static size_t write_file(void *ptr,size_t size,size_t nmemb,FILE *stream);
	virtual int AsyncDownload(DownloadOver over, CurrentProgress percent) override;
	virtual int SyncDownload() override;
	virtual int stop() override;
	virtual int valid() override;
	
	int GetResurceType(const char *url);
protected:
	int getFileLength(const char* url);
	virtual int exec() override;
private:
	DownloadOver overFun;
	CurrentProgress	 percentFun;
	volatile bool run;
	std::mutex mutex_thread;
};

class StreamResource : public ResourceCreater
{
public:
	StreamResource(const char* url);
	virtual ~StreamResource();
	virtual int AsyncDownload(DownloadOver , CurrentProgress ) override;
	virtual int SyncDownload() override { return 0; }
	virtual int stop() override{return 0;}	
	virtual int valid() override{return 0;}
protected:
	virtual int exec() override{return 0;}

private:
	static int decode_interrupt_cb(void *ctx);

};
#endif /* RESOURCECREATOR_H_ */
