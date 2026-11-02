import ctypes
import sys

def check_vst3():
    print("=== Quick VST3 Checker ===\n")
    
    vst3_path = r"C:\Program Files\Common Files\VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3"
    
    print(f"Loading: {vst3_path}\n")
    
    try:
        # Load the DLL
        hModule = ctypes.WinDLL(vst3_path)
        print("✓ DLL loaded successfully\n")
        
        # Try to get common VST3 entry points
        entry_points = [
            ("VST3Main", "VST3Main"),
            ("createPluginFilter", "?createPluginFilter@@YAPEAUAEAudioProcessor@@XZ"),
        ]
        
        found = False
        for name, mangled in entry_points:
            try:
                addr = getattr(hModule, mangled)
                if addr:
                    print(f"✓ Found: {name} at {addr}")
                    found = True
            except AttributeError:
                print(f"✗ Missing: {name}")
        
        print()
        if found:
            print("Result: Plugin appears valid")
            print("If Cubase still can't find it, check:")
            print("  - Cubase VST3 cache")
            print("  - Plugin scan in Cubase")
            return 0
        else:
            print("Result: NO entry points found!")
            print("This is a critical problem.")
            return 1
            
    except Exception as e:
        print(f"✗ Cannot load DLL: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(check_vst3())
