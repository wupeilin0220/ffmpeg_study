#include "Serial.h"
FILE* Serial::fp = nullptr;
std::mutex Serial::mutex_syncFile;
std::string Serial::path;
