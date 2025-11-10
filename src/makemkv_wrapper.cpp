#include "makemkv_wrapper.h"
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <regex>
#include <sstream>

namespace bluray {

MakeMKVWrapper::MakeMKVWrapper() = default;

bool MakeMKVWrapper::is_available() {
    // Check if makemkvcon is in PATH
    FILE* pipe = popen("which makemkvcon 2>/dev/null", "r");
    if (!pipe) return false;
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int status = pclose(pipe);
    return status == 0 && !result.empty();
}

std::optional<std::string> MakeMKVWrapper::get_version() {
    FILE* pipe = popen("makemkvcon --version 2>&1", "r");
    if (!pipe) return std::nullopt;
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    
    if (!result.empty()) {
        return result;
    }
    return std::nullopt;
}

std::future<bool> MakeMKVWrapper::rip_titles(
    const std::string& device_path,
    const std::vector<int>& title_indices,
    const std::string& output_dir,
    ProgressCallback callback) {
    
    return std::async(std::launch::async, [=, this]() {
        bool success = true;
        
        for (size_t i = 0; i < title_indices.size(); ++i) {
            RipProgress progress;
            progress.current_title = i + 1;
            progress.total_titles = title_indices.size();
            progress.percentage = 0.0;
            progress.status_message = "Ripping title " + 
                std::to_string(title_indices[i]);
            
            callback(progress);
            
            bool result = execute_makemkv(
                device_path, 
                title_indices[i], 
                output_dir,
                callback
            );
            
            if (!result) {
                success = false;
                break;
            }
        }
        
        return success;
    });
}

bool MakeMKVWrapper::execute_makemkv(
    const std::string& device_path,
    int title_index,
    const std::string& output_dir,
    ProgressCallback callback) {
    
    // Build command: makemkvcon mkv disc:0 <title_index> <output_dir>
    // Note: disc:0 assumes first drive, you'd map device_path properly
    std::string cmd = "makemkvcon mkv disc:0 " + 
                      std::to_string(title_index) + " " +
                      output_dir + " 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    std::array<char, 256> buffer;
    RipProgress progress;
    progress.current_title = 1;
    progress.total_titles = 1;
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line(buffer.data());
        
        // MakeMKV outputs progress in format:
        // PRGV:1000,0,1048576
        // PRGV:current,min,max
        // or PRGT:n,n,message
        
        if (line.find("PRGV:") != std::string::npos) {
            // Parse progress
            std::regex progress_regex(R"(PRGV:(\d+),(\d+),(\d+))");
            std::smatch match;
            
            if (std::regex_search(line, match, progress_regex)) {
                long current = std::stol(match[1]);
                long max = std::stol(match[3]);
                
                if (max > 0) {
                    progress.percentage = (current * 100.0) / max;
                    progress.status_message = "Progress: " + 
                        std::to_string(static_cast<int>(progress.percentage)) + "%";
                    callback(progress);
                }
            }
        } else if (line.find("PRGT:") != std::string::npos) {
            // Parse status message
            size_t pos = line.find("PRGT:");
            if (pos != std::string::npos) {
                progress.status_message = line.substr(pos + 5);
                callback(progress);
            }
        }
    }
    
    int status = pclose(pipe);
    return status == 0;
}

std::string MakeMKVWrapper::parse_progress_line(const std::string& line) {
    // Helper to extract meaningful info from MakeMKV output
    return line;
}

} // namespace bluray
