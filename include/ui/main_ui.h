#pragma once

#include "ftxui/component/component.hpp"
#include "disc_detector.h"
#include "makemkv_wrapper.h"
#include "handbrake_wrapper.h"
#include <memory>
#include <vector>

namespace bluray::ui {

enum class AppState {
    SCANNING,
    DISC_SELECTION,
    TITLE_SELECTION,
    RIPPING,
    ENCODING,
    COMPLETED
};

class MainUI {
public:
    MainUI();
    
    // Run the main UI loop
    void run();
    
private:
    // UI Components
    ftxui::Component create_disc_selector();
    ftxui::Component create_title_selector();
    ftxui::Component create_progress_view();
    ftxui::Component create_log_viewer();
    
    // State management
    AppState current_state_;
    std::vector<DiscInfo> available_discs_;
    std::vector<Title> available_titles_;
    std::vector<bool> selected_titles_;
    int selected_disc_index_ = 0;
    int selected_title_index_ = 0;
    
    // Progress tracking
    RipProgress current_rip_progress_;
    EncodeProgress current_encode_progress_;
    std::vector<std::string> log_messages_;
    
    // Wrappers
    std::unique_ptr<DiscDetector> disc_detector_;
    std::unique_ptr<MakeMKVWrapper> makemkv_;
    std::unique_ptr<HandBrakeWrapper> handbrake_;
    
    // UI state
    std::string output_directory_ = "./output";
    std::string handbrake_preset_ = "Fast 1080p30";
    
    // Helper methods
    void add_log(const std::string& message);
    void scan_for_discs();
    void load_disc_titles();
    void start_ripping();
    void start_encoding(const std::string& mkv_file);
};

} // namespace bluray::ui
