#include "Spider.hpp"

#include <iostream>
#include <unordered_set>

void Spider::crawlUrl(const std::string & url, bool recursive, int maxDepth, const std::string & savePath) {

    std::unordered_set<std::string> visitedUrls;

    std::cout << "r = " << recursive << ", l = " << maxDepth << ", p = " << savePath << ", url = " << url << "\n"; 
}
