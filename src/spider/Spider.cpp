#include "Spider.hpp"

#include <filesystem>
#include <unistd.h>
#include <curl/curl.h>
#include <regex>
#include <vector>
#include <unordered_set>
#include <iostream>

static size_t writeCallback(void * contents, size_t size, size_t nmemb, void * userp) {

    std::string * str = static_cast<std::string *>(userp);
    str->append(static_cast<char *>(contents), size * nmemb);
    return (size * nmemb);
}

static std::string getSchemeHost(const std::string & url) {

    std::regex re(R"(^(\w+://[^/]+))");
    std::smatch match;

    if (std::regex_search(url, match, re))
        return match[1];
    return ("");
}

static bool isImage(const std::string & filename) {

    std::string imageTypes[] = {".jpg", ".jpeg", ".png", ".gif", ".bmp"};
    std::string exts = std::filesystem::path(filename).extension().string();

    for (const std::string & current : imageTypes)
        if (current == exts)
            return (1);

    return (0);
}

static void extractLinks(const std::string & url, std::vector<std::string> & imageUrls, std::vector<std::string> & urls) {

    std::regex img_re("<img[^>]*src=[\"']([^\"'>]+)[\"']");
    std::regex a_re("<a[^>]*href=[\"']([^\"'>]+)[\"']");
    std::smatch match;
    std::string html, temp;

    CURL * curl = curl_easy_init();

    if (curl) {

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
        if (curl_easy_perform(curl) == CURLE_OK) {

            long httpCode = 0;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (httpCode == 200)
                std::cout << "\033[1;32m[HTML] request '" << url << "' = " << httpCode << "\033[0m\n";
            else
                std::cerr << "\033[1;33m[HTML] request '" << url << "' = " << httpCode << "\033[0m\n";
        }
        else
            std::cerr << "\033[1;31m" << "[HTML] request error '" << url << "'\033[0m\n";
        curl_easy_cleanup(curl);        
    }

    std::string currentUrl;
    temp = html;
    while (std::regex_search(temp, match, img_re)) {

        currentUrl = match[1];
        if (!currentUrl.empty() && currentUrl[0] == '/')
            currentUrl = getSchemeHost(url) + currentUrl;
        if (isImage(match[1]))
            imageUrls.push_back(currentUrl);
        temp = match.suffix();
    }

    temp = html;
    while (std::regex_search(temp, match, a_re)) {

        currentUrl = match[1];
        if (!currentUrl.empty() && currentUrl[0] == '/')
            currentUrl = getSchemeHost(url) + currentUrl;
        urls.push_back(currentUrl);
        temp = match.suffix();
    }
}

static std::string getHost(const std::string & url) {

    std::regex re(R"(^\w+://([^/:]+))");
    std::smatch match;

    if (std::regex_search(url, match, re))
        return match[1].str();
    return "unknown Host";
}

static void verifieSavePath(const std::filesystem::path & savePath) {

    if (!std::filesystem::exists(savePath) && !std::filesystem::create_directories(savePath))
        throw std::runtime_error(std::string("Cannot create directory: ") + savePath.string());
    else if (!std::filesystem::is_directory(savePath))
        throw std::runtime_error(std::string("Save path is not a directory: ") + savePath.string());
    else if (!access(savePath.c_str(), W_OK) == 0)
        throw std::runtime_error(std::string("Write permission denied: ") + savePath.string());
}

static void download_image(const std::string & url, const std::filesystem::path & savePath) {

    CURL * curl = curl_easy_init();

    if (curl) {

        std::filesystem::path filename = url.substr(url.find_last_of("/\\") + 1);
        std::filesystem::path filepath = savePath / getHost(url);

        verifieSavePath(filepath);
        filepath /= filename;

        FILE * fp = fopen(filepath.c_str(), "wb");

        if (!fp) {

            curl_easy_cleanup(curl);
            std::cerr << "\033[1;31m" << "[IMAGE] Cannot write to file: '" << filepath.string() << "'\033[0m\n";
            return ;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        if (curl_easy_perform(curl) == CURLE_OK) {

            long httpCode = 0;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (httpCode == 200)
                std::cout << "\033[1;32m[IMAGE] request '" << url << "' = " << httpCode << "\033[0m\n";
            else
                std::cerr << "\033[1;33m[IMAGE] request '" << url << "' = " << httpCode << "\033[0m\n";
        }
        else
            std::cerr << "\033[1;31m" << "[IMAGE] request error '" << url << "'\033[0m\n";
        fclose(fp);
        curl_easy_cleanup(curl);
    }
}

void Spider::crawlUrl(const std::string & url, const bool recursive, const int maxDepth, const std::string & savePath) {

    verifieSavePath(savePath);

    std::unordered_set<std::string> visitedUrls;
    std::vector<std::string> urlsToVisit;
    std::unordered_set<std::string> visitedImageUrls;
    std::vector<std::string> imageUrls;

    urlsToVisit.push_back(url);
    for (int i = 0; (i < 1) || (recursive && i < maxDepth); i++) {

        std::cout << "-\t-\t-\t-\t-\tdepth = " << i << "\t-\t-\t-\t-\t-\n";
        std::vector<std::string> currentUrls = urlsToVisit;

        urlsToVisit.clear();
        while (!currentUrls.empty()) {

            std::string currentUrl = currentUrls.back();

            currentUrls.pop_back();
            if (visitedUrls.find(currentUrl) != visitedUrls.end())
                continue ;
            visitedUrls.insert(currentUrl);
            extractLinks(currentUrl, imageUrls, urlsToVisit);
            while (!imageUrls.empty()) {
    
                std::string currentImage = imageUrls.back();
    
                imageUrls.pop_back();
                if (visitedImageUrls.find(currentImage) != visitedImageUrls.end())
                    continue ;
                visitedImageUrls.insert(currentImage);
                download_image(currentImage, savePath);
            }
        }
    }
}
