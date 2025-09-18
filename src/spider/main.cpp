#include "Spider.hpp"

#include <iostream>
#include <cxxopts.hpp>

int main(int ac, char ** av) {

    std::string savePath = "./data/";
    std::string url;
    std::string arg;
    int         maxDepth = 5;
    bool        recursive = false;

    try {

        cxxopts::Options options(av[0], "online image crawler");

        options.custom_help("[OPTION...] URL");
        options.parse_positional({"URL"});
        options.add_options()
            ("r,recursive", "enable recursive crawl")
            ("l,length", "max recursion depth", cxxopts::value<int>()->default_value("5"))
            ("p,path", "path to save images", cxxopts::value<std::string>()->default_value("./data"))
            ("h,help", "display this help and exit")
            ("URL", "URL to crawl", cxxopts::value<std::string>());

        if (ac < 2 || ac > 7)
            std::cout << options.help() << std::endl;
        cxxopts::ParseResult result = options.parse(ac, av);

        if (result.count("h")) {

            std::cout << options.help() << std::endl;
            return (0);
        }

        if (!result.count("URL")) {

            std::cerr << "Error: URL is required\n";
            std::cout << options.help() << std::endl;
            return (1);
        }

        recursive = result["recursive"].as<bool>();
        maxDepth = result["length"].as<int>();
        savePath = result["path"].as<std::string>();
        url = result["URL"].as<std::string>();
    }
    catch (const std::exception & e) {

        std::cerr << "Error parsing options: " << e.what() << std::endl;
        return (1);
    }

    Spider::crawlUrl(url, recursive, maxDepth, savePath);

    return (0);
}
