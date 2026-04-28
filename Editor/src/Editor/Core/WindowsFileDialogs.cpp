#include "Editor/Core/WindowsFileDialogs.h"

#include <windows.h>
#include <shobjidl.h>
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Ole32.lib")

namespace
{
    String WideToString(const wchar_t* text)
    {
        if (!text || text[0] == L'\0')
            return {};

        const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 1)
            return {};

        std::string utf8(static_cast<size_t>(needed - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), needed, nullptr, nullptr);
        return String(utf8.c_str());
    }
}

namespace Editor::WindowsFileDialogs
{
    String OpenFile(const wchar_t* title, const wchar_t* filter)
    {
        wchar_t fileBuffer[MAX_PATH] = {};

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.lpstrFilter = filter;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn))
            return {};

        return WideToString(fileBuffer);
    }

    String SaveFile(const wchar_t* title, const wchar_t* filter, const wchar_t* defaultExtension)
    {
        wchar_t fileBuffer[MAX_PATH] = {};

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title;
        ofn.lpstrFilter = filter;
        ofn.lpstrDefExt = defaultExtension;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&ofn))
            return {};

        return WideToString(fileBuffer);
    }

    String OpenFolder(const wchar_t* title)
    {
        HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
        {
            if (shouldUninitialize)
                CoUninitialize();
            return {};
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
        dialog->SetTitle(title);

        String result;
        if (SUCCEEDED(dialog->Show(GetActiveWindow())))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                {
                    result = WideToString(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (shouldUninitialize)
            CoUninitialize();

        return result;
    }
}
