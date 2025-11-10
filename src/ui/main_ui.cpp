#include "ui/main_ui.h"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include <chrono>
#include <thread>

using namespace ftxui;

namespace bluray::ui {

MainUI::MainUI() 
    : current_state_(AppState::SCANNING),
      disc_detector_(std::make_unique<DiscDetector>()),
      makemkv_(std::make_unique<MakeMKVWrapper>()),
      handbrake_(std::make_unique<HandBrakeWrapper>()) {
    
    add_log("Blu-ray Ripper initialized");
    
    // Check if tools are available
    if (!MakeMKVWrapper::is_available()) {
        add_log("WARNING: MakeMKV not found in PATH");
    }
    if (!HandBrakeWrapper::is_available()) {
        add_log("WARNING: HandBrakeCLI not found in PATH");
    }
}

void MainUI::run() {
    auto screen = ScreenInteractive::Fullscreen();
    
    // Main container component
    auto main_container = Container::Vertical({});
    
    // Title bar
    auto title = Renderer([] {
        return vbox({
            text("Blu-ray Ripper") | bold | center,
            separator()
        });
    });
    
    // Status section
    auto status = Renderer([this] {
        std::string state_text;
        switch (current_state_) {
            case AppState::SCANNING: state_text = "Scanning for discs..."; break;
            case AppState::DISC_SELECTION: state_text = "Select disc"; break;
            case AppState::TITLE_SELECTION: state_text = "Select titles to rip"; break;
            case AppState::RIPPING: state_text = "Ripping disc..."; break;
            case AppState::ENCODING: state_text = "Encoding..."; break;
            case AppState::COMPLETED: state_text = "Completed!"; break;
        }
        
        return vbox({
            hbox({
                text("Status: ") | bold,
                text(state_text)
            }),
            separator()
        });
    });
    
    // Disc selector
    std::vector<std::string> disc_entries;
    auto disc_menu = Menu(&disc_entries, &selected_disc_index_);
    
    auto disc_selector = Renderer(disc_menu, [this, &disc_entries, disc_menu] {
        // Update disc entries
        disc_entries.clear();
        for (const auto& disc : available_discs_) {
            disc_entries.push_back(
                disc.volume_name + " (" + disc.device_path + ")"
            );
        }
        
        if (available_discs_.empty()) {
            return vbox({
                text("No discs detected") | dim,
                text("Insert a Blu-ray disc and press 'r' to rescan") | dim
            });
        }
        
        return vbox({
            text("Available Discs:") | bold,
            separator(),
            disc_menu->Render() | frame | size(HEIGHT, LESS_THAN, 10)
        });
    });
    
    // Title selector with checkboxes
    std::vector<Component> title_checkboxes;
    
    auto title_selector = Renderer([this] {
        if (available_titles_.empty()) {
            return text("No titles loaded") | dim;
        }
        
        Elements title_elements;
        title_elements.push_back(text("Select titles to rip:") | bold);
        title_elements.push_back(separator());
        
        for (size_t i = 0; i < available_titles_.size(); ++i) {
            const auto& title = available_titles_[i];
            title_elements.push_back(hbox({
                text(selected_titles_[i] ? "[X] " : "[ ] "),
                text("Title " + std::to_string(title.index) + ": "),
                text(title.duration + " "),
                text("(" + title.size + ")") | dim
            }));
        }
        
        return vbox(title_elements) | frame | size(HEIGHT, LESS_THAN, 15);
    });
    
    // Progress view
    auto progress_view = Renderer([this] {
        if (current_state_ == AppState::RIPPING) {
            return vbox({
                text("Ripping Progress") | bold,
                separator(),
                hbox({
                    text("Title: "),
                    text(std::to_string(current_rip_progress_.current_title) + "/" + 
                         std::to_string(current_rip_progress_.total_titles))
                }),
                gauge(current_rip_progress_.percentage / 100.0) | flex,
                text(current_rip_progress_.status_message) | dim
            });
        } else if (current_state_ == AppState::ENCODING) {
            return vbox({
                text("Encoding Progress") | bold,
                separator(),
                text("Input: " + current_encode_progress_.input_file) | dim,
                gauge(current_encode_progress_.percentage / 100.0) | flex,
                hbox({
                    text("FPS: " + std::to_string(current_encode_progress_.fps) + " "),
                    text("ETA: " + current_encode_progress_.eta) | dim
                })
            });
        }
        return text("");
    });
    
    // Log viewer
    auto log_viewer = Renderer([this] {
        Elements log_elements;
        log_elements.push_back(text("Log:") | bold);
        log_elements.push_back(separator());
        
        // Show last 10 log messages
        size_t start = log_messages_.size() > 10 ? log_messages_.size() - 10 : 0;
        for (size_t i = start; i < log_messages_.size(); ++i) {
            log_elements.push_back(text(log_messages_[i]) | dim);
        }
        
        return vbox(log_elements) | frame | size(HEIGHT, LESS_THAN, 12);
    });
    
    // Help bar
    auto help = Renderer([] {
        return vbox({
            separator(),
            hbox({
                text("Commands: ") | bold,
                text("q: Quit | r: Rescan | s: Start rip | e: Encode")
            }) | dim
        });
    });
    
    // Combine all components
    auto layout = Container::Vertical({
        title,
        status,
        disc_selector,
        title_selector,
        progress_view,
        log_viewer,
        help
    });
    
    auto renderer = Renderer(layout, [&] {
        return vbox({
            title->Render(),
            status->Render(),
            disc_selector->Render() | flex,
            title_selector->Render(),
            progress_view->Render(),
            log_viewer->Render(),
            help->Render()
        }) | border;
    });
    
    // Handle keyboard input
    renderer |= CatchEvent([&](Event event) {
        if (event == Event::Character('q')) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Character('r')) {
            scan_for_discs();
            return true;
        }
        if (event == Event::Character('s')) {
            if (current_state_ == AppState::TITLE_SELECTION) {
                start_ripping();
            }
            return true;
        }
        if (event == Event::Character(' ')) {
            // Toggle title selection
            if (current_state_ == AppState::TITLE_SELECTION && 
                selected_disc_index_ < static_cast<int>(available_titles_.size())) {
                selected_titles_[selected_disc_index_] = 
                    !selected_titles_[selected_disc_index_];
            }
            return true;
        }
        return false;
    });
    
    // Initial scan
    scan_for_discs();
    
    screen.Loop(renderer);
}

void MainUI::scan_for_discs() {
    add_log("Scanning for optical drives...");
    current_state_ = AppState::SCANNING;
    
    // This would be done in a background thread in a real implementation
    available_discs_ = disc_detector_->scan_drives();
    
    if (available_discs_.empty()) {
        add_log("No optical drives found");
        current_state_ = AppState::DISC_SELECTION;
    } else {
        add_log("Found " + std::to_string(available_discs_.size()) + " drive(s)");
        current_state_ = AppState::DISC_SELECTION;
    }
}

void MainUI::start_ripping() {
    add_log("Starting rip process...");
    current_state_ = AppState::RIPPING;
    
    // Collect selected title indices
    std::vector<int> selected_indices;
    for (size_t i = 0; i < selected_titles_.size(); ++i) {
        if (selected_titles_[i]) {
            selected_indices.push_back(available_titles_[i].index);
        }
    }
    
    if (selected_indices.empty()) {
        add_log("No titles selected");
        current_state_ = AppState::TITLE_SELECTION;
        return;
    }
    
    add_log("Ripping " + std::to_string(selected_indices.size()) + " title(s)");
    
    // This would start the actual ripping process
    // For now, just simulate
    current_rip_progress_.current_title = 1;
    current_rip_progress_.total_titles = selected_indices.size();
    current_rip_progress_.percentage = 0.0;
}

void MainUI::start_encoding(const std::string& mkv_file) {
    add_log("Starting encoding: " + mkv_file);
    current_state_ = AppState::ENCODING;
    
    // This would start the actual encoding process
}

void MainUI::add_log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time_t);
    timestamp.pop_back(); // Remove newline
    
    log_messages_.push_back("[" + timestamp + "] " + message);
}

} // namespace bluray::ui
