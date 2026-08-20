# ETWX
[![Build status](https://img.shields.io/appveyor/build/hfiref0x/etwx?logo=appveyor)](https://ci.appveyor.com/project/hfiref0x/etwx)
![Visitors](https://api.visitorbadge.io/api/visitors?path=https%3A%2F%2Fgithub.com%2Fhfiref0x%2FETWX&label=Visitors&countColor=%23263759&style=flat)

## ETW Explorer
**ETW Explorer** (aka ETWX) is a native Win32 GUI application for inspecting registered ETW providers, active trace sessions, provider metadata and schemas, and captured or replayed ETW events.

**Note:** This tool was developed for internal use and specific research workflows. It is maintained on an "as-needed" basis and may lack standard features you might expect. Administrator privileges are highly recommended for broad live-capture coverage; casual users may find the interface and behavior confusing.

## System Requirements

* **Operating System**: Windows 10 (Version 1809 / Build 17763) or later.
  * **Note**: Legacy OS versions, including Windows 7, are explicitly unsupported due to reliance on modern TDH APIs.
* **Architecture**: x64 only. ARM64 is currently untested due to lack of hardware and x86-32... well, you know.
* **Permissions**: Administrator privileges are highly recommended for broad live-trace enumeration and capture coverage.

## Features

### Provider Explorer

* Enumerates registered ETW providers.
* Displays provider metadata and distinguishes manifest-based providers from legacy MOF providers.
* Supports provider-name filtering within the tree view.
* Supports viewing and editing the security descriptor of the selected provider.
* Allows checkbox selection of providers for active captures.
* Provides context commands to copy provider GUIDs and names.
* Allows navigating from a captured event back to its source provider.

### Session Explorer

* Enumerates visible ETW trace sessions.
* Displays session properties, including session GUIDs and log-file information when available.
* Cross-references providers and active trace sessions.
* Displays enabled-provider levels and keyword information bidirectionally.
* Supports viewing and editing the security descriptor of the selected session.

### Schema & Metadata Browser

* Displays manifest-provider schemas and their XML source.
* Reconstructs legacy MOF event-class definitions through WMI metadata enumeration (best-effort).
* Supports WPP event decoding when a TMF search path is configured.
* Supports best-effort reconstruction of manifest XML from available provider metadata.
* Presents provider metadata through a unified metadata view regardless of the underlying schema format.

### Live Capture & Replay

* Creates a dedicated real-time ETW session named ETWXLiveSession.
* Enables all checked providers using the selected level and MatchAnyKeyword mask.
* Supports Event ID filtering at both the ETW enablement layer and as a defensive secondary filter in the event callback.
* Writes real-time captures directly to an ETL file.
* Replays existing ETL files through the same event-processing pipeline used for live capture.
* Supports pause/resume and autoscroll.
* Provides event-type colorization for easier visual inspection.
* Supports a configurable ring-buffer depth for live captures.

### Captured Event Inspection

* Displays event time, provider, Event ID, level, keyword, and decoded properties.
* Provides a modeless Event Details window containing provider GUID, Activity ID, Related Activity ID, and formatted event properties.
* Provides context-menu actions for copying Activity IDs, opening event details, and navigating to the source provider.
* Implements Activity ID-based event highlighting with persistent backing state.

### Data Export & User Settings

* Exports the current in-memory event list to UTF-8 encoded CSV with proper CSV quoting.
* Persists window placement, capture levels, keyword filters, and Event ID filters under HKCU\Software\ETWX.
* Supports self-relaunch through the Shell `runas` verb when elevated access is required.

# Build

* Visual Studio 2026 with the **C++ desktop workload** and the configured x64 MSVC toolset.
* Latest Windows SDK is recommended.
* Administrator privileges are recommended for broad live-capture coverage.

## How It's Organized

```text
Source/
  main.cpp          Process entry point, main window procedure, and UI event routing.
  capture.cpp       Live-session creation, provider enabling, ETL replay, and event callback.
  etwman.cpp        ETW manifest XML reconstruction (best-effort).
  etwmeta.cpp       ETW provider/session discovery, TDH decoding, and manifest/MOF schema loading.
  etwmof.cpp        ETW provider MOF reconstruction (best-effort).
  security.c        System security dialog interface implementation.
  sup.cpp            UI helpers, virtual ListView formatting, sorting, export, settings, and menus.
  commonDlg.cpp     Filter, About, modeless captured-event details, and Find dialogs.

  global.h          Shared includes and global APP_CTX application state.
  types.h           Provider/session/schema/live-event backing-store types.
  const.h           Commands, limits, view columns, timers, node encoding, and program constants.
  capture.h         Capture/replay entry points.
  etwmeta.h         ETW/TDH/EVT/WMI-facing application interfaces.
  security.h        Interface for the system security dialog implementation.
  sup.h             Shared support/UI helper interfaces.
  commonDlg.h       Common-dialog interfaces.

  prtl/
    prtl.h          Small string helpers.
    prtl.c          String-helper implementation.

  rsrc/
    rsrc.rc         Version information, icons, and keyboard accelerators.
    resource.h      Resource and command IDs.
    app.manifest    Common Controls v6, as-invoker execution, and compatibility metadata.
    *.ico           Main/tree/toolbar/status icons.

  tests/
    tests.cpp       Various test code used during debug and development.
```

# License

MIT

# Authors

(c) 2025 - 2026 ETWX Project, hfiref0x
