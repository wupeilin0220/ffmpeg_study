/*
 *  ResourceCreater.h
 *
 *  Created on: 2016?12?16?
 *  Author    : cdvcloud
 */

#include <unistd.h>
//#include <string.h>
#include <stdexcept>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ResourceCreater.h"
#include "DataStructure.h"
#include <pcrecpp.h>
#include <uuid/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <iostream>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}
#include <dirent.h>
#include <fcntl.h>
#include "JsonConf.h"

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif




int remove_dir(std::string& dir)
{
	char cur_dir[] = ".";
	char up_dir[] = "..";
	std::string dir_name;
	DIR *dirp;
	struct dirent *dp;
	struct stat dir_stat;

	if ( 0 != access(dir.c_str(), F_OK) ) {
		return 0;
	}

	if ( 0 > stat(dir.c_str(), &dir_stat) ) {
		perror("get directory stat error");
		return -1;
	}

	if ( S_ISREG(dir_stat.st_mode) ) {
		remove(dir.c_str());
	} else if ( S_ISDIR(dir_stat.st_mode) ) {
		dirp = opendir(dir.c_str());
		while ( (dp=readdir(dirp)) != NULL ) {
			if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
				continue;
			}

			dir_name = dir+dp->d_name;
			remove_dir(dir_name);
		}
		closedir(dirp);
		rmdir(dir.c_str()); 
	} else {
		perror("unknow file type!");    
	}

	return 0;
}

int CreateDir(std::string path)
{
	std::size_t offset = 0;
	while (true) {
		offset = path.find_first_of("/", offset+1);

		if (offset != std::string::npos) {
			std::cout << "dir " << path.substr(0, offset).c_str() << std::endl;
			if (access(path.substr(0, offset).c_str(), F_OK | W_OK | R_OK) != 0) {
				if (mkdir(path.substr(0, offset).c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO) == -1) {
					return -1;
				}
			}			
		}
		else {
			offset = path.length();
			std::cout << "dir " << path.substr(0, offset).c_str() << std::endl;
			if (access(path.substr(0, offset).c_str(), F_OK | W_OK | R_OK) != 0) {
				if (mkdir(path.substr(0, offset).c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO) == -1) {
					return -1;
				}
			}
			break;
		}
	
	}

	return 0;
}

using namespace std;
std::multimap<std::string, FileResource*> ResourceCreater::m_file_res;
std::string ResourceCreater::m_filePath;
std::mutex ResourceCreater::mutex_res;
bool is_dir(std::string& path)
{
	struct stat statbuf;
	if(lstat(path.c_str(), &statbuf) ==0)
	{
			return S_ISDIR(statbuf.st_mode) != 0;
	}
	return false;
}

ResourceCreater::ResourceCreater():
	m_res_type(LOCALFILE),
	hand_download(nullptr)
{
	const Config *conf = Config::getConf(CONFPATH);
	CachePath = conf->GetCacheFilePath();
    m_bready = false;
    m_bvalid = false;
	memset(m_res_path,0,MAX_RES_PATH);
	memset(m_res_url,0,MAX_RES_PATH);
}

ResourceCreater::~ResourceCreater()
{
	
}

ResourceCreater* ResourceCreater::create_res(const char* url)
{
	char *tmp = new char[strlen(url)+1];
	memset(tmp,0,strlen(url)+1);
	memcpy(tmp,url,strlen(url));
	char *p = strchr(tmp,'?');
	if(p){
		*p = 0; 
	}

	pcrecpp::RE oPattern1("^rtmp://"),oPattern2("^rtsp://"),oPattern3("^https*://.+m3u8$"),oPattern4("^https*://"),oPattern5("^ftp://"),oPattern6("^(?:/[^/]*)*/*$");
	if (oPattern1.PartialMatch(tmp) || oPattern2.PartialMatch(tmp) || oPattern3.PartialMatch(tmp))
	{
		StreamResource *file_source = new StreamResource(tmp);
		strcpy(file_source->m_res_url ,tmp);
		return file_source;
	}
	else if (oPattern4.PartialMatch(tmp) || oPattern5.PartialMatch(tmp))
	{
		std::lock_guard<std::mutex> locked(mutex_res);
		map<string, FileResource*>::iterator map_it;
		map_it = m_file_res.find(url);
		if (map_it != m_file_res.end()) {
			while(map_it != m_file_res.end()) {
				FileResource* fp = map_it->second;
				if ((!fp->m_bdownloading) && fp->m_bstopdownload && fp->m_bhasdownload) {
					std::cout << "file is buffer, return now\n";
					return fp;
				}
				++map_it; 
			}			
		}

		FileResource* stream_source = new FileResource(tmp);
		m_file_res.insert( std::make_pair(url,stream_source));
		std::cout << "can't search file,create downloader............\n";
		return stream_source;
		
	}
	else if (oPattern6.PartialMatch(tmp))
	{
		return NULL;
	}
	return NULL;
}



std::string ResourceCreater::FindSourceUrl(const std::string& path){

	auto iter = std::find_if(m_file_res.begin(),m_file_res.end(),[&](std::map<std::string, FileResource*>::value_type &it)->bool{
		return (path == std::string((it.second)->m_res_path));
	});

	if(iter == m_file_res.end()){
		return std::string();
	}
//	std::cout << "ResourceCreater::FindSourceUrl path "<< path << " find result: "<< iter->first<<std::endl;
	return std::string(iter->first);
}

int ResourceCreater::RemoveResource(const std::string &url,ResourceCreater* removeRes){
	std::lock_guard<std::mutex> guard(mutex_res);
//	std::cout << "ResourceCreater::RemoveResource remove url "<< url<<std::endl;
	auto iter = m_file_res.find(url);
	for(;iter != m_file_res.end();iter++){
		if(iter->second == removeRes){
			ResourceCreater* res = iter->second;
			m_file_res.erase(iter);
			delete res;
			return 0;
		}
	}
	std::cout << "ResourceCreater::RemoveResource can't find remove target "<< url<<std::endl;
	return 0;
}

int ResourceCreater::getNetFileInfo(const char* url, double& size)
{
	int ret = 0;
	CURL* handle = curl_easy_init();
	if (!handle && !(handle = curl_easy_init())) {
		std::cout << "FileResource: err init curl\n";
		return -1;
	}

	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "GET");    //使用CURLOPT_CUSTOMREQUEST  
	curl_easy_setopt(handle, CURLOPT_NOBODY, 1);    //不需求body  
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);   //重定向
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10); // 连接10秒超时

	if (curl_easy_perform(handle) == CURLE_OK)
	{
		int ret = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
		if (ret == CURLE_OK)
			std::cout << "downlaod file size " << size << std::endl;
		else {
			size = -1;
			std::cout << "can't get http file size\n";
		}
	}
	else {
		ret = -1;
	}

	curl_easy_cleanup(handle);
	return ret;
}
int ResourceCreater::removeFile(const std::string&)
{
	return 0;
}


int ResourceCreater::ClearBuff()
{
	std::lock_guard<std::mutex> locked(mutex_res);
	std::string path;	
	for(auto val: m_file_res){
		path = val.second->getFilePath();
		delete (val.second);
		val.second = nullptr;
	}
	m_file_res.clear();

	remove_dir(path);
	return 0;
}


#define MAX 1 /* number of simultaneous transfers */
FileResource::FileResource(const char* url):
 	overFun(nullptr),percentFun(nullptr),run(false)
{
	memset(m_res_name,0,MAX_RES_PATH);
	memset(m_res_exp,0,10);
	strcpy(m_res_url,url);
	m_res_type = NETWORKFILE;
	m_res_totalsize = 0;
	m_bhasdownload = false;
	m_blocal  = false;
	m_bdownloading = false;
	m_bstopdownload = false;
	
}

FileResource::~FileResource()
{
//	run = false;
//	std::cout << "~FileResource stop download\n";
	stop();
	char cmd[1024] = {};
	snprintf(cmd, 1023, "rm -rf %s", m_res_path);
	system(cmd);
	std::cout << "remove file " << cmd << std::endl;
}

int FileResource::stop(){
	std::lock_guard<std::mutex>  lock(mutex_thread);
	run = false;
	if(hand_download){
		if(hand_download->joinable()){
			std::cout << "wait download thread quit____________________\n";
			hand_download->join();
		}
		delete hand_download;
		hand_download = nullptr;
	}	
	return 0;
}



int FileResource::AsyncDownload(DownloadOver over, CurrentProgress percent)
{
	if (m_bstopdownload && m_bhasdownload && !m_bdownloading)
		return 1;

//	if (!over || !percent)
//		return -1;

	if (getFileLength(m_res_url) == -1) {
		return -1;
	}
	overFun = over;
	percentFun = percent;

	Init();

	return 0;
}
int FileResource::valid(){
	std::cout << "SyncDownload:: wait download thread quit____________________\n";
	if (hand_download) {
		if (hand_download->joinable()) {
			std::cout << "SyncDownload:: wait download thread quit____________________\n";
			hand_download->join();
		}
		delete hand_download;
		hand_download = nullptr;
	}

	//判断是否下载完成
	if (!m_bstopdownload || !m_bhasdownload ||m_bdownloading) {
		return -1;
	}
	return 0;
}

int FileResource::SyncDownload()
{

	if (m_bstopdownload && m_bhasdownload && !m_bdownloading)
		return 0;

	//// 判断地址是否可用
	//if (getFileLength(m_res_url) == -1) 
	//	return -1;

	Init();

	std::cout << "SyncDownload:: wait download thread quit____________________\n";
	if (hand_download) {
		if (hand_download->joinable()) {
			hand_download->join();
		}
		delete hand_download;
		hand_download = nullptr;
	}

	//判断是否下载完成
	if (m_bstopdownload && m_bhasdownload && !m_bdownloading){
		return 0;
	}
	
	return -1;
}

int FileResource::GetResurceType(const char *url)
{
	string strPath = url;
	int pos = strPath.find_last_of('/');
	string str = strPath.substr(pos + 1, strPath.length() - 1 - pos);
	string strexp = str.substr(str.find_last_of('.') + 1, str.length() - 1 - str.find_last_of('.'));
	if (strexp == "jpg" || strexp == "JPG" ||
			strexp == "jpeg" || strexp == "JPEG" ||
				strexp == "png" || strexp == "PNG")
	{
		return 1;
	}
	else
		return 2;

	return -1;
}


void FileResource::Init()
{
	string res_url = m_res_url;
	int pos = res_url.find_last_of('.');
	strcpy(m_res_exp,res_url.substr(pos+1,res_url.length()).c_str());

	uuid_t uuid;
    char str_uuid[36];
    uuid_generate(uuid);
    uuid_unparse(uuid, str_uuid);
	strcpy(m_res_name, str_uuid);

//	char filePath[1024]={0};
//	getcwd(filePath,1024);
	string strPath = CachePath;
	char pid[16]={0};
    snprintf(pid, 16, "/%d/", getpid());
	strPath += pid;
	int isCreate = mkdir(strPath.c_str(),S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
	if( !isCreate){
		m_filePath = strPath;
		printf("create path:%s\n",strPath.c_str());
	}
	else{
		
		printf("create path failed! error code : %d %s \n",isCreate,strPath.c_str());
	}

	strPath += str_uuid;
	strPath += ".";
	strPath += m_res_exp;
	strcpy(m_res_path, strPath.c_str());
	m_bready = false;

	std::lock_guard<std::mutex>  lock(mutex_thread);
	if(!hand_download){
		run = true;
		hand_download = new std::thread([&](){this->exec();});
	}
}

void FileResource::initeasy(CURLM *cm, FILE *fl)
{
	CURL *eh = curl_easy_init();

	curl_easy_setopt(eh, CURLOPT_WRITEDATA, fl);
	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, write_file);
	curl_easy_setopt(eh, CURLOPT_CONNECTTIMEOUT, 10); // 连接10秒超时
	curl_easy_setopt(eh, CURLOPT_URL, m_res_url);
	curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1);   //重定向


	curl_multi_add_handle(cm, eh);
}

size_t FileResource::write_file(void *ptr,size_t size,size_t nmemb,FILE *stream)
{
	if (stream != NULL)
		return fwrite(ptr, size, nmemb, stream);
	else
		return 0;
}

int FileResource::getFileLength(const char* url)
{
	int ret = 0;
	CURL* handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST,"GET");   
	curl_easy_setopt(handle, CURLOPT_NOBODY, 1);			//不需求body  
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);	//重定向
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10);	// 连接10秒超时

	if (curl_easy_perform(handle) == CURLE_OK)
	{
		int ret = curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &m_res_totalsize);
		if(ret == CURLE_OK)
			std::cout << "downlaod file size " << m_res_totalsize << std::endl;
		else
			m_res_totalsize = -1;
	}
	else {
		ret = -1;
	}
	curl_easy_cleanup(handle);
	
	return ret;
}

int FileResource::exec()
{
//	std::cout << "prepare downloading........\n";
	av_usleep(1*1000*1000);	
	m_bdownloading = true;
	m_bstopdownload = false;
	m_bhasdownload = false;
	int max_fd = -1, msgs_left, still_running = -1;//still_running判断multi handler是否传输完毕
	fd_set fd_read, fd_write, fd_except;
	uint64_t lastfilesize = 0;
	uint64_t count = 0;
	struct timeval T;
	CURLM *cm;
	CURLMsg *msg;
	long curl_timeo;
	FILE *outfile;
	uint64_t filesize = 0;
	uint64_t ReportT = 0;

	curl_global_init(CURL_GLOBAL_ALL);
	cm = curl_multi_init();

	outfile = fopen(m_res_path, "wb");
//	std::cout << "write file "<< m_res_path<<std::endl;
	curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, (long)MAX);
	initeasy(cm, outfile);
	do 
	{
		curl_multi_perform(cm, &still_running);

		if (still_running && run) 
		{
			FD_ZERO(&fd_read);
			FD_ZERO(&fd_write);
			FD_ZERO(&fd_except);

			curl_multi_fdset(cm, &fd_read, &fd_write, &fd_except, &max_fd);

			curl_multi_timeout(cm, &curl_timeo);

			if (curl_timeo == -1)
				curl_timeo = 100;

			if (max_fd == -1) 
			{
				sleep((unsigned int)curl_timeo / 1000);
			}
			else 
			{
				T.tv_sec = curl_timeo / 1000;
				T.tv_usec = (curl_timeo % 1000) * 1000;

				if (0 > select(max_fd + 1, &fd_read, &fd_write, &fd_except, &T)) 
				{
					m_bstopdownload = false;
					fclose(outfile);
					outfile = NULL;
					if(overFun)
						overFun(false);
					return 1;
				}


				filesize = ftell(outfile);
				if (lastfilesize == filesize)
					count++;
				else
				{
					count = 0;
					lastfilesize = filesize;
				}
				if (count == 300) 
				{
					printf("retry 300 times, download fail - %s\n", m_res_url);
					fflush(stdout);
					fclose(outfile);
					outfile = NULL;
					if(overFun)
						overFun(false);
					return 1;
				}
			}
		}

		while ((msg = curl_multi_info_read(cm, &msgs_left)) && run) 
		{
			if (msg->msg == CURLMSG_DONE) 
			{
				char *url;
				CURL *e = msg->easy_handle;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
				fprintf(stderr, "R: %d - %s <%s>\n",
							msg->data.result, curl_easy_strerror(msg->data.result), url);
				if (msg->data.result == CURLE_OK)
				{   
					filesize = ftell(outfile);
					m_bstopdownload = true;
					m_bhasdownload = true;
					m_bready = true;
					printf("it had download.\n");
				}
				else
				{
					m_bstopdownload = false;
					printf("it download failed.\n");
				}
				fflush(stdout);

				curl_multi_remove_handle(cm, e);
				curl_easy_cleanup(e);
			}
			else
			{
				fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
			}
		}


		// 500ms   av_gettime
		if(percentFun && (av_gettime() - ReportT) > 500*1000){
			percentFun(m_res_totalsize, filesize);
			ReportT = av_gettime();
		}

	}while (still_running && run);

	
	curl_multi_cleanup(cm);
	curl_global_cleanup();
	
	if (run) {
		filesize = ftell(outfile);
		if(m_res_totalsize == -1)
			m_res_totalsize = filesize;

		if(percentFun)
			percentFun(m_res_totalsize, filesize);

		if(overFun)
			overFun(m_bhasdownload);

		m_bdownloading = false;
		m_bstopdownload = true;
	}

	if (outfile)
		fclose(outfile);
	run = false;
	std::cout << "FileResource:: download thread is quit\n";
	return 0;
}

StreamResource::StreamResource(const char* url)
{
	strcpy(m_res_url,url);
	m_res_type = NETWORKSTREAM;
}

StreamResource::~StreamResource()
{

}

int StreamResource::decode_interrupt_cb(void *ctx)
{
	time_t startTime = *(time_t *)ctx;
	time_t nowTime = time(NULL);
	int Timeout = nowTime - startTime;
	//printf("diff======%ld\n",nowTime - startTime);
	if(Timeout > 10)
	{
		return -1;
	}
	return 0;
}

int StreamResource::AsyncDownload(DownloadOver , CurrentProgress )
{
	AVFormatContext *pFormatCtx;
	pFormatCtx = avformat_alloc_context();
	pFormatCtx->interrupt_callback.callback = StreamResource::decode_interrupt_cb;
	time_t tm = time(NULL);
	pFormatCtx->interrupt_callback.opaque = &tm;
	if(avformat_open_input(&pFormatCtx,m_res_url,NULL,NULL)!=0)
	{
		return -1;
	}
	else
	{
		m_bvalid = true;
		return 1;
	}
	return 0;
}
