#pragma once

#include <string>
#include <vector>
#include <optional>

namespace bluray {

struct DiscInfo {
    std::string device_path;  // e.g., /dev/sr0
    std::string volume_name;
    std::string disc_type;    // "Blu-ray", "DVD", etc.
    bool has_disc;
};

struct Title {
    int index;
    std::string duration;     // e.g., "1:45:23"
    std::string size;         // e.g., "25.4 GB"
    int chapters;
    std::string description;
};

class DiscDetector {
public:
    DiscDetector();
    
    // Scan for available optical drives
    std::vector<DiscInfo> scan_drives();
    
    // Get detailed info about disc in specific drive
    std::optional<std::vector<Title>> get_disc_titles(const std::string& device_path);
    
private:
    std::vector<std::string> find_optical_drives();
};

} // namespace bluray
