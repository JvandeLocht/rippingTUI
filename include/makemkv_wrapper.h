#pragma once

#include <string>
#include <functional>
#include <future>
#include <optional>

namespace bluray {

struct RipProgress {
    int current_title;
    int total_titles;
    double percentage;      // 0.0 to 100.0
    std::string current_file;
    std::string status_message;
};

using ProgressCallback = std::function<void(const RipProgress&)>;

class MakeMKVWrapper {
public:
    MakeMKVWrapper();
    
    // Start ripping selected titles asynchronously
    std::future<bool> rip_titles(
        const std::string& device_path,
        const std::vector<int>& title_indices,
        const std::string& output_dir,
        ProgressCallback callback
    );
    
    // Check if MakeMKV is installed
    static bool is_available();
    
    // Get MakeMKV version
    static std::optional<std::string> get_version();
    
private:
    std::string parse_progress_line(const std::string& line);
    bool execute_makemkv(
        const std::string& device_path,
        int title_index,
        const std::string& output_dir,
        ProgressCallback callback
    );
};

} // namespace bluray
