# Akshara for Windows

Akshara is a native, private Sinhala input method for Windows 10 and Windows 11. It provides Smart Phonetic, Classic Phonetic, and Wijesekara/SLS 1134 profiles through the Windows Text Services Framework (TSF).

Typing is processed entirely offline. The TSF DLL contains no networking, telemetry, updater, or typed-text persistence.

## Settings

The installer adds **Akshara Settings** to the Start menu. It follows the Windows Settings visual style and lets each user select Smart Phonetic, Classic Phonetic, or Wijesekara without administrator privileges. The selected mode is also available through **Windows + Space**.

The Typing page controls when an active composition is committed. Changes are picked up when an application or Akshara input profile receives focus.

## Install and use

Akshara supports 64-bit Windows 10 and Windows 11. The installer includes both x64 and x86 text-service DLLs so Akshara works in 64-bit and 32-bit applications on a 64-bit system. ARM64 is compile-checked but is not currently distributed as a native package.

1. Download the signed `Akshara-Windows-vMAJOR.MINOR.PATCH-Setup.exe` from GitHub Releases and run it as an administrator.
2. Restart applications that were open during installation.
3. Press **Windows + Space** and select an Akshara input method. Smart Phonetic is recommended for new users.
4. Open **Akshara** from the Start menu to switch profiles or change composition settings.

Akshara does not contain an updater. Install a newer signed release over the existing version to upgrade. Remove it from **Settings > Apps > Installed apps**, or run the same setup executable with `/uninstall`.

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

## Unsigned developer setup

An unsigned developer installer can be built manually from the GitHub Actions **Developer setup** workflow, or by pushing a `dev-vMAJOR.MINOR.PATCH` tag such as `dev-v0.1.0`. Open the completed workflow run, download the `Akshara-Windows-unsigned-*` artifact, extract it, and run the `Setup.exe` as an administrator. Production `vMAJOR.MINOR.PATCH` tags are reserved for the signed release workflow.

Because this package is not code-signed, Windows may show **Windows protected your PC**. A developer can choose **More info**, verify that the app name is Akshara, and then choose **Run anyway**. The MSI in the same artifact is an alternative and installs the same payload. Do not distribute either unsigned package to end users.

To build the same package on a Windows development machine, use a Developer PowerShell for Visual Studio:

```powershell
$env:WIX_ACCEPT_EULA = 'wix7'
./tools/build-dev-setup.ps1 -Version 0.1.0
```

The setup, MSI, and SHA-256 checksums are written to `dist/dev`. Production releases continue to use the signed tag workflow described in [CODE_SIGNING.md](CODE_SIGNING.md).

Microsoft Store submission is dispatched after a signed GitHub Release is published. It requires a protected `microsoft-store` environment with the Partner Center credentials and product ID referenced by `.github/workflows/store.yml`.
