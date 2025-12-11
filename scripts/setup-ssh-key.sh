#!/bin/bash

# SSH Key Setup Script - Enables passwordless SSH to VM
# Usage: ./setup-ssh-key.sh

# Configuration
VM_USER="student"
VM_IP="172.31.0.1"  # Windows host IP from WSL
VM_PORT="5679"

echo "Setting up SSH key for passwordless login to VM..."
echo ""

# Check if SSH key already exists
if [ ! -f ~/.ssh/id_rsa ]; then
    echo "Generating SSH key..."
    ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N ""
else
    echo "SSH key already exists at ~/.ssh/id_rsa"
fi

# Copy public key to VM
echo ""
echo "Copying SSH key to VM..."
echo "You will be prompted for your VM password one last time."
ssh-copy-id -p "${VM_PORT}" "${VM_USER}@${VM_IP}"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ SSH key setup complete!"
    echo "You can now SSH to your VM without a password."
    echo ""
    echo "Test it with: ssh -p ${VM_PORT} ${VM_USER}@${VM_IP}"
else
    echo ""
    echo "✗ SSH key setup failed."
    exit 1
fi
