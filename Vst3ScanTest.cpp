#include <iostream>
#include <string>

// This is a simple VST3 scanner that checks if a plugin can be loaded
// It uses the VST3 SDK's plugin detection mechanism

// VST3 SDK headers are available in our build environment

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
    std::cout << "=== VST3 Scan Test ===" << std::endl;
    std::cout << "Testing plugin at: C:\\Program Files\\Common Files\\VST3\\Project Seven.vst3" << std::endl;
    
    // Check if the plugin file exists
    std::string dllPath = "C:\Program Files\Common Files\VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3";
    
    HANDLE hFile = CreateFileA(dllPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "ERROR: Cannot open plugin file" << std::endl;
        std::cout << "Error code: " << GetLastError() << std::endl;
        return 1;
    }
    
    std::cout << "Plugin file exists" << std::endl;
    CloseHandle(hFile);
    
    // Try to load the DLL
    HMODULE hModule = LoadLibraryA(dllPath.c_str());
    
    if (hModule == NULL) {
        std::cout << "ERROR: Cannot load plugin DLL" << std::endl;
        std::cout << "Error code: " << GetLastError() << std::endl;
        return 1;
    }
    
    std::cout << "DLL loaded successfully" << std::endl;
    
    // Check for VST3 entry point
    typedef Steinberg::Vst::IPluginFactory* (*CreatePluginFactoryProc)();
    CreatePluginFactoryProc createFunc = (CreatePluginFactoryProc)GetProcAddress(hModule, "CreatePluginFactory");
    
    if (createFunc == NULL) {
        std::cout << "WARNING: CreatePluginFactory not found" << std::endl;
        std::cout << "This might be expected for some VST3 plugins" << std::endl;
    } else {
        std::cout << "CreatePluginFactory found" << std::endl;
        
        // Try to create the plugin factory
        Steinberg::Vst::IPluginFactory* factory = createFunc();
        
        if (factory == NULL) {
            std::cout << "ERROR: Could not create plugin factory" << std::endl;
        } else {
            std::cout << "Plugin factory created successfully" << std::endl;
            factory->release();
        }
    }
    
    FreeLibrary(hModule);
    std::cout << "=== Scan Complete ===" << std::endl;
    
    return 0;
}
