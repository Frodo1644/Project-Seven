#include <iostream>
#include <windows.h>

int main() {
    std::cout << "=== Project Seven VST3 Scanner ===" << std::endl;
    std::cout << std::endl;
    
    const wchar_t* vst3Path = L"C:\\Program Files\\Common Files\\VST3\\Project Seven.vst3\\Contents\\x86_64-win\\Project Seven.vst3";
    
    std::wcout << L"Checking VST3 at: " << vst3Path << std::endl;
    std::cout << std::endl;
    
    // Step 1: Check if file exists
    std::cout << "Step 1: Checking if VST3 file exists..." << std::endl;
    HANDLE hFile = CreateFileW(vst3Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "  ERROR: Cannot open VST3 file!" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
        std::cout << "  Possible issues:" << std::endl;
        std::cout << "    - File not extracted properly" << std::endl;
        std::cout << "    - Wrong path" << std::endl;
        std::cout << "    - Insufficient permissions" << std::endl;
        return 1;
    }
    std::cout << "  OK: File exists" << std::endl;
    CloseHandle(hFile);
    std::cout << std::endl;
    
    // Step 2: Try to load the VST3 DLL
    std::cout << "Step 2: Loading VST3 DLL..." << std::endl;
    HMODULE hModule = LoadLibraryW(vst3Path);
    
    if (hModule == NULL) {
        std::cout << "  ERROR: Cannot load VST3 DLL!" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
        std::cout << "  This usually means:" << std::endl;
        std::cout << "    - Missing dependencies (but we use static runtime)" << std::endl;
        std::cout << "    - Wrong architecture (should be x64)" << std::endl;
        std::cout << "    - File corrupted" << std::endl;
        std::cout << "    - Antivirus blocking it" << std::endl;
        return 1;
    }
    std::cout << "  OK: DLL loaded successfully" << std::endl;
    std::cout << std::endl;
    
    // Step 3: Check for VST3 entry points
    std::cout << "Step 3: Checking for VST3 entry points..." << std::endl;
    
    // Check for standard VST3 entry point
    typedef int (WINAPI *VST3MainFunc)(void**, void**);
    VST3MainFunc vst3Main = (VST3MainFunc)GetProcAddress(hModule, "VST3Main");
    if (vst3Main) {
        std::cout << "  Found: VST3Main" << std::endl;
    } else {
        std::cout << "  Missing: VST3Main" << std::endl;
    }
    
    // Check for JUCE createPluginFilter (decorated name)
    typedef void* (*CreatePluginFilterFunc)();
    CreatePluginFilterFunc createPluginFilter = (CreatePluginFilterFunc)GetProcAddress(hModule, "?createPluginFilter@@YAPEAUAEAudioProcessor@@XZ");
    if (createPluginFilter) {
        std::cout << "  Found: createPluginFilter (JUCE)" << std::endl;
    } else {
        // Try undecorated name (unlikely but possible)
        createPluginFilter = (CreatePluginFilterFunc)GetProcAddress(hModule, "createPluginFilter");
        if (createPluginFilter) {
            std::cout << "  Found: createPluginFilter (undecorated)" << std::endl;
        } else {
            std::cout << "  Missing: createPluginFilter" << std::endl;
        }
    }
    
    std::cout << std::endl;
    
    // Step 4: Check exports directory
    std::cout << "Step 4: Checking exports..." << std::endl;
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
        IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dosHeader->e_lfanew);
        if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
            if (ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size > 0) {
                std::cout << "  Has export directory: YES" << std::endl;
                
                // Get export directory
                IMAGE_EXPORT_DIRECTORY* exportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)hModule + 
                    ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
                
                std::cout << "  Number of functions: " << exportDir->NumberOfFunctions << std::endl;
                std::cout << "  Number of names: " << exportDir->NumberOfNames << std::endl;
            } else {
                std::cout << "  Has export directory: NO (this is bad!)" << std::endl;
            }
        }
    }
    
    std::cout << std::endl;
    std::cout << "Step 5: Summary" << std::endl;
    
    bool hasEntryPoint = (vst3Main != NULL) || (createPluginFilter != NULL);
    if (hasEntryPoint) {
        std::cout << "  VST3 plugin structure: VALID" << std::endl;
        std::cout << "  Should be detectable by hosts" << std::endl;
    } else {
        std::cout << "  VST3 plugin structure: INVALID" << std::endl;
        std::cout << "  Missing required entry points!" << std::endl;
        std::cout << "  This plugin will NOT be detected by any VST3 host." << std::endl;
        std::cout << std::endl;
        std::cout << "  Possible causes:" << std::endl;
        std::cout << "    - Built without VST3 support" << std::endl;
        std::cout << "    - Wrong build configuration" << std::endl;
        std::cout << "    - JUCE VST3 module not linked" << std::endl;
        std::cout << "    - Entry point not exported correctly" << std::endl;
    }
    
    FreeLibrary(hModule);
    
    return hasEntryPoint ? 0 : 1;
}
