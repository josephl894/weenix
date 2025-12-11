#!/bin/bash

# Weenix Sync Script - Syncs code from WSL to Ubuntu VM
# Usage: ./sync-to-vm.sh

# Configuration
VM_USER="student"
VM_IP="172.31.0.1"  # Windows host IP from WSL
VM_PORT="5679"
VM_PATH="/home/student/weenix-claude"
LOCAL_PATH="/home/joseph/weenix-claude"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Syncing Weenix to VM...${NC}"

# Sync files (preserves symlinks, only copies changed files)
rsync -avz --delete \
    -e "ssh -p ${VM_PORT}" \
    --exclude='*.o' \
    --exclude='*.img' \
    --exclude='kernel/weenix.bin' \
    --exclude='kernel/weenix.elf' \
    --exclude='user/disk0.img' \
    "${LOCAL_PATH}/" "${VM_USER}@${VM_IP}:${VM_PATH}/"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Sync completed successfully!${NC}"
else
    echo -e "${YELLOW}✗ Sync failed. Check your VM connection.${NC}"
    exit 1
fi
