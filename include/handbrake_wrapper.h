#pragma once

#include <string>
#include <functional>
#include <future>
#include <optional>

namespace bluray {

struct EncodeProgress {
    std::string input_file;
    std::string output_file;
    double percentage;        // 0.0 to 100.0
    double fps;
    double avg_fps;
    std::string eta;          // e.g., "00:15:32"
    std::string status_message;
};

using EncodeCallback = std::function<void(const EncodeProgress&)>;

class HandBrakeWrapper {
public:
    HandBrakeWrapper();
    
    // Start encoding asynchronously with custom parameters
    std::future<bool> encode(
        const std::string& input_file,
        const std::string& output_file,
        int title_number,           // Title number to encode from the MKV
        const std::string& encoder, // e.g., "nvenc_h265"
        const std::string& encoder_preset, // e.g., "slow"
        int quality,                // CRF/quality value (e.g., 22)
        EncodeCallback callback
    );
    
    // Check if HandBrakeCLI is installed
    static bool is_available();
    
    // Get HandBrakeCLI version
    static std::optional<std::string> get_version();
    
    // List available presets
    static std::vector<std::string> list_presets();
    
private:
    bool execute_handbrake(
        const std::string& input_file,
        const std::string& output_file,
        int title_number,
        const std::string& encoder,
        const std::string& encoder_preset,
        int quality,
        EncodeCallback callback
    );
    
    std::optional<EncodeProgress> parse_json_progress(const std::string& json_line);
};

} // namespace bluray
