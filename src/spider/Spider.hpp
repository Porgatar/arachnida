#pragma once

#include <string>

class Spider {

public:
    static void crawlUrl(const std::string & url, const bool recursive, const int maxDepth, const std::string & savePath);

private:

};
