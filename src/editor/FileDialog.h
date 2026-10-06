#pragma once
#include <string>

namespace VAEditor
{
    // Opens the native Windows folder picker.
    // Returns the selected folder path, or an empty string if cancelled.
    std::string BrowseForFolder(const char* title = "Select Folder",
                                const std::string& initialPath = "");
}