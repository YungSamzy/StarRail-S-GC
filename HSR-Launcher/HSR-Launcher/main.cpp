#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <optional>
#include <fstream>
#include <commdlg.h>
#define _CRT_SECURE_NO_WARNINGS
namespace fs = std::filesystem;

namespace util
{
    void log(const char* fmt, std::string args)
    {
        printf("[star railer] ");
        printf(fmt, args);
    }

    void logdialog(const char* fmt)
    {
        const char* errordialogformat = "CS.LAMLMFNDPHJ.HAFGEFPIKFK(\"%s\",\"star railer\")";
        char errordialogtext[256];
        snprintf(errordialogtext, sizeof(errordialogtext), errordialogformat, fmt);
        printf(errordialogtext);
    }
}

bool InjectStandard(HANDLE hTarget, const char* dllpath)
{
    LPVOID loadlib = GetProcAddress(GetModuleHandle(L"kernel32"), "LoadLibraryA");

    LPVOID dllPathAddr = VirtualAllocEx(hTarget, NULL, strlen(dllpath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (dllPathAddr == NULL)
    {
        std::cout << "Failed allocating memory in the target process. GetLastError(): " << GetLastError() << "\n";
        return false;
    }

    if (!WriteProcessMemory(hTarget, dllPathAddr, dllpath, strlen(dllpath) + 1, NULL))
    {
        std::cout << "Failed writing to process. GetLastError(): " << GetLastError() << "\n";
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hTarget, NULL, NULL, (LPTHREAD_START_ROUTINE)loadlib, dllPathAddr, NULL, NULL);
    if (hThread == NULL)
    {
        std::cout << "Failed to create a thread in the target process. GetLastError(): " << GetLastError() << "\n";
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeThread(hThread, &exit_code);

    VirtualFreeEx(hTarget, dllPathAddr, 0, MEM_RELEASE);
    CloseHandle(hThread);

    if (exit_code == 0)
    {
        std::cout << "LoadLibrary failed.\n";
        return false;
    }
    return true;
}

std::optional<std::string> read_whole_file(const fs::path& file)
try
{
    std::stringstream buf;
    std::ifstream ifs(file);
    if (!ifs.is_open())
        return std::nullopt;
    ifs.exceptions(std::ios::failbit);
    buf << ifs.rdbuf();
    return buf.str();
}
catch (const std::ios::failure&)
{
    return std::nullopt;
}

std::optional<fs::path> this_dir()
{
    HMODULE mod = NULL;
    TCHAR path[MAX_PATH]{};
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)&this_dir, &mod))
    {
        printf("GetModuleHandleEx failed (%i)\n", GetLastError());
        return std::nullopt;
    }

    if (!GetModuleFileName(mod, path, MAX_PATH))
    {
        printf("GetModuleFileName failed (%i)\n", GetLastError());
        return std::nullopt;
    }

    return fs::path(path).remove_filename();
}

int main()
{
    auto current_dir = this_dir();
    if (!current_dir)
        return 0;

    auto dll_path = current_dir.value() / "HSR-GC.dll";
    if (!fs::is_regular_file(dll_path))
    {
        printf("DLL not found\n");
        system("pause");
        return 0;
    }
    std::string exe_path;
    auto settings_path = current_dir.value() / "settings.txt";
    if (!fs::is_regular_file(settings_path))
    {
        std::ofstream settings_file("settings.txt", std::ios_base::app);
        if (settings_file.is_open()) {
            settings_file << exe_path << std::endl;
            settings_file.close();
        }
        else {
            std::ofstream create_file("settings.txt");
            if (create_file.is_open()) {
                create_file << exe_path << std::endl;
                create_file.close();
            }
            else {
                std::cout << "Error: Unable to create settings file." << std::endl;
                return 1;
            }
        }
        return 0;
    }

    auto settings = read_whole_file(settings_path);
    if (!settings)
    {
        printf("Failed reading settings.txt\n");
        system("pause");
        return 0;
    }

    std::getline(std::stringstream(settings.value()), exe_path);
    if (!fs::is_regular_file(exe_path))
    {
        std::cout << "File path in settings.txt invalid" << std::endl;
        std::cout << "Please select your Game Executable" << std::endl;
        OPENFILENAMEA ofn{};
        char szFile[260]{};
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.hwndOwner = NULL;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = "Select Executable File";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn))
        {
            exe_path = ofn.lpstrFile;
            std::ofstream settings_file("settings.txt", std::ios_base::out);
            if (settings_file.is_open()) {
                settings_file << exe_path << std::endl;
                settings_file.close();
            }
            else {
                std::cout << "Error: Unable to open settings file." << std::endl;
                return 1;
            }
        }
        else {
            std::cout << "Error: Unable to open file dialog." << std::endl;
            return 1;
        }

        PROCESS_INFORMATION proc_info{};
        STARTUPINFOA startup_info{};
        CreateProcessA(exe_path.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup_info, &proc_info);

        InjectStandard(proc_info.hProcess, dll_path.string().c_str());
        ResumeThread(proc_info.hThread);
        CloseHandle(proc_info.hThread);
        CloseHandle(proc_info.hProcess);
        return 0;
    }

    PROCESS_INFORMATION proc_info{};
    STARTUPINFOA startup_info{};
    CreateProcessA(exe_path.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup_info, &proc_info);

    InjectStandard(proc_info.hProcess, dll_path.string().c_str());
    ResumeThread(proc_info.hThread);
    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);
    std::cout << "LoadLibrary Success!" << std::endl;
    return 0;
}