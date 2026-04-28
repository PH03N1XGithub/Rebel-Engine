#pragma once

#include "Engine/Framework/EnginePch.h"

namespace Editor::WindowsFileDialogs
{
    String OpenFile(const wchar_t* title, const wchar_t* filter);
    String SaveFile(const wchar_t* title, const wchar_t* filter, const wchar_t* defaultExtension);
    String OpenFolder(const wchar_t* title);
}
