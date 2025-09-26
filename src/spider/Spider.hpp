#pragma once

#include <string>

#include <curl/curl.h>

class Spider {

public:
    static void crawlUrl(const std::string & url, const bool recursive, const int maxDepth, const std::string & savePath);

private:

};
