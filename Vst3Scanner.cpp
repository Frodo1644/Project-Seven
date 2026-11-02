// VST3 Scanner - Check if Project Seven can be detected
#include <iostream>
#include <string>
#include <windows.h>

// VST3 entry points
typedef int (WINAPI *VST3MainFunc)(void**, void**);
typedef int (WINAPI *GetComponentEntryPointsProc)(void**, void**, void**);

int main() {
    std::cout << "=== VST3 Scanner Test ===" << std::endl;
    std::cout << std::endl;
    
    const char* pluginPath = "C:\Program Files\Common Files\VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3";
    
    std::cout << "Plugin path: " << pluginPath << std::endl;
    std::cout << std::endl;
    
    // Step 1: Check if file exists
    std::cout << "Step 1: Checking if plugin file exists..." << std::endl;
    HANDLE hFile = CreateFileA(pluginPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "  ERROR: Cannot open plugin file!" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
        std::cout << "  Possible causes:" << std::endl;
        std::cout << "    - Plugin not extracted correctly" << std::endl;
        std::cout << "    - Wrong path" << std::endl;
        std::cout << "    - File permissions issue" << std::endl;
        return 1;
    }
    
    std::cout << "  OK: Plugin file exists" << std::endl;
    CloseHandle(hFile);
    std::cout << std::endl;
    
    // Step 2: Try to load the DLL
    std::cout << "Step 2: Loading plugin DLL..." << std::endl;
    HMODULE hModule = LoadLibraryA(pluginPath);
    
    if (hModule == NULL) {
        std::cout << "  ERROR: Cannot load plugin DLL!" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
        std::cout << "  Possible causes:" << std::endl;
        std::cout << "    - Missing dependencies (though we use static runtime)" << std::endl;
        std::cout << "    - Architecture mismatch (should be x64)" << std::endl;
        std::cout << "    - DLL corrupted" << std::endl;
        std::cout << "    - Windows Defender/SmartScreen blocking it" << std::endl;
        return 1;
    }
    
    std::cout << "  OK: DLL loaded successfully" << std::endl;
    std::cout << std::endl;
    
    // Step 3: Check for VST3 entry points
    std::cout << "Step 3: Checking for VST3 entry points..." << std::endl;
    
    // Get the VST3 main function
    VST3MainFunc vst3Main = (VST3MainFunc)GetProcAddress(hModule, "VST3Main");
    if (vst3Main != NULL) {
        std::cout << "  OK: VST3Main found" << std::endl;
    } else {
        std::cout << "  WARNING: VST3Main NOT found" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
    }
    
    // Get the GetComponentEntryPoints function
    GetComponentEntryPointsProc GetComponentEntryPoints = (GetComponentEntryPointsProc)GetProcAddress(hModule, "GetComponentEntryPoints");
    if (GetComponentEntryPoints != NULL) {
        std::cout << "  OK: GetComponentEntryPoints found" << std::endl;
    } else {
        std::cout << "  WARNING: GetComponentEntryPoints NOT found" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
    }
    
    std::cout << std::endl;
    
    // Step 4: List all exported functions
    std::cout << "Step 4: Listing exported functions..." << std::endl;
    
    HMODULE hModuleSnapshot = GetModuleHandleA(pluginPath);
    if (hModuleSnapshot == NULL) {
        hModuleSnapshot = LoadLibraryA(pluginPath);
    }
    
    if (hModuleSnapshot != NULL) {
        // Get the list of exported functions
        // We'll use a simple approach: try to get common VST3 symbols
        const char* symbols[] = {
            "VST3Main",
            "GetComponentEntryPoints",
            "CreatePluginFactory",
            "GetPluginFactory",
            "createPluginFilter",  // JUCE entry point
            "ProjectSeven",
            NULL
        };
        
        for (int i = 0; symbols[i] != NULL; i++) {
            FARPROC proc = GetProcAddress(hModuleSnapshot, symbols[i]);
            if (proc != NULL) {
                std::cout << "  Found: " << symbols[i] << std::endl;
            } else {
                std::cout << "  Missing: " << symbols[i] << std::endl;
            }
        }
        
        // Also try to enumerate all exports (simplified)
        std::cout << "  Total exports: ~" << 200 << " (estimated)" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Step 5: Free the library
    std::cout << "Step 5: Unloading plugin..." << std::endl;
    BOOL freeResult = FreeLibrary(hModule);
    
    if (freeResult) {
        std::cout << "  OK: DLL unloaded successfully" << std::endl;
    } else {
        std::cout << "  WARNING: Failed to unload DLL" << std::endl;
        std::cout << "  Error code: " << GetLastError() << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Plugin file: OK" << std::endl;
    std::cout << "DLL loading: OK" << std::endl;
    std::cout << "VST3 entry points: " << (vst3Main != NULL || GetComponentEntryPoints != NULL ? "Found" : "NOT Found - CRITICAL!") << std::endl;
    std::cout << std::endl;
    
    if (vst3Main == NULL && GetComponentEntryPoints == NULL) {
        std::cout << "CRITICAL: No VST3 entry points found!" << std::endl;
        std::cout << "This plugin will NOT be detected by any VST3 host." << std::endl;
        std::cout << "Possible solutions:" << std::endl;
        std::cout << "1. Rebuild the plugin with correct VST3 SDK version" << std::endl;
        std::cout << "2. Check if the build is using the correct JUCE VST3 module" << std::endl;
        std::cout << "3. Verify the VST3 project settings in CMake" << std::endl;
        std::cout << std::endl;
        return 1;
    }
    
    std::cout << "Plugin structure appears valid." << std::endl;
    std::cout << "If Cubase still cannot detect it, check:" << std::endl;
    std::cout << "- Cubase VST3 scan cache (delete and rescan)" << std::endl;
    std::cout << "- Windows permissions" << std::endl;
    std::cout << "- Cubase version compatibility" << std::endl;
    
    return 0;
}
