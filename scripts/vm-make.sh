#!/bin/bash

# Weenix Build Script - Syncs and builds on VM
# Usage:
#   ./vm-make.sh          # sync + make
#   ./vm-make.sh clean    # sync + make clean
#   ./vm-make.sh run      # sync + make + run weenix

# Configuration
VM_USER="student"
VM_IP="172.31.0.1"  # Windows host IP from WSL
VM_PORT="5679"
VM_PATH="/home/student/weenix-submit"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Step 1: Sync files
echo -e "${BLUE}Step 1: Syncing files to VM...${NC}"
"$(dirname "$0")/sync-to-vm.sh"
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Sync failed. Exiting.${NC}"
    exit 1
fi

# Step 2: Build on VM
echo -e "${BLUE}Step 2: Building on VM...${NC}"

if [ "$1" = "clean" ]; then
    ssh -p "${VM_PORT}" "${VM_USER}@${VM_IP}" "cd ${VM_PATH} && make clean && make"
elif [ "$1" = "run" ]; then
    ssh -p "${VM_PORT}" "${VM_USER}@${VM_IP}" "cd ${VM_PATH} && make && make run"
else
    ssh -p "${VM_PORT}" "${VM_USER}@${VM_IP}" "cd ${VM_PATH} && make"
fi

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build completed successfully!${NC}"
else
    echo -e "${YELLOW}✗ Build failed. Check errors above.${NC}"
    exit 1
fi
