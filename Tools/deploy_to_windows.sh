#!/bin/bash
# Deploy RocketAR UE project files to Windows filesystem.
# Usage: ./Tools/deploy_to_windows.sh [destination]
# Default: /mnt/c/UE_Projects/RocketAR

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DEST="${1:-/mnt/c/Users/tpolson/Documents/Unreal Projects/RocketAR}"

echo "Deploying RocketAR to: $DEST"
echo "Source: $REPO_ROOT"

mkdir -p "$DEST"

# --- UE project files ---
cp "$REPO_ROOT/RocketAR.uproject" "$DEST/"

# Config
mkdir -p "$DEST/Config"
cp "$REPO_ROOT/Config/"*.ini "$DEST/Config/"

# Source (game module + build targets)
mkdir -p "$DEST/Source/RocketARGame"
cp "$REPO_ROOT/Source/RocketAR.Target.cs" "$DEST/Source/"
cp "$REPO_ROOT/Source/RocketAREditor.Target.cs" "$DEST/Source/"
cp "$REPO_ROOT/Source/RocketARGame/"* "$DEST/Source/RocketARGame/"

# Plugin
mkdir -p "$DEST/Plugins/RocketAR/Source/RocketAR/Public"
mkdir -p "$DEST/Plugins/RocketAR/Source/RocketAR/Private/Tests"
cp "$REPO_ROOT/Plugins/RocketAR/RocketAR.uplugin" "$DEST/Plugins/RocketAR/"
cp "$REPO_ROOT/Plugins/RocketAR/Source/RocketAR/RocketAR.Build.cs" "$DEST/Plugins/RocketAR/Source/RocketAR/"
cp "$REPO_ROOT/Plugins/RocketAR/Source/RocketAR/Public/"*.h "$DEST/Plugins/RocketAR/Source/RocketAR/Public/"
cp "$REPO_ROOT/Plugins/RocketAR/Source/RocketAR/Private/"*.cpp "$DEST/Plugins/RocketAR/Source/RocketAR/Private/"
cp "$REPO_ROOT/Plugins/RocketAR/Source/RocketAR/Private/Tests/"*.cpp "$DEST/Plugins/RocketAR/Source/RocketAR/Private/Tests/"

# Content (CSV test data)
mkdir -p "$DEST/Content/Data"
cp "$REPO_ROOT/Content/Data/SimulatedTelemetry.csv" "$DEST/Content/Data/"

echo ""
echo "Deployed successfully!"
echo "Windows path: $(echo "$DEST" | sed 's|/mnt/c/|C:\\|' | sed 's|/|\\|g')"
echo ""
echo "Next steps:"
echo "  1. Install Cesium for Unreal from Marketplace"
echo "  2. Open RocketAR.uproject in UE 5.7"
