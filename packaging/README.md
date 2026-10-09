# Packaging SavvyCAN

Scripts for producing distributable packages.

## Windows installer (`.exe`)

`build-windows-installer.ps1` builds the release binary, deploys the required
Qt/MinGW runtime next to it, stages a clean payload and compiles a Windows
installer with [NSIS](https://nsis.sourceforge.io/) (`makensis`).

```powershell
# prerequisites: Qt (mingw) + MinGW in the paths below, and NSIS installed
#   scoop install nsis
.\packaging\build-windows-installer.ps1
```

The installer is written to `build\installer\SavvyCAN-<ver>-win64-setup.exe`.
It installs to `%ProgramFiles%\SavvyCAN`, adds Start Menu / desktop shortcuts
and registers an uninstaller.

`installer.nsi` is the NSIS script used by the PowerShell wrapper. It must be
saved as **UTF-8 with BOM** (the wrapper handles this automatically) because it
contains Chinese text and NSIS otherwise mis-detects the encoding.
