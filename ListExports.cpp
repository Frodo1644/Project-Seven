#include <iostream>
#include <windows.h>
#include <vector>
#include <string>

int main() {
    const wchar_t* vst3Path = L"C:\Program Files\Common Files\VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3";
    
    std::wcout << L"=== Export List for: " << vst3Path << L" ===" << std::endl;
    std::cout << std::endl;
    
    HMODULE hModule = LoadLibraryW(vst3Path);
    if (hModule == NULL) {
        std::cout << "Cannot load DLL" << std::endl;
        return 1;
    }
    
    // Get DOS header
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "Not a valid PE file" << std::endl;
        FreeLibrary(hModule);
        return 1;
    }
    
    // Get NT headers
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "Invalid NT headers" << std::endl;
        FreeLibrary(hModule);
        return 1;
    }
    
    // Get export directory
    IMAGE_EXPORT_DIRECTORY* exportDir = (IMAGE_EXPORT_DIRECTORY*)((BYTE*)hModule + 
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    
    std::cout << "Export Directory:" << std::endl;
    std::cout << "  Base: " << exportDir->Base << std::endl;
    std::cout << "  Number of functions: " << exportDir->NumberOfFunctions << std::endl;
    std::cout << "  Number of names: " << exportDir->NumberOfNames << std::endl;
    std::cout << std::endl;
    
    // Get pointers to export tables
    DWORD* namePointerTable = (DWORD*)((BYTE*)hModule + exportDir->AddressOfNames);
    WORD* ordinalTable = (WORD*)((BYTE*)hModule + exportDir->AddressOfNameOrdinals);
    DWORD* functionAddressTable = (DWORD*)((BYTE*)hModule + exportDir->AddressOfFunctions);
    
    std::cout << "Exported functions:" << std::endl;
    std::cout << "-------------------" << std::endl;
    
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        DWORD nameRVA = namePointerTable[i];
        const char* functionName = (const char*)((BYTE*)hModule + nameRVA);
        WORD ordinal = ordinalTable[i] + exportDir->Base;
        DWORD funcRVA = functionAddressTable[ordinal - exportDir->Base];
        void* funcAddr = (BYTE*)hModule + funcRVA;
        
        std::cout << "  [" << ordinal << "] " << functionName;
        
        // Check if it's a forwarder
        if (funcRVA >= exportDir->VirtualAddress && funcRVA < exportDir->VirtualAddress + exportDir->Size) {
            std::cout << " (forwarder)";
        } else {
            std::cout << " (0x" << funcAddr << ")";
        }
        std::cout << std::endl;
    }
    
    if (exportDir->NumberOfFunctions > exportDir->NumberOfNames) {
        std::cout << std::endl;
        std::cout << "Unnamed exports (by ordinal):" << std::endl;
        std::cout << "-----------------------------" << std::endl;
        
        for (DWORD ordinal = exportDir->Base; ordinal < exportDir->Base + exportDir->NumberOfFunctions; ordinal++) {
            bool hasName = false;
            for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
                if (ordinalTable[i] + exportDir->Base == ordinal) {
                    hasName = true;
                    break;
                }
            }
            
            if (!hasName) {
                DWORD funcRVA = functionAddressTable[ordinal - exportDir->Base];
                void* funcAddr = (BYTE*)hModule + funcRVA;
                std::cout << "  [" << ordinal << "] 0x" << funcAddr << std::endl;
            }
        }
    }
    
    std::cout << std::endl;
    
    // Check for specific VST3 entry points
    std::cout << "Checking for VST3/AU entry points:" << std::endl;
    
    typedef int (WINAPI *VST3MainFunc)(void**, void**);
    VST3MainFunc vst3Main = (VST3MainFunc)GetProcAddress(hModule, "VST3Main");
    std::cout << "  VST3Main: " << (vst3Main ? "FOUND" : "not found") << std::endl;
    
    // Check for various JUCE exports
    const char* juceExports[] = {
        "?createPluginFilter@@YAPEAUAEAudioProcessor@@XZ",
        "?createPluginFilter@ProjectSeven@@YAPEAUAEAudioProcessor@@XZ",
        "createPluginFilter",
        NULL
    };
    
    for (int i = 0; juceExports[i] != NULL; i++) {
        void* addr = GetProcAddress(hModule, juceExports[i]);
        if (addr) {
            std::cout << "  JUCE createPluginFilter (" << juceExports[i] << "): FOUND" << std::endl;
        }
    }
    
    std::cout << std::endl;
    
    // Check for VST2 compatibility entry points (should not exist for VST3-only)
    typedef AudioEffect* (*VST2MainFunc)(AudioEffect**);
    VST2MainFunc vst2Main = (VST2MainFunc)GetProcAddress(hModule, "main");
    if (vst2Main) {
        std::cout << "  VST2 main(): FOUND (unexpected for VST3-only plugin!)" << std::endl;
    } else {
        std::cout << "  VST2 main(): not found (expected for VST3-only)" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== Analysis ===" << std::endl;
    
    bool hasVst3Entry = (vst3Main != NULL);
    bool hasJuceEntry = false;
    const char* juceExports2[] = {
        "?createPluginFilter@@YAPEAUAEAudioProcessor@@XZ",
        NULL
    };
    for (int i = 0; juceExports2[i] != NULL; i++) {
        if (GetProcAddress(hModule, juceExports2[i])) {
            hasJuceEntry = true;
            break;
        }
    }
    
    if (hasVst3Entry || hasJuceEntry) {
        std::cout << "Plugin appears to have valid entry points." << std::endl;
        std::cout << "It should be loadable by VST3 hosts." << std::endl;
    } else {
        std::cout << "WARNING: No standard VST3 entry points found!" << std::endl;
        std::cout << "This may explain why Cubase cannot detect the plugin." << std::endl;
        std::cout << std::endl;
        std::cout << "Possible issues:" << std::endl;
        std::cout << "1. Plugin built as VST2-only or standalone" << std::endl;
        std::cout << "2. JUCE VST3 module not properly linked" << std::endl;
        std::cout << "3. Export definitions missing" << std::endl;
        std::cout << "4. Wrong architecture or corrupted build" << std::endl;
    }
    
    FreeLibrary(hModule);
    return 0;
}
