#include "disc_detector.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <array>
#include <memory>
#include <sstream>
#include <regex>
#include <iostream>
#include <map>

namespace bluray {

namespace {
    // Helper function to parse size string (e.g., "20.3 GB") to bytes for sorting
    double parse_size_to_bytes(const std::string& size_str) {
        std::regex size_regex(R"regex((\d+\.?\d*)\s*(GB|MB|KB|B))regex");
        std::smatch match;

        if (std::regex_search(size_str, match, size_regex)) {
            double value = std::stod(match[1].str());
            std::string unit = match[2].str();

            if (unit == "GB") return value * 1024 * 1024 * 1024;
            if (unit == "MB") return value * 1024 * 1024;
            if (unit == "KB") return value * 1024;
            return value;
        }
        return 0.0;
    }
}

DiscDetector::DiscDetector() = default;

std::vector<DiscInfo> DiscDetector::scan_drives() {
    std::vector<DiscInfo> discs;
    
    // Find optical drives in /dev
    auto drives = find_optical_drives();
    
    for (const auto& drive : drives) {
        DiscInfo info;
        info.device_path = drive;
        info.has_disc = false;
        
        // Check if disc is present
        // In a real implementation, you'd use ioctl or check /proc/sys/dev/cdrom/info
        // For now, just check if device is readable
        std::ifstream test(drive);
        if (test.good()) {
            info.has_disc = true;
            // Try to read volume name from disc
            // This would typically use libcdio or similar
            info.volume_name = "Unknown Disc";
            info.disc_type = "Blu-ray"; // Would detect actual type
        } else {
            info.volume_name = "No Disc";
            info.disc_type = "Empty";
        }
        
        discs.push_back(info);
    }
    
    return discs;
}

std::optional<std::vector<Title>> DiscDetector::get_disc_titles(
    const std::string& device_path) {

    // Map device path to disc index for makemkvcon
    // For now, use disc:0 as we typically have one disc at a time
    std::string disc_spec = "disc:0";

    // Build command: makemkvcon -r info disc:0
    std::string command = "makemkvcon -r info " + disc_spec + " 2>&1";

    // Execute command and capture output
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        std::cerr << "Failed to run makemkvcon" << std::endl;
        return std::nullopt;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Parse the output
    std::vector<Title> titles;
    std::istringstream stream(result);
    std::string line;

    // MakeMKV output format:
    // TCOUNT:<number of titles>
    // TINFO:<title_index>,<attribute_id>,<attribute_code>,"<value>"
    // Example attributes: 2=description, 8=chapters, 9=duration, 10=size

    std::map<int, Title> title_map;

    while (std::getline(stream, line)) {
        if (line.find("TINFO:") == 0) {
            // Parse TINFO line
            std::regex tinfo_regex(R"regex(TINFO:(\d+),(\d+),(\d+),"([^"]*)")regex");
            std::smatch match;

            if (std::regex_search(line, match, tinfo_regex)) {
                int title_idx = std::stoi(match[1]);
                int attr_id = std::stoi(match[2]);
                std::string value = match[4];

                // Ensure title exists in map
                if (title_map.find(title_idx) == title_map.end()) {
                    Title t;
                    t.index = title_idx;
                    t.duration = "Unknown";
                    t.size = "Unknown";
                    t.chapters = 0;
                    t.description = "";
                    title_map[title_idx] = t;
                }

                // Parse different attributes
                if (attr_id == 2) {
                    title_map[title_idx].description = value;
                } else if (attr_id == 8) {
                    try {
                        title_map[title_idx].chapters = std::stoi(value);
                    } catch (...) {}
                } else if (attr_id == 9) {
                    title_map[title_idx].duration = value;
                } else if (attr_id == 10) {
                    title_map[title_idx].size = value;
                }
            }
        }
    }

    // Convert map to vector
    for (const auto& [idx, title] : title_map) {
        titles.push_back(title);
    }

    // Sort by size descending (largest first)
    std::sort(titles.begin(), titles.end(),
        [](const Title& a, const Title& b) {
            return parse_size_to_bytes(a.size) > parse_size_to_bytes(b.size);
        });

    if (titles.empty()) {
        return std::nullopt;
    }

    return titles;
}

std::vector<std::string> DiscDetector::find_optical_drives() {
    std::vector<std::string> drives;
    
    // Check common optical drive device paths
    const std::vector<std::string> possible_drives = {
        "/dev/sr0", "/dev/sr1", "/dev/sr2",
        "/dev/cdrom", "/dev/dvd", "/dev/bluray"
    };
    
    for (const auto& drive : possible_drives) {
        if (std::filesystem::exists(drive)) {
            drives.push_back(drive);
        }
    }
    
    return drives;
}

} // namespace bluray
