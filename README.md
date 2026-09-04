# Akshara for Windows

Akshara is a native, private Sinhala input method for Windows 10 and Windows 11. It provides Smart Phonetic, Classic Phonetic, and Wijesekara/SLS 1134 profiles through the Windows Text Services Framework (TSF).

Typing is processed entirely offline. The TSF DLL contains no networking, telemetry, updater, or typed-text persistence.

## Build

Portable core (macOS/Linux):

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Windows (Visual Studio 2022 with the C++ desktop workload):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build/x64 -C Release --output-on-failure
```

Use `windows-x86` for 32-bit binaries and `windows-arm64` for compile validation. Release artifacts are built, signed, smoke-tested, and published only by the tag workflow. WiX Toolset 7.0.0 is pinned through its MSBuild SDK.

The release installer supports interactive use and silent Store use:

```text
Akshara-Windows-v1.0.0-Setup.exe
Akshara-Windows-v1.0.0-Setup.exe /quiet /norestart
Akshara-Windows-v1.0.0-Setup.exe /uninstall /quiet /norestart
```

