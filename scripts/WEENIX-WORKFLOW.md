# Weenix Development Workflow

## Setup

### 1. Configure Your VM Details

Edit these scripts and update the configuration section:
- `sync-to-vm.sh`
- `vm-make.sh`
- `setup-ssh-key.sh`

Update these values:
```bash
VM_USER="your-username"      # Your Ubuntu VM username
VM_IP="192.168.1.100"        # Your VM's IP address
```

To find your VM's IP, run this on the VM:
```bash
ip addr show
```

### 2. Setup SSH Key (Optional but Recommended)

This allows passwordless login so you don't type your password every sync:
```bash
./setup-ssh-key.sh
```

### 3. Place Your Weenix Code in WSL

Make sure your weenix code is at:
```
/home/joseph/weenix-assignment-3.9.0/
```

## Daily Workflow

### Option A: Quick Sync Only
```bash
./sync-to-vm.sh
```
Syncs your code changes to the VM. You then SSH to VM manually to compile.

### Option B: Sync + Build
```bash
./vm-make.sh          # Sync and compile
./vm-make.sh clean    # Sync, make clean, then compile
./vm-make.sh run      # Sync, compile, and run Weenix
```

### Option C: Manual Control
```bash
# 1. Edit code in WSL with Claude Code
claude code /home/joseph/weenix-assignment-3.9.0

# 2. Sync to VM
./sync-to-vm.sh

# 3. SSH to VM and build
ssh your-username@192.168.1.100
cd ~/weenix-assignment-3.9.0/weenix
make
make run
```

## What Gets Synced

The sync script:
- ✓ Syncs all source code files
- ✓ Preserves symbolic links (critical for Weenix)
- ✓ Only copies changed files (fast)
- ✗ Excludes build artifacts (.o, .a, .img files)

## Troubleshooting

**Can't connect to VM:**
- Check VM is running
- Verify VM IP with `ip addr show` on VM
- Ensure SSH is running on VM: `sudo systemctl status ssh`

**Sync is slow:**
- First sync is always slower (copies everything)
- Subsequent syncs only copy changes
- Exclude patterns prevent copying build files

**Permission denied:**
- Run `./setup-ssh-key.sh` for passwordless login
- Or enter your VM password when prompted

**Symlink errors:**
- Never use shared folders or FileZilla
- Always use rsync/scp (these scripts use rsync)
