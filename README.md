# FuseFlow — VST3 mic host for Windows

A Windows 11 desktop prototype for a live microphone-effects chain. The interface uses a dark studio-console look with warm amber accents; it is an independent app, not an Arturia product or a modified MiniFuse Control Center.

## What this source project is intended to do

- Receive audio from an audio-device input (including MiniFuse 1).
- Scan a selected folder for VST3 plug-ins and add effects to a serial chain.
- Open each plug-in's own editor, bypass it, reorder it, or remove it.
- Send the processed signal to the selected physical or virtual output endpoint.
- Record the post-effect signal to a 24-bit stereo WAV file.
- Use software input trim and output gain, plus input/output meters.

The project is a **source-code prototype**; it is not a prebuilt `.exe` and has not been compiled on Windows in this workspace. It requires a Windows build with JUCE/CMake dependencies available.

## MiniFuse 1: wet monitoring setup

For the lowest software round-trip latency:

1. Install the current Arturia MiniFuse driver/Control Center, then reboot if its installer asks.
2. Open **AUDIO ROUTING** in FuseFlow and choose the **Arturia MiniFuse ASIO** driver.
3. Select MiniFuse 1 input 1 and MiniFuse 1 output 1/2; start at 48 kHz and a 128-sample buffer. If stable, try 64 samples. If you hear clicks/dropouts, return to 128 or 256.
4. Add your VST3 effects and use the MiniFuse headphones/line outputs.
5. Turn **off the physical Direct Monitor button** on MiniFuse 1 when you want to hear only the VST-processed return. The hardware Direct Monitoring path is dry; FuseFlow cannot insert VST3 processing into it.

The app's **FX MONITOR** button controls FuseFlow's software return only. It does not control or read the MiniFuse's physical Direct Monitor switch.

## Routing to a virtual line (for example, VB-CABLE)

Install the virtual audio device separately. In **AUDIO ROUTING**, choose a Windows audio backend that supports separate input and output endpoints (for example, WASAPI shared mode where available):

- Input: MiniFuse 1 microphone input.
- Output: the virtual cable's playback endpoint, commonly named **CABLE Input**.
- In OBS/Discord, choose the corresponding recording endpoint, commonly named **CABLE Output**.

For the lowest latency, MiniFuse ASIO normally uses the MiniFuse for both input and output. Sending ASIO input to a separate virtual endpoint may require an aggregate/router driver such as Voicemeeter or FlexASIO; this depends on the installed drivers. The Audio Routing window exposes the device choices available to JUCE on your PC.

## Build on Windows 11

### Prerequisites

- Visual Studio 2022 with **Desktop development with C++**.
- CMake 3.22 or newer.
- Internet access for the first CMake configure; JUCE 8.0.11 is fetched from the JUCE GitHub release archive.
- Arturia MiniFuse Windows driver for ASIO at runtime.

### Build

Open **Developer PowerShell for VS 2022** in this folder and run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target FuseFlow
```

The app should be produced under `build\FuseFlow_artefacts\Release\` (the exact subfolder can vary slightly with the JUCE/CMake generator).

Or run `build_windows.ps1` from this folder.

### Build without installing Visual Studio/CMake locally

The repository includes `.github/workflows/windows-build.yml`. Upload the project to a GitHub repository and the hosted Windows 2022 runner will build it and publish a downloadable artifact. You do not need to install a C++ toolchain on your own PC. The first cloud build downloads JUCE, so it can take several minutes.

The workflow is triggered by a push to `main` or manually from the repository's **Actions** tab. When it completes, open the run and download **FuseFlow-Windows-x64** under **Artifacts**. Extract it and run `FuseFlow.exe`. The project source and its JUCE/ASIO licensing notes are included separately in this archive.

## Known prototype limits

- This is an initial VST3 host, not a fully hardened commercial DAW. Third-party plug-ins run in-process; a faulty plug-in can close the host or cause dropouts.
- The first chain is stereo. MiniFuse 1's mono mic input is duplicated to left and right before the VST3 chain.
- It records the processed chain output, not a separate dry track.
- VST3 plug-in state/presets are not yet saved in a session file.
- Some plug-ins add their own latency (look-ahead, linear-phase processing, oversampling). The app does not promise zero latency or plug-in delay compensation.
- Device-routing combinations depend on the Windows driver backend. ASIO is the recommended mode for MiniFuse-to-MiniFuse low-latency monitoring; WASAPI is useful when choosing different input/output endpoints.

## Licensing note

This app uses JUCE and enables its VST3-hosting and ASIO backends. JUCE is available under GPL/commercial terms; review the current JUCE and ASIO-related terms before redistributing a binary or using this project in a closed-source commercial product. The VST3 plug-ins you load have their own licences.
