#include "FileDialog.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>   // IFileDialog
#include <string>

namespace
{
    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
        return w;
    }

    std::string WideToUtf8(const wchar_t* w)
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
        return s;
    }
}

namespace VAEditor
{
    std::string BrowseForFolder(const char* title, const std::string& initialPath)
    {
        std::string result;

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        IFileDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                       IID_PPV_ARGS(&dialog))))
        {
            DWORD options = 0;
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            dialog->SetTitle(Utf8ToWide(title).c_str());

            if (!initialPath.empty())
            {
                IShellItem* folder = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(Utf8ToWide(initialPath).c_str(),
                                                          nullptr, IID_PPV_ARGS(&folder))))
                {
                    dialog->SetFolder(folder);
                    folder->Release();
                }
            }

            // Pass the GLFW window's HWND here later if you want it modal to the editor
            if (SUCCEEDED(dialog->Show(nullptr)))
            {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)))
                {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                    {
                        result = WideToUtf8(path);
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }

        if (SUCCEEDED(hrInit)) CoUninitialize();
        return result;
    }
}