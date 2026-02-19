#include <iostream>
#include <thread>
#include <chrono>
#include "../choc/platform/choc_FileWatcher.h"
#include "../choc/text/choc_Files.h"

int main()
{
    std::cout << "FileWatcher Example" << std::endl;
    std::cout << "Monitoring current directory for changes... (Press Ctrl+C to exit)" << std::endl;

    choc::file::TempFile tempDir ("file_watcher_test_dir");
    std::cout << "Temporary directory created: " << tempDir.file.string() << std::endl;

    choc::file::Watcher watcher (tempDir.file, [] (const choc::file::Watcher::Event& event)
    {
        std::cout << "Event: " << event.file.filename().string() << " - ";
        switch (event.eventType)
        {
            case choc::file::Watcher::EventType::created: std::cout << "Added"; break;
            case choc::file::Watcher::EventType::destroyed: std::cout << "Removed"; break;
            case choc::file::Watcher::EventType::modified: std::cout << "Modified"; break;
            case choc::file::Watcher::EventType::renamed: std::cout << "Moved"; break;
            default: break;
        }
        std::cout << std::endl;
    });

    // Create a file
    choc::file::replaceFileWithContent (tempDir.file / "test_file.txt", "Hello, FileWatcher!");
    std::this_thread::sleep_for (std::chrono::milliseconds (100)); // Give watcher time to process

    // Modify the file
    choc::file::replaceFileWithContent (tempDir.file / "test_file.txt", "Hello again, FileWatcher!");
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

    // Create another file
    choc::file::replaceFileWithContent (tempDir.file / "another_file.txt", "Another one!");
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

    // Delete a file
    std::filesystem::remove (tempDir.file / "test_file.txt");
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

    // Keep the main thread alive to allow the watcher to run
    for (;;)
        std::this_thread::sleep_for (std::chrono::seconds (1));

    return 0;
}
