#include "Spider.hpp"

#include <filesystem>
#include <unistd.h>
#include <curl/curl.h>
#include <regex>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iostream>

static void verifieSavePath(const std::filesystem::path & savePath) {

    if (!std::filesystem::exists(savePath) && !std::filesystem::create_directories(savePath))
        throw std::runtime_error(std::string("Cannot create directory: ") + savePath.string());
    else if (!std::filesystem::is_directory(savePath))
        throw std::runtime_error(std::string("Save path is not a directory: ") + savePath.string());
    else if (!access(savePath.c_str(), W_OK) == 0)
        throw std::runtime_error(std::string("Write permission denied: ") + savePath.string());
}

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

static void downloadHtml(const std::string & url, std::string & html, CURLM * curlMultiHandle) {

    CURL * curlEasyHandle = curl_easy_init();

    if (curlEasyHandle) {
        curl_easy_setopt(curlEasyHandle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlEasyHandle, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curlEasyHandle, CURLOPT_WRITEDATA, &html);
        curl_multi_add_handle(curlMultiHandle, curlEasyHandle);
        curl_multi_perform(curlMultiHandle, 0);
        // if (curl_easy_perform(curlEasyHandle) == CURLE_OK) {

        //     long httpCode = 0;

        //     curl_easy_getinfo(curlEasyHandle, CURLINFO_RESPONSE_CODE, &httpCode);
        //     if (httpCode == 200)
        //         std::cout << "\033[1;32m[HTML] request '" << url << "' = " << httpCode << "\033[0m\n";
        //     else
        //         std::cerr << "\033[1;33m[HTML] request '" << url << "' = " << httpCode << "\033[0m\n";
        // }
        // else
        //     std::cerr << "\033[1;31m" << "[HTML] request error '" << url << "'\033[0m\n";
    }

}

static void extractLinks(std::pair<std::string, std::string> & html, std::vector<std::string> & imageUrls, std::unordered_map<std::string, std::string> & urls) {

    std::regex img_re("<img[^>]*src=[\"']([^\"'>]+)[\"']");
    std::regex a_re("<a[^>]*href=[\"']([^\"'>]+)[\"']");
    std::smatch match;
    std::string currentUrl, tmp;

    tmp = html.second;
    while (std::regex_search(tmp, match, img_re)) {

        currentUrl = match[1];
        if (!currentUrl.empty() && currentUrl[0] == '/')
            currentUrl = getSchemeHost(html.first) + currentUrl;
        if (isImage(match[1]))
            imageUrls.push_back(currentUrl);
        tmp = match.suffix();
    }

    tmp = html.second;
    while (std::regex_search(tmp, match, a_re)) {

        currentUrl = match[1];
        if (!currentUrl.empty() && currentUrl[0] == '/')
            currentUrl = getSchemeHost(html.first) + currentUrl;
        urls[currentUrl] = "";
        tmp = match.suffix();
    }
}

static std::string getHost(const std::string & url) {

    std::regex re(R"(^\w+://([^/:]+))");
    std::smatch match;

    if (std::regex_search(url, match, re))
        return match[1].str();
    return "unknown Host";
}

static void downloadImage(const std::string & url, const std::filesystem::path & savePath, std::vector<FILE * > & fps, CURLM * curlMultiHandle) {

    CURL * curl = curl_easy_init();

    if (curl) {

        std::filesystem::path filename = url.substr(url.find_last_of("/\\") + 1);
        std::filesystem::path filepath = savePath / getHost(url);

        std::filesystem::create_directories(filepath);
        filepath /= filename;

        if (!access(filepath.c_str(), F_OK)) {

            curl_easy_cleanup(curl);
            return ;
        }

        FILE * fp = fp = fopen(filepath.c_str(), "wb");
        if (!fp) {

            curl_easy_cleanup(curl);
            std::cerr << "\033[1;31m" << "[IMAGE] Cannot write to file: '" << filepath.string() << "'\033[0m\n";
            return ;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_multi_add_handle(curlMultiHandle, curl);
        curl_multi_perform(curlMultiHandle, 0);
        fps.emplace_back(fp);
    }
}

static void printDownloadProgress(CURLM * curlMultiHandle) {

    int msgq = 0;
    struct CURLMsg * m = curl_multi_info_read(curlMultiHandle, &msgq);

    while (m) {

        if(m->msg == CURLMSG_DONE) {

            char * urlp;

            curl_easy_getinfo(m->easy_handle, CURLINFO_EFFECTIVE_URL, &urlp);
            if (!m->data.result)
                std::cout << "\033[1;32m[IMAGE] request '" << urlp << "\033[0m\n";
            else
                std::cerr << "\033[1;33m[IMAGE] request '" << urlp << "' = " << m->data.result << "\033[0m\n";
            curl_multi_remove_handle(curlMultiHandle, m->easy_handle);
            curl_easy_cleanup(m->easy_handle);
        }
        m = curl_multi_info_read(curlMultiHandle, &msgq);
    }
}

static void processHtmls(CURLM * curlMultiHandle, std::vector<std::string> & imageUrls, std::unordered_map<std::string, std::string> & urlsToVisit) {

    int msgq = 0;
    struct CURLMsg * m = curl_multi_info_read(curlMultiHandle, &msgq);

    while (m) {

        if(m->msg == CURLMSG_DONE) {

            char * urlp;
            std::pair<std::string, std::string> html;

            curl_easy_getinfo(m->easy_handle, CURLINFO_EFFECTIVE_URL, &urlp);
            html = {urlp, urlsToVisit[urlp]};
            extractLinks(html, imageUrls, urlsToVisit);
            curl_multi_remove_handle(curlMultiHandle, m->easy_handle);
            curl_easy_cleanup(m->easy_handle);
        }
        m = curl_multi_info_read(curlMultiHandle, &msgq);
    }
}

void Spider::crawlUrl(const std::string & url, const bool recursive, const int maxDepth, const std::string & savePath) {

    verifieSavePath(savePath);

    std::unordered_set<std::string> visitedUrls;
    std::unordered_map<std::string, std::string> urlsToVisit;
    std::unordered_set<std::string> visitedImageUrls;
    std::vector<std::string> imageUrls;

    CURLM * curlMultiHandle = curl_multi_init();
    CURLM * curlMultiExtractor = curl_multi_init();
    std::vector<FILE * > fps;

    urlsToVisit[url] = "";
    for (int i = 0; (i < 1) || (recursive && i < maxDepth); i++) {

        for (auto [key, value] : urlsToVisit)
            downloadHtml(key, value, curlMultiExtractor);

        processHtmls(curlMultiExtractor, imageUrls, urlsToVisit);
        // while (!urlsToVisit.empty()) {

        //     std::string currentUrl = currentUrls.back();

        //     currentUrls.pop_back();
        //     if (visitedUrls.find(currentUrl) != visitedUrls.end())
        //         continue ;
        //     visitedUrls.insert(currentUrl);
        //     extractLinks(currentUrl, imageUrls, urlsToVisit);
        // }
        while (!imageUrls.empty()) {

            std::string currentImage = imageUrls.back();

            imageUrls.pop_back();
            if (visitedImageUrls.find(currentImage) != visitedImageUrls.end())
                continue ;
            visitedImageUrls.insert(currentImage);
            downloadImage(currentImage, savePath, fps, curlMultiHandle);
            printDownloadProgress(curlMultiHandle);
        }
    }

    for (FILE * fp : fps)
        fclose(fp);
    curl_multi_cleanup(curlMultiHandle);
    curl_easy_cleanup(curlMultiExtractor);
}
