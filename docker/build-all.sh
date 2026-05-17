#!/bin/bash
# Build BobMan Python for all platforms using Docker
# Usage: ./build-all.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Building BobMan Python for all platforms${NC}"
echo -e "${YELLOW}========================================${NC}"

# Create output directory
mkdir -p "$PROJECT_DIR/build/python"

# Build for Linux
echo -e "\n${GREEN}Building Linux version...${NC}"
docker build -f "$SCRIPT_DIR/Dockerfile.linux" -t bobman-python-linux "$PROJECT_DIR"
docker create --name bobman-linux-temp bobman-python-linux
mkdir -p "$PROJECT_DIR/build/python/linux"
docker cp bobman-linux-temp:/app/BobMan "$PROJECT_DIR/build/python/linux/BobMan"
docker rm bobman-linux-temp
echo -e "${GREEN}Linux build complete!${NC}"

# Build for Windows
echo -e "\n${GREEN}Building Windows version...${NC}"
docker build -f "$SCRIPT_DIR/Dockerfile.windows" -t bobman-python-windows "$PROJECT_DIR"
docker create --name bobman-windows-temp bobman-python-windows
mkdir -p "$PROJECT_DIR/build/python/windows"
docker cp bobman-windows-temp:/app/BobMan.exe "$PROJECT_DIR/build/python/windows/BobMan.exe"
docker rm bobman-windows-temp
echo -e "${GREEN}Windows build complete!${NC}"

# Build for macOS (experimental - may not produce runnable binary)
echo -e "\n${YELLOW}Building macOS version (experimental)...${NC}"
docker build -f "$SCRIPT_DIR/Dockerfile.macos" -t bobman-python-macos "$PROJECT_DIR"
docker create --name bobman-macos-temp bobman-python-macos
mkdir -p "$PROJECT_DIR/build/python/macos"
docker cp bobman-macos-temp:/app/BobMan "$PROJECT_DIR/build/python/macos/BobMan"
docker rm bobman-macos-temp
echo -e "${YELLOW}macOS build complete (may require native build for full compatibility)${NC}"

echo -e "\n${YELLOW}========================================${NC}"
echo -e "${GREEN}All builds complete!${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "\nBuild artifacts:"
echo "  Linux:   $PROJECT_DIR/build/python/linux/BobMan"
echo "  Windows: $PROJECT_DIR/build/python/windows/BobMan.exe"
echo "  macOS:   $PROJECT_DIR/build/python/macos/BobMan"
echo -e "\n${YELLOW}Note: macOS build may not run without native compilation${NC}"
