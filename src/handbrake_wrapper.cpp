#include "handbrake_wrapper.h"
#include <cstdio>
#include <array>
#include <thread>
#include <regex>
#include <sstream>
#include <iomanip>

namespace bluray {

HandBrakeWrapper::HandBrakeWrapper() = default;

bool HandBrakeWrapper::is_available() {
    FILE* pipe = popen("which HandBrakeCLI 2>/dev/null", "r");
    if (!pipe) return false;
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int status = pclose(pipe);
    return status == 0 && !result.empty();
}

std::optional<std::string> HandBrakeWrapper::get_version() {
    FILE* pipe = popen("HandBrakeCLI --version 2>&1", "r");
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

std::vector<std::string> HandBrakeWrapper::list_presets() {
    std::vector<std::string> presets;
    
    FILE* pipe = popen("HandBrakeCLI --preset-list 2>&1", "r");
    if (!pipe) return presets;
    
    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line(buffer.data());
        
        // Parse preset names from output
        // Format: "    + Preset Name"
        if (line.find("    + ") != std::string::npos) {
            size_t pos = line.find("    + ");
            std::string preset = line.substr(pos + 6);
            // Remove newline
            if (!preset.empty() && preset.back() == '\n') {
                preset.pop_back();
            }
            presets.push_back(preset);
        }
    }
    
    pclose(pipe);
    return presets;
}

std::future<bool> HandBrakeWrapper::encode(
    const std::string& input_file,
    const std::string& output_file,
    int title_number,
    const std::string& encoder,
    const std::string& encoder_preset,
    int quality,
    EncodeCallback callback) {

    return std::async(std::launch::async, [=, this]() {
        return execute_handbrake(input_file, output_file, title_number,
                                 encoder, encoder_preset, quality, callback);
    });
}

bool HandBrakeWrapper::execute_handbrake(
    const std::string& input_file,
    const std::string& output_file,
    int title_number,
    const std::string& encoder,
    const std::string& encoder_preset,
    int quality,
    EncodeCallback callback) {

    // Build command with custom parameters matching user's requirements:
    // HandBrakeCLI -i input.mkv -o output.mkv -e nvenc_h265 --encoder-preset slow
    // -q 22 -m --subtitle scan -F --subtitle-burned --all-audio --title N --json
    std::string cmd = "HandBrakeCLI -i \"" + input_file +
                      "\" -o \"" + output_file +
                      "\" -e " + encoder +
                      " --encoder-preset " + encoder_preset +
                      " -q " + std::to_string(quality) +
                      " -m" +  // Chapter markers
                      " --subtitle scan -F" +  // Scan for forced subtitles
                      " --subtitle-burned" +  // Burn in subtitles
                      " --all-audio" +  // Include all audio tracks
                      " --title " + std::to_string(title_number) +
                      " --json 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    std::array<char, 1024> buffer;
    EncodeProgress progress;
    progress.input_file = input_file;
    progress.output_file = output_file;
    progress.percentage = 0.0;
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line(buffer.data());
        
        // HandBrake with --json outputs progress as:
        // {"Progress": {"Working": 1, "Percent": 45.5, "Rate": 123.4, ...}}
        
        if (line.find("\"Progress\"") != std::string::npos) {
            auto parsed = parse_json_progress(line);
            if (parsed) {
                progress = *parsed;
                progress.input_file = input_file;
                progress.output_file = output_file;
                callback(progress);
            }
        }
        
        // Also handle non-JSON progress output for older versions
        // Format: Encoding: task 1 of 1, 45.23 % (123.45 fps, avg 120.12 fps, ETA 00h15m32s)
        std::regex progress_regex(R"((\d+\.\d+) %.*?(\d+\.\d+) fps.*?avg (\d+\.\d+) fps.*?ETA (\d+h\d+m\d+s))");
        std::smatch match;
        
        if (std::regex_search(line, match, progress_regex)) {
            progress.percentage = std::stod(match[1]);
            progress.fps = std::stod(match[2]);
            progress.avg_fps = std::stod(match[3]);
            progress.eta = match[4];
            progress.status_message = "Encoding: " + 
                std::to_string(static_cast<int>(progress.percentage)) + "%";
            callback(progress);
        }
    }
    
    int status = pclose(pipe);
    return status == 0;
}

std::optional<EncodeProgress> HandBrakeWrapper::parse_json_progress(
    const std::string& json_line) {
    
    // Simple JSON parsing for progress
    // In a real implementation, use a proper JSON library like nlohmann/json
    
    EncodeProgress progress;
    
    // Extract percentage
    std::regex percent_regex(R"("Percent":\s*(\d+\.?\d*))");
    std::smatch match;
    if (std::regex_search(json_line, match, percent_regex)) {
        progress.percentage = std::stod(match[1]);
    }
    
    // Extract rate (fps)
    std::regex rate_regex(R"("Rate":\s*(\d+\.?\d*))");
    if (std::regex_search(json_line, match, rate_regex)) {
        progress.fps = std::stod(match[1]);
    }
    
    // Extract average rate
    std::regex avg_regex(R"("RateAvg":\s*(\d+\.?\d*))");
    if (std::regex_search(json_line, match, avg_regex)) {
        progress.avg_fps = std::stod(match[1]);
    }
    
    // Extract ETA
    std::regex eta_regex(R"("ETASeconds":\s*(\d+))");
    if (std::regex_search(json_line, match, eta_regex)) {
        int seconds = std::stoi(match[1]);
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << secs;
        progress.eta = oss.str();
    }
    
    progress.status_message = "Encoding: " + 
        std::to_string(static_cast<int>(progress.percentage)) + "%";
    
    return progress;
}

} // namespace bluray
