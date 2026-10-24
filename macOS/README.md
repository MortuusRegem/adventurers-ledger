# Adventurer's Ledger — Mac version

Requires macOS 13 Ventura or later. Choose arm64 for Apple Silicon (M1, M2, M3 and newer) or x86_64 for Intel. Apple menu > About This Mac shows your chip or processor.

1. Open the downloaded .dmg file.
2. Drag Adventurers Ledger onto Applications.
3. Open it from Applications.
4. This build is not Apple-notarized. If macOS blocks it because the developer cannot be verified, open System Settings > Privacy & Security and choose Open Anyway for Adventurers Ledger. Confirm Open and authenticate if requested. Only approve a copy downloaded from this project's releases.

Apple's instructions: https://support.apple.com/102445

## Your characters and settings

Characters are saved in your user Library/Application Support/AdventurersLedger/AdventurersLedger/characters folder, separately from the app. Use the app's import and export commands to exchange character files with Windows users. Installing an update does not replace that folder.

Appearance > Dark mode switches between parchment and dark themes and remembers your choice on this Mac.

## Updating

Choose Rules and help > Check for updates. The app downloads the matching Mac disk image, verifies its checksum, saves your character and backs up the collection. It then opens the disk image and closes. Drag the new app into Applications and choose Replace. You may need to approve the new version in Privacy & Security again.

## Building

The macOS workflow builds and tests both architectures using Qt 6.8.3. It bundles Qt frameworks, so players do not need Qt or development tools installed. The bundle has an ad-hoc signature for integrity; it does not claim an Apple-verified developer identity. No paid Apple account or signing credentials are used.

Local build on a Mac with Qt installed:

    export QT_ROOT_DIR=/path/to/Qt/6.8.3/macos
    cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
    cmake --build build-macos --parallel 3
    QT_QPA_PLATFORM=offscreen "build-macos/Adventurers Ledger.app/Contents/MacOS/Adventurers Ledger" --self-test
    bash package-macos.sh

Only source files and embedded application catalogs are in this folder. Personal characters, original uploaded PDFs, build backups and local filesystem locations are excluded.
