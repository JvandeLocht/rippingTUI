#include "disc_detector.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace bluray {

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
    
    // In a real implementation, this would call:
    // makemkvcon -r info disc:0
    // and parse the output to get title information
    
    // For now, return dummy data
    std::vector<Title> titles;
    
    Title t1;
    t1.index = 0;
    t1.duration = "1:45:23";
    t1.size = "25.4 GB";
    t1.chapters = 12;
    t1.description = "Main movie";
    titles.push_back(t1);
    
    Title t2;
    t2.index = 1;
    t2.duration = "0:15:32";
    t2.size = "2.1 GB";
    t2.chapters = 1;
    t2.description = "Bonus feature";
    titles.push_back(t2);
    
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
