#!/bin/bash

# Weenix Reverse Sync Script - Syncs code from VM to WSL
# Usage: ./sync-from-vm.sh

# Configuration
VM_USER="student"
VM_IP="172.31.0.1"  # Windows host IP from WSL
VM_PORT="5679"
VM_PATH="/home/student/weenix-submit"
LOCAL_PATH="/home/joseph/weenix-submit/weenix-bitbucket"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Syncing Weenix from VM to WSL...${NC}"

# Sync files (preserves symlinks, only copies changed files)
rsync -avz --delete \
    -e "ssh -p ${VM_PORT}" \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='*.img' \
    --exclude='kernel/weenix.bin' \
    --exclude='kernel/weenix.elf' \
    --exclude='user/disk0.img' \
    "${VM_USER}@${VM_IP}:${VM_PATH}/" "${LOCAL_PATH}/"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Sync from VM completed successfully!${NC}"
else
    echo -e "${YELLOW}✗ Sync failed. Check your VM connection.${NC}"
    exit 1
fi
