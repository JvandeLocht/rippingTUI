#include "ui/main_ui.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        bluray::ui::MainUI app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
