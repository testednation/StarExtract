# ⭐ Star Extract — Universal Extractor Edition

A modern WPF front-end for 7-Zip and the UniExtract2 tool suite. Browse archive
contents with a real file-manager interface, extract with the right engine
picked automatically, identify any file's true format from its binary
signature, keep codec and unpacker plugins up to date from editable URLs, and
never get stuck on a password again.

![version](https://img.shields.io/badge/version-1.6.0-blue)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![framework](https://img.shields.io/badge/.NET%20Framework-4.8-purple)
![arch](https://img.shields.io/badge/arch-x64%20%7C%20x86-success)

---

## ✨ Features

### 📁 Archive Browser
A file manager for archive contents — no flat listing, no guessing:

- **Folder tree side panel** — resizable, toggleable (`🗂 Tree`), and two-way
  synced with the file grid: click a tree folder to jump there; navigate in the
  grid and the tree expands, selects, and scrolls to match
- **File-manager navigation** — double-click (or Enter) to descend into a
  folder, **⬅ Back / ➡ Forward / ⬆ Up** buttons with full history, a clickable
  **breadcrumb bar** (`archive › docs › deep`), plus a `..` row and **Backspace**
  for quick parent hops
- **Live search** scoped to the current folder
- **Preview files** — double-click any file to extract it to a temp folder and
  open it with the default application
- **Drag & drop anywhere** — plain archives open in the browser; installers and
  MSI packages route straight to the Smart Extractor
- Handles any archive 7-Zip or the plugin suite supports:
  `.7z .zip .rar .tar .gz .xz .zst .iso .cab .wim .msi .pea` and dozens more
- Status bar with file/folder counts, total size, detected engine, and
  encryption state

### 🧠 Smart Extractor
Picks the right engine for the job automatically:

| Engine | Handles |
|---|---|
| **7-Zip core** | 7z, zip, rar, tar, iso, and dozens more |
| **InnoUnp** | Inno Setup installers |
| **Unshield** | InstallShield cabinets |
| **LessMSI** | MSI / MSP packages |
| **PeaZip / Wise / EnigmaVB / GARbro / UnRPA** | Via the UniExtract plugin set |

- Real-time progress, per-file log, optional subfolder, auto-open destination,
  optional source deletion
- Encrypted archives unlock automatically from the password vault

### 🔎 File-Type Scanner
Know what you're looking at before you extract. Right-click any file in
Explorer and pick **Scan file type (TrID)**, or run `StarExtract.exe <file>
/scan`:

- **TrID signature analysis** — matches the file against 22,000+ binary
  definitions to reveal the real format, even with a missing, wrong, or
  extension-less name — with a confidence percentage for each candidate
- **Magika analysis** (optional) — Google's AI-powered detector runs as a second
  opinion when it's on `PATH` or in `bin\magika\`; control it with the
  **Use Magika** checkbox in the scan window (choice persists in settings)
- **Inline scanner downloader** — if TrID or Magika is missing, the scan window
  offers a one-click **⬇️ Download & Install** with a live progress bar,
  speed readout, and Cancel button; TrID + its definition pack come from
  mark0.net, Magika from Google's official GitHub releases
- Themed results window; the **Format & Signature Detectors** entry in the
  Components tab installs/updates TrID and ExeInfo, and a dedicated
  **Magika File-Type Detector (Google AI)** entry covers the AI detector

### 📌 Explorer Context Menu
Integrate into Windows Explorer right-click menus on every file and folder:

- **Open**, **Extract files…**, **Extract Here**, **Extract to <Folder>\**,
  **Test archive**, **Add to archive…**
- One-click compression per file: **Add each file to separate .7z** and
  **Add each file to separate .zip**
- Multi-select combine: **Combine all selected into one .7z** and
  **Combine all selected into one .zip** (single `Combined.7z`/`.zip`)
- **Extract MSI with LessMSI** on `.msi`/`.msp` files (forced LessMSI engine)
- **CRC SHA** submenu — CRC-32 / CRC-64 / SHA-1 / SHA-256 / all hashes
- **Scan file type (TrID)** — signature analysis from the menu
- Flat menu or cascaded under **Star Extract ▸**; optional icons; per-item
  toggles in Settings

### ⚙ Components & Plugins
- One-click **Download / Update All** with per-component progress and speed
- **Editable plugin source URLs** — point any component at a GitHub release,
  mirror, or direct file URL
- **Add custom plugins** — name + URL + optional verification file; archives
  unpack into `bin\<name>\` and are detected automatically
- Plugin bundles default to
  [`https://github.com/gvp9000/UniExtract2/releases`](https://github.com/gvp9000/UniExtract2/releases)
  (v3.0.4 bundle: innounp, unshield, lessmsi, GARbro, pea, unrpa, exeinfope,
  Wise & Enigma unpackers, and 100+ more tools)
- URL edits and custom plugins persist in `%AppData%\StarExtract\components.json`

### 🔐 Password Vault
- Remembers archive passwords encrypted with **DPAPI** (current-user scope)
- Tries saved passwords automatically before prompting
- Registers Explorer double-click behavior: quick-extract or open in browser

### 🔍 UI Zoom
Resize all text and controls together, 50%–200%:

- **Ctrl + `+`** zoom in · **Ctrl + `−`** zoom out · **Ctrl + `0`** reset to 100%
- **Ctrl + mouse wheel** for continuous zoom anywhere in the app
- **− / % / + / ⟲** buttons in the title bar for mouse-only adjustment
- Your chosen level persists across restarts

### 🎨 Themes
Cycle with the **🌗 Theme** button in the title bar — the choice is remembered:

| Theme | Look |
|---|---|
| 🌙 **Dark** | Default charcoal + blue accent |
| ☀️ **Light** | Clean white/gray for bright rooms |
| 🌌 **Midnight** | Deep navy, softer contrast |
| ⚡ **High Contrast** | Pure black/yellow — accessibility friendly |

Every window (main UI, password prompt, quick extract) restyles instantly via
WPF dynamic resources.

---

## 📸 Screenshots

Captured from the live app (Dark theme, default layout). Click for full size.

### 📁 Archive Browser
Folder tree + file grid, breadcrumbs, Back/Forward/Up — the demo archive is
open at root:

![Archive Browser](docs/screenshots/01-browser-root.png)

### 🧠 Smart Extractor
Drop an installer or MSI here; the engine is detected automatically:

![Smart Extractor](docs/screenshots/02-smart-extractor.png)

### ⚙ Components & Updates
Plugin inventory with editable source URLs, download progress, and per-plugin
status:

![Components & Updates](docs/screenshots/03-components.png)

### 🔑 Settings & Vault
Explorer associations, password vault, and behavior toggles:


### 🔎 File-Type Scanner
TrID signature analysis launched from the context menu or `StarExtract.exe
<file> /scan` — here scanning an executable:

![File-Type Scanner](docs/screenshots/05-file-scanner.png)

---

## ⬇️ Scanner auto-download
The File-Type Scanner is self-bootstrapping. Open `/scan` on a machine without
the detectors and the window itself offers to fetch them:

| Scanner | Source | Installed to | Size |
|---|---|---|---|
| TrID executable | `mark0.net/download/trid_w32.zip` | `bin\trid.exe` | ~50 KB |
| TrID definitions | `mark0.net/download/triddefs.zip` | `bin\triddefs.trd` | ~2.8 MB |
| Magika CLI (Google AI) | `github.com/google/magika` releases | `bin\magika\magika.exe` | ~10 MB |

Downloads run inside the scan window with a progress bar, MB counter, and live
speed; **Cancel** aborts mid-transfer. Unpacked with the bundled 7-Zip core, so
no extra dependencies are needed. The same Magika package is available in the
**Components** tab as "Magika File-Type Detector (Google AI)".

---

### 📷 Regenerating screenshots
The screenshots above are captured automatically from the live app by a UI
automation script (WPF UIAutomation + GDI+ screen capture). It builds a demo
archive, drives the real UI through all four pages plus a live extraction and
a `/scan` window, and writes fresh PNGs into `docs/screenshots/`:

```bash
cd src && dotnet build -c Release -p:Platform=x64 && cd ..
powershell -NoProfile -ExecutionPolicy Bypass -File tools\generate-screenshots.ps1
```

The script temporarily pins `%AppData%\StarExtract\settings.json` (dark
theme, browser-on-open, no auto-open of Explorer) and restores your original
settings afterwards. Keep it pure ASCII — Windows PowerShell 5.1 misparses
non-ASCII scripts saved without a BOM.

---

## ⌨ Keyboard Shortcuts

Press **F1** (or **Shift + `/`** outside text fields) any time for the in-app
cheat sheet.

| Shortcut | Action |
|---|---|
| Ctrl + `+` / `−` / `0` | Zoom in / out / reset (also Ctrl+wheel) |
| Double-click | Enter folder · preview-open file |
| Enter | Open the selected item |
| Backspace | Up one folder (in the grid) |
| Alt + `←` / `→` | Back / Forward in folder history |
| Esc | Close the help overlay |

---

## 🚀 Getting Started

### Option 1 — Installer (recommended)
1. Download `StarExtract-1.6.0-setup-x64.exe` (or `-x86` for 32-bit Windows)
   and run it
2. Pick **for all users** or **just for me** on the first setup page
3. Launch, open the **Components** tab, and click **Download / Update All** to
   pull the full UniExtract2 plugin suite (already bundled in the installer —
   this step just verifies it)
4. *(Optional)* In **Settings**, register Explorer double-click associations

### Option 2 — Portable
Copy the app folder (needs `StarExtract.exe` beside `7z.exe` / `7z.dll` and
the `bin\` plugin folder) anywhere you like and run the exe. Nothing is written
outside `%AppData%\StarExtract` for settings.

### Command line
```
StarExtract.exe <archive>             open in browser
StarExtract.exe <archive> /extract    quick-extract immediately
StarExtract.exe <file> /scan          file-type scan (TrID)
StarExtract.exe <target> /add7z       one .7z per target (separate)
StarExtract.exe <target> /addzip      one .zip per target (separate)
StarExtract.exe <f1> <f2>... /combine7z    all inputs into one Combined.7z
StarExtract.exe <f1> <f2>... /combinezip   all inputs into one Combined.zip
StarExtract.exe /register             register Explorer context menu
StarExtract.exe /unregister           remove Explorer context menu
StarExtract.exe /registercascaded     context menu as sub-menu
StarExtract.exe /registermain         context menu flat in main menu
```

Full version is `VERSION.BUILD_NUMBER` (e.g. `1.6.0.1`): it is stamped into
`StarExtract.exe`, the setup names, and every ZIP name. `BUILD_NUMBER`
auto-increments after each local `build-installers` run; CI tags use
`v<version>.<build>` (or append the run number).

### Data & settings locations
| File | Purpose |
|---|---|
| `%AppData%\StarExtract\settings.json` | Theme, zoom, and behavior toggles |
| `%AppData%\StarExtract\components.json` | Edited plugin URLs + custom plugins |
| `%AppData%\StarExtract\vault.dat` | DPAPI-encrypted password vault |
| *(1.4.x or older)* `%AppData%\Easy7ZipModern\` | Legacy data folder — migrated automatically on first 1.5+ launch |
| `<app>\bin\` | Plugin tools (innounp, lessmsi, GARbro, …) |
| `<app>\Codecs\` | Zstandard / Brotli / LZ4 / Lizard codec DLLs |

---

## 📦 Installers

Two official installers ship per release plus portable and setup ZIPs — every
file name carries the full version **including the build number**
(`StarExtract-1.6.0.1-setup-x64.exe`,
`StarExtract-1.6.0.1-portable-x64.zip`, ...):

| Artifact | For |
|---|---|
| `StarExtract-<ver>-setup-x64.exe` | 64-bit Windows (recommended) |
| `StarExtract-<ver>-setup-x86.exe` | 32-bit Windows (runs on x64 too) |
| `StarExtract-<ver>-portable-x64.zip` | 64-bit portable (payload + `bin\` + README) |
| `StarExtract-<ver>-portable-x86.zip` | 32-bit portable (payload + `bin\` + README) |
| `StarExtract-<ver>-setup-x64.zip` | Zipped x64 installer |
| `StarExtract-<ver>-setup-x86.zip` | Zipped x86 installer |

Each carries an **architecture-matched 7-Zip core and codecs** (x64 build →
64-bit 7z + x64 codecs; x86 build → 32-bit equivalents — verified per binary).
Both register a proper Add/Remove Programs entry and support silent install
(`/VERYSILENT /NORESTART`) and silent uninstall.

### Native-only installer
`build-installers.ps1 -NativeOnly` builds a variant that ships **only the C++
Win32 edition** (`StarExtractNative.exe` — no .NET build, no .NET runtime
requirement). Same version stamping and build-number bump; names carry a
`-native` marker:

| Artifact | For |
|---|---|
| `StarExtract-<ver>-native-setup-x64.exe` / `-x86.exe` | Native-only installers |
| `StarExtract-<ver>-native-portable-x64.zip` / `-x86.zip` | Native portable builds |
| `StarExtract-<ver>-native-setup-x*.zip` | Zipped native installers |

### Install modes
Choose on the first setup page, or force from the command line:

| Mode | Command | Location | Uninstall entry |
|---|---|---|---|
| All users (default) | `/ALLUSERS` | `C:\Program Files\Star Extract` | HKLM |
| Current user only | `/CURRENTUSER` | `%LOCALAPPDATA%\Programs\Star Extract` | HKCU |

### Upgrades
The AppId GUID is the stable upgrade code. Installing a newer build over an
existing one replaces it in place, reuses the previous install directory, and
warns before allowing a downgrade. Detection spans registry hives, so a
per-user setup finds an earlier per-machine install. A running app instance is
closed automatically before files are replaced.

### Requirements
- Windows 7 SP1 or newer
- .NET Framework 4.8 (auto-detected at setup, with a download link if missing)
- *Native-only installer: no .NET requirement*

---

## 🛠 Building from Source

**Requirements:** any .NET SDK (the build targets .NET Framework 4.8 via the
`Microsoft.NETFramework.ReferenceAssemblies` NuGet package — no VS targeting
pack needed) and Windows.

### Application
```bash
cd src
dotnet build -c Release -p:Platform=x64   # or -p:Platform=x86
```
Output: `src/bin/<arch>/Release/net48/StarExtract.exe` — run it from a
folder containing the 7-Zip core (`7z.exe`, `7z.dll`), `Codecs\`, and `bin\`.

### Installers
Fast path — everything (build + stage + installers + ZIPs + build-number bump):
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build-installers.ps1            # .NET + native
powershell -NoProfile -ExecutionPolicy Bypass -File build-installers.ps1 -NativeOnly  # native only
```

Manual steps:
1. Build and stage the payload folders (per-arch app binary, arch-matched
   7-Zip core and codecs — x86 binaries are downloaded from the official
   7-Zip and 7-Zip-zstd releases and PE-verified):
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\stage-payload.ps1
   ```
2. Compile with [Inno Setup](https://jrsoftware.org/isinfo.php), overriding the
   defaults as needed:
   ```bash
    ISCC.exe installer/StarExtract-installer.iss /DArch=x64 \
      /DRepoRoot=. /DPayloadRoot="%TEMP%\e7z-payload" /DAppVersion=1.6.0.1
    ISCC.exe installer/StarExtract-installer.iss /DArch=x86 \
      /DRepoRoot=. /DPayloadRoot="%TEMP%\e7z-payload" /DAppVersion=1.6.0.1
   ```
Setup binaries land in `<PayloadRoot>\output`. The `.iss` handles mode
selection, upgrade detection, version metadata, and .NET checks.

### Continuous delivery
Pushing a tag (`v1.2.3` style) triggers
[`.github/workflows/release.yml`](.github/workflows/release.yml): GitHub
Actions builds the x64 and x86 installers from a clean checkout (staging +
compile exactly as above) and attaches both setup exes to the tag's GitHub
Release with generated release notes. It can also be run manually via
*Run workflow*.

---

## 📁 Project Layout

```
StarExtract/
├── installer/
│   └── StarExtract-installer.iss  # parameterized Inno Setup script (x64/x86)
├── src/
│   ├── App.xaml / App.cs             # startup, theme bootstrap, CLI args
│   ├── MainWindow.xaml / .cs         # 4-page shell: browser, extractor, components, settings
│   ├── QuickExtractWindow.*          # one-shot drag-drop extractor
│   ├── PasswordPromptWindow.*        # password dialog
│   ├── ScanWindow.*                  # file-type scan results (TrID/Magika)
│   ├── Models/
│   │   ├── AppSettings.cs            # persisted settings (theme, zoom, toggles)
│   │   ├── ArchiveItem.cs            # archive entry view-model
│   │   └── ComponentItem.cs          # plugin/component view-model
│   └── Services/
│       ├── ExtractorEngine.cs        # engine detection, listing, folder logic, extraction
│       ├── UpdateService.cs          # plugin downloads, installs, persistence
│       ├── ThemeService.cs           # Dark / Light / Midnight / High Contrast palettes
│       ├── PasswordVaultService.cs   # DPAPI vault
│       ├── PasswordPromptHandler.cs  # password callback plumbing
│       └── ShellAssociationService.cs# Explorer integration
└── bin/                              # UniExtract2 plugin suite (downloads/installs land here)
```

---

## ❓ Troubleshooting

- **"Engine: … but plugin missing" / formats not extracting** — open the
  **Components** tab and run **Download / Update All**; the status column tells
  you exactly which verification file is absent
- **Windows SmartScreen / antivirus flags the first launch** — the app and
  installers are unsigned; verify and choose *Run anyway*. The binary is clean
- **A password prompt keeps appearing** — add the password to the vault in
  **Settings**; the app will try saved passwords automatically next time
- **ZIP archives created by some tools show odd entries** — supported: the
  browser deduplicates explicit folder entries and filters 7-Zip's self-listing
- **Settings look corrupted** — delete the file in question under
  `%AppData%\StarExtract\`; it is recreated with defaults on next launch

---

## 📝 Changelog

### 1.6.0
- **Native C++ Win32 edition** — `StarExtractNative.exe` ships alongside the
  .NET app in the native installer variants; no .NET runtime required. The
  native edition is now the recommended artifact (`-native` setup/portable
  files)
- **Component manager in the native edition** — 15-entry catalog (7-Zip core,
  zstd/brotli codecs, UniExtract2 bundle, LessMSI, GARbro, PeaZip, ExeInfoPE,
  TrID + defs, Magika, innounp/unshield/unrpa/wise/enigma) with live download
  (WinINet), SHA-256 pinning, 7-Zip-based install, per-component URL override,
  and custom plugin add/remove
- **Browser upgrades (native)** — full `7z l -slt` metadata (size/packed/
  modified/attributes/encrypted), live search, multi-select, real extraction
  progress with cancel, timestamped log
- Engine-detection parity: InstallShield `ISc(` magic, Wise/Inno byte markers;
  password vault with `%APPDATA%` fallback; `ISc(`-checked CAB routing
- Versioning now stamps `VERSION.BUILD_NUMBER` into the native exe, every
  installer, and every ZIP

### 1.5.0
- **Project renamed to Star Extract** — same engine, new name. The executable is
  now `StarExtract.exe`, per-user data lives in `%AppData%\StarExtract`
  (settings/components/vault are migrated automatically from the old
  `%AppData%\Easy7ZipModern` folder on first launch), and the installer replaces
  earlier "Easy 7-Zip Modern" releases in place (same upgrade code, Explorer
  context-menu entries are re-registered under the new name)
- **Unified downloader** — the Components updater and the File-Type Scanner now
  share one download/install engine (progress, speed readout, cancel)
- **Scanner auto-download with progress** — when TrID or Magika is missing, the
  File-Type Scanner window offers a one-click inline downloader (progress bar,
  speed, cancel) and rescans automatically once installed; TrID's signature
  definitions download alongside the executable
- **Magika on-demand** — new **Use Magika (Google AI detector)** toggle in the
  scan window (persisted in settings) plus a dedicated "Magika File-Type
  Detector (Google AI)" entry in the Components tab
- Scanner logic moved into a dedicated `FileScanService`; smarter output when a
  detector exits without producing results

### 1.4.0
- **File-type scanner** — right-click **Scan file type (TrID)** or `/scan`:
  TrID signature analysis (18,000+ definitions) with confidence ranking,
  optional Magika second opinion, themed results window
- **Scan file type** toggle added to the context-menu settings (default on)
- Version housekeeping: installer metadata and build scripts bumped to 1.4.0

### 1.3.0
- **Explorer context menu** — flat or cascaded integration on every file and
  folder: Open, Extract files…, Extract Here, Extract to <Folder>\, Test,
  Add to archive…, one-click **Add to <name>.7z / <name>.zip** (via 7zG),
  **CRC SHA** submenu (CRC-32/64, SHA-1/256, all hashes)
- Per-item context-menu toggles in Settings with Select All / Deselect All,
  optional icons, one-click register/remove, cascaded ↔ flat switching
- CLI switches: `/register`, `/unregister`, `/registercascaded`,
  `/registermain`, `/add7z`, `/addzip`
- Installer option to add the context menu during setup; uninstall cleans up

### 1.2.0
- **Official x64 & x86 installers** with arch-matched 7-Zip cores/codecs,
  per-machine/per-user mode selection, upgrade-code versioning with cross-hive
  detection, downgrade guard, silent install/uninstall, .NET 4.8 detection
- **File-manager navigation in the Archive Browser** — folder tree side panel
  with two-way grid sync, Back/Forward/Up history, clickable breadcrumbs,
  `..` row, Backspace/Enter shortcuts, per-folder search
- **UI zoom** — Ctrl +/−/0, Ctrl+mouse wheel, title-bar buttons; 50%–200%, persisted
- **Keyboard shortcut help overlay** (F1 / Shift + `/`)
- **Four themes** — Dark, Light, Midnight, High Contrast — instant switching,
  applied to all dialogs, persisted
- **Drag & drop** anywhere in the window; installers auto-route to the Smart Extractor
- Version badge in the title bar; project documentation added

### 1.1.0
- Plugin source moved to `gvp9000/UniExtract2` releases (v3.0.4 bundle)
- Editable plugin URLs, custom plugin support, per-component download progress
- Fixed: downloads extracting to the wrong folder (empty `bin\` subfolders)

### 1.0.0
- Initial release: archive browser, smart extractor, password vault

---

## 🙏 Credits

### Core
- [7-Zip](https://www.7-zip.org) by **Igor Pavlov** — the compression engine,
  `7z.exe` / `7z.dll` core, and SFX modules (GNU LGPL; unRAR code has separate
  restrictions — see `License.txt`)
- [UniExtract2](https://github.com/gvp9000/UniExtract2) — the Universal
  Extractor plugin suite this app drives and bundles (maintained fork of
  Jared Breland's Universal Extractor)
- [7-Zip-zstd](https://github.com/mcmilk/7-Zip-zstd) by **Tino Reichardt** —
  Zstandard / Brotli / LZ4 / Lizard codec DLLs in `Codecs\`

### Bundled extractors & scanners (in `bin\`)
- [LessMSI](https://github.com/activescott/lessmsi) by **Scott Bilas** — MSI/MSP
  extraction
- [innounp](https://sourceforge.net/projects/innounp/) — Inno Setup installer
  unpacker
- [GARbro](https://github.com/morkt/GARbro) by **morkt** — 100+ game
  archive and image formats
- [PeaZip / pea](https://github.com/peazip/PeaZip) by **Giorgio Tani** — PEA and
  ARC containers
- [Magika](https://github.com/google/magika) by **Google** — AI-powered file
  content-type detection (optional second opinion in the File-Type Scanner)
- [TrID](https://mark0.net/soft-trid-e.html) by **Marco Pontello** — file
  identification from binary signatures (powering the File-Type Scanner)
- [ExeInfoPE](https://github.com/ExeInfoASL/ExeInfoPe) — executable
  packer/protector detection
- [Wise & Enigma unpackers](https://github.com/gvp9000/UniExtract2) — legacy
  installer support (`E_WISE_W.EXE`, `EnigmaVBUnpacker.exe`)
- The many additional unpackers shipped with the UniExtract2 bundle — see the
  license files inside `bin\` after install

### Tooling
- [Inno Setup](https://jrsoftware.org/isinfo.php) by **Jordan Russell** and
  **Martijn Laan** — the Windows installers
- [.NET Framework 4.8](https://dotnet.microsoft.com/download/dotnet-framework/net48)
  / **WPF** — application runtime and UI framework
- Built and tested with the open-source .NET SDK; screenshots regenerated via
  PowerShell **UIAutomation**

### Contributors
- The Star Extract project contributors

*Every bundled tool remains the property of its authors and is distributed
under its own license; use `Help → About` and the license files inside `bin\`
and `Codecs\` to review them.*

---

*Licensed under the MIT-style terms of the bundled components; see
`7zip-license.txt` and the licenses inside `bin\` and `Codecs\` after install.*
