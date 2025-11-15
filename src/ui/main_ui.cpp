#include "ui/main_ui.h"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include <chrono>
#include <thread>
#include <filesystem>
#include <regex>

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
    std::vector<std::string> title_entries;
    auto title_menu = Menu(&title_entries, &selected_title_index_);

    auto title_selector = Renderer(title_menu, [this, &title_entries, title_menu] {
        // Update title entries
        title_entries.clear();
        for (size_t i = 0; i < available_titles_.size(); ++i) {
            const auto& title = available_titles_[i];
            std::string checkbox = selected_titles_[i] ? "[X] " : "[ ] ";
            title_entries.push_back(
                checkbox + "Title " + std::to_string(title.index) + ": " +
                title.duration + " (" + title.size + ")"
            );
        }

        if (available_titles_.empty()) {
            return text("No titles loaded") | dim;
        }

        return vbox({
            text("Select titles to rip:") | bold,
            separator(),
            title_menu->Render() | frame | size(HEIGHT, LESS_THAN, 15)
        });
    });
    
    // Progress view
    auto progress_view = Renderer([this] {
        if (current_state_ == AppState::RIPPING) {
            // Check if ripping is complete (thread-safe check)
            // This allows the UI to update when ripping finishes
            const_cast<MainUI*>(this)->check_rip_completion();

            // Thread-safe access to progress data
            RipProgress progress_copy;
            {
                std::lock_guard<std::mutex> lock(progress_mutex_);
                progress_copy = current_rip_progress_;
            }

            return vbox({
                text("Ripping Progress") | bold,
                separator(),
                hbox({
                    text("Title: "),
                    text(std::to_string(progress_copy.current_title) + "/" +
                         std::to_string(progress_copy.total_titles))
                }),
                gauge(progress_copy.percentage / 100.0) | flex,
                text(progress_copy.status_message) | dim
            });
        } else if (current_state_ == AppState::ENCODING) {
            // Thread-safe access to progress data
            EncodeProgress progress_copy;
            {
                std::lock_guard<std::mutex> lock(progress_mutex_);
                progress_copy = current_encode_progress_;
            }

            return vbox({
                text("Encoding Progress") | bold,
                separator(),
                hbox({
                    text("File: "),
                    text(std::to_string(current_encode_index_ + 1) + "/" +
                         std::to_string(ripped_files_.size()))
                }),
                gauge(progress_copy.percentage / 100.0) | flex,
                hbox({
                    text("FPS: " + std::to_string(static_cast<int>(progress_copy.fps)) + " | "),
                    text("Avg: " + std::to_string(static_cast<int>(progress_copy.avg_fps)) + " | "),
                    text("ETA: " + progress_copy.eta)
                }) | dim,
                text(progress_copy.status_message) | dim
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
                text("q: Quit | r: Rescan | Enter: Load titles | s: Start rip | e: Encode")
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
        if (event == Event::Return) {
            // Load titles when Enter is pressed on a selected disc
            if (current_state_ == AppState::DISC_SELECTION && !available_discs_.empty()) {
                load_disc_titles();
            }
            return true;
        }
        if (event == Event::Character('s')) {
            if (current_state_ == AppState::TITLE_SELECTION) {
                start_ripping();
            }
            return true;
        }
        if (event == Event::Character('e')) {
            // Start encoding - scan for MKV files if needed
            if (ripped_files_.empty()) {
                // Scan output directory for MKV files
                add_log("Scanning for MKV files in " + output_directory_);
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(output_directory_)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".mkv") {
                            std::string mkv_path = entry.path().string();
                            std::string filename = entry.path().filename().string();

                            // Extract title number from filename
                            // MakeMKV creates files like "Movie_t01.mkv" or "title01.mkv"
                            int title_num = 1;  // Default to 1
                            std::regex title_regex(R"(_t(\d+)\.mkv|title(\d+)\.mkv)");
                            std::smatch match;
                            if (std::regex_search(filename, match, title_regex)) {
                                // Check which group matched
                                if (match[1].matched) {
                                    title_num = std::stoi(match[1]);
                                } else if (match[2].matched) {
                                    title_num = std::stoi(match[2]);
                                }
                            }

                            RippedFile ripped;
                            ripped.mkv_path = mkv_path;
                            ripped.title_number = title_num;
                            ripped.output_name = filename;

                            ripped_files_.push_back(ripped);
                            add_log("Found: " + filename + " (title " + std::to_string(title_num) + ")");
                        }
                    }
                } catch (const std::exception& e) {
                    add_log("Error scanning directory: " + std::string(e.what()));
                }
            }

            if (!ripped_files_.empty()) {
                start_encoding();
            } else {
                add_log("No MKV files found in " + output_directory_);
            }
            return true;
        }
        if (event == Event::Character(' ')) {
            // Toggle title selection
            if (current_state_ == AppState::TITLE_SELECTION &&
                selected_title_index_ >= 0 &&
                selected_title_index_ < static_cast<int>(available_titles_.size())) {
                selected_titles_[selected_title_index_] =
                    !selected_titles_[selected_title_index_];
            }
            return true;
        }
        return false;
    });
    
    // Initial scan
    scan_for_discs();

    // Store screen reference for async operations
    screen_ = &screen;

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

void MainUI::load_disc_titles() {
    if (selected_disc_index_ < 0 ||
        selected_disc_index_ >= static_cast<int>(available_discs_.size())) {
        add_log("No disc selected");
        return;
    }

    const auto& selected_disc = available_discs_[selected_disc_index_];
    add_log("Loading titles from " + selected_disc.device_path + "...");

    // Load titles from the selected disc
    auto titles = disc_detector_->get_disc_titles(selected_disc.device_path);

    if (titles.has_value()) {
        available_titles_ = titles.value();
        selected_titles_.clear();
        selected_titles_.resize(available_titles_.size(), false);
        selected_title_index_ = 0;

        add_log("Found " + std::to_string(available_titles_.size()) + " title(s)");
        current_state_ = AppState::TITLE_SELECTION;
    } else {
        add_log("Failed to load titles from disc");
        available_titles_.clear();
        selected_titles_.clear();
        selected_title_index_ = 0;
    }
}

void MainUI::start_ripping() {
    add_log("Starting rip process...");

    // Collect selected title indices
    std::vector<int> selected_indices;
    for (size_t i = 0; i < selected_titles_.size(); ++i) {
        if (selected_titles_[i]) {
            selected_indices.push_back(available_titles_[i].index);
        }
    }

    if (selected_indices.empty()) {
        add_log("No titles selected");
        return;
    }

    // Create output directory if it doesn't exist
    try {
        std::filesystem::create_directories(output_directory_);
    } catch (const std::exception& e) {
        add_log("Error creating output directory: " + std::string(e.what()));
        return;
    }

    // Get device path from selected disc
    if (selected_disc_index_ < 0 ||
        selected_disc_index_ >= static_cast<int>(available_discs_.size())) {
        add_log("Invalid disc selection");
        return;
    }

    const auto& device_path = available_discs_[selected_disc_index_].device_path;

    add_log("Ripping " + std::to_string(selected_indices.size()) + " title(s) to " + output_directory_);
    current_state_ = AppState::RIPPING;

    // Initialize progress tracking
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        current_rip_progress_.current_title = 0;
        current_rip_progress_.total_titles = selected_indices.size();
        current_rip_progress_.percentage = 0.0;
        current_rip_progress_.status_message = "Starting...";
    }

    // Create progress callback that updates UI
    auto progress_callback = [this](const RipProgress& progress) {
        {
            std::lock_guard<std::mutex> lock(progress_mutex_);
            current_rip_progress_ = progress;
        }

        // Add visible logging for debugging
        if (progress.percentage > 0 && static_cast<int>(progress.percentage) % 10 == 0) {
            add_log("RIP: " + std::to_string(static_cast<int>(progress.percentage)) +
                    "% - " + progress.status_message);
        }

        // Trigger screen refresh
        if (screen_) {
            screen_->Post(Event::Custom);
        }
    };

    // Start the actual ripping process
    rip_future_ = makemkv_->rip_titles(
        device_path,
        selected_indices,
        output_directory_,
        progress_callback
    );
}

void MainUI::start_encoding() {
    if (ripped_files_.empty()) {
        add_log("No files to encode");
        return;
    }

    add_log("Starting encoding of " + std::to_string(ripped_files_.size()) + " file(s)...");
    current_state_ = AppState::ENCODING;
    current_encode_index_ = 0;

    // Create encoded output subdirectory
    std::string encoded_dir = output_directory_ + "/encoded";
    try {
        std::filesystem::create_directories(encoded_dir);
    } catch (const std::exception& e) {
        add_log("Error creating encoded output directory: " + std::string(e.what()));
        return;
    }

    // Reset encode progress
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        current_encode_progress_.percentage = 0.0;
        current_encode_progress_.fps = 0.0;
        current_encode_progress_.avg_fps = 0.0;
        current_encode_progress_.eta = "00:00:00";
        current_encode_progress_.status_message = "Starting...";
    }

    // Encode all files sequentially
    encode_future_ = std::async(std::launch::async, [this, encoded_dir]() {
        bool all_success = true;

        for (size_t i = 0; i < ripped_files_.size(); ++i) {
            current_encode_index_ = i;
            const auto& file = ripped_files_[i];

            std::string output_path = encoded_dir + "/" + file.output_name;
            add_log("Encoding " + std::to_string(i + 1) + "/" +
                    std::to_string(ripped_files_.size()) + ": " + file.output_name);

            // Create progress callback for this file
            auto progress_callback = [this, i, total = ripped_files_.size()](const EncodeProgress& progress) {
                {
                    std::lock_guard<std::mutex> lock(progress_mutex_);
                    current_encode_progress_ = progress;
                    // Add file tracking info to the progress
                    current_encode_progress_.status_message =
                        "File " + std::to_string(i + 1) + "/" + std::to_string(total) +
                        " - " + std::to_string(static_cast<int>(progress.percentage)) + "%";
                }

                // Trigger screen refresh
                if (screen_) {
                    screen_->Post(Event::Custom);
                }
            };

            // Encode with custom parameters: x265 (or nvenc_h265 if GPU available), slow, quality 22
            // Note: Use "nvenc_h265" if you have NVIDIA GPU, otherwise use "x265"
            auto encode_future = handbrake_->encode(
                file.mkv_path,
                output_path,
                file.title_number,
                "x265",  // Change to "nvenc_h265" if you have NVIDIA GPU
                "slow",
                22,
                progress_callback
            );

            // Wait for this file to complete
            bool success = encode_future.get();
            if (!success) {
                add_log("ERROR: Failed to encode " + file.output_name);
                all_success = false;
                break;
            } else {
                add_log("Successfully encoded " + file.output_name);
            }
        }

        // Update state when all encoding is complete
        if (all_success) {
            add_log("All files encoded successfully!");
            current_state_ = AppState::COMPLETED;
        }

        if (screen_) {
            screen_->Post(Event::Custom);
        }

        return all_success;
    });
}

void MainUI::check_rip_completion() {
    // Check if ripping is complete
    if (rip_future_.valid() &&
        rip_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {

        bool success = rip_future_.get();
        if (success) {
            add_log("Ripping completed successfully!");

            // Scan output directory for ripped MKV files
            ripped_files_.clear();
            try {
                for (const auto& entry : std::filesystem::directory_iterator(output_directory_)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".mkv") {
                        std::string mkv_path = entry.path().string();
                        std::string filename = entry.path().filename().string();

                        // Extract title number from filename if possible
                        // MakeMKV creates files like "Movie_t01.mkv" or "title01.mkv"
                        int title_num = 1;  // Default to 1
                        std::regex title_regex(R"(_t(\d+)\.mkv|title(\d+)\.mkv)");
                        std::smatch match;
                        if (std::regex_search(filename, match, title_regex)) {
                            // Check which group matched
                            if (match[1].matched) {
                                title_num = std::stoi(match[1]);
                            } else if (match[2].matched) {
                                title_num = std::stoi(match[2]);
                            }
                        }

                        // Create output filename: same as input but in encoded subdirectory
                        RippedFile ripped;
                        ripped.mkv_path = mkv_path;
                        ripped.title_number = title_num;
                        ripped.output_name = filename;  // Keep same filename

                        ripped_files_.push_back(ripped);
                        add_log("Found ripped file: " + filename);
                    }
                }

                if (ripped_files_.empty()) {
                    add_log("Warning: No MKV files found in output directory");
                } else {
                    add_log("Found " + std::to_string(ripped_files_.size()) +
                           " file(s) ready to encode");
                    add_log("Press 'e' to start encoding");
                }
            } catch (const std::exception& e) {
                add_log("Error scanning output directory: " + std::string(e.what()));
            }

            // Stay in RIPPING state but allow 'e' key to trigger encoding
        } else {
            add_log("Ripping failed");
            current_state_ = AppState::COMPLETED;
        }
    }
}

void MainUI::add_log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time_t);
    timestamp.pop_back(); // Remove newline
    
    log_messages_.push_back("[" + timestamp + "] " + message);
}

} // namespace bluray::ui
