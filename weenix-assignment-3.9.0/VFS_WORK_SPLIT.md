# Kernel 2 VFS Work Split

## Person A: Core Path & File Operations (12 tasks)

### Path Resolution (`kernel/fs/namev.c`)
1. **lookup()** - Look up name in directory, call vn_ops->lookup()
2. **dir_namev()** - Parse path, return parent dir + basename
3. **open_namev()** - Get vnode, handle O_CREAT flag

### File Operations (`kernel/fs/open.c` + `kernel/fs/vfs_syscall.c`)
4. **do_open()** - Get fd, create file_t, call open_namev()
5. **do_read()** - fget(), call vn_ops->read(), update f_pos
6. **do_write()** - fget(), handle FMODE_APPEND, call vn_ops->write()
7. **do_close()** - fput() and clear p_files[fd]
8. **do_dup()** - fget(), get_empty_fd(), share file_t
9. **do_dup2()** - Like do_dup() but to specific fd
10. **do_lseek()** - Calculate new f_pos based on whence
11. **do_stat()** - open_namev(), call vn_ops->stat()
12. **do_getdent()** - fget(), call vn_ops->readdir()

---

## Person B: Directory/Device Ops & Initialization (11 tasks)

### Directory Operations (`kernel/fs/vfs_syscall.c`)
1. **do_mkdir()** - dir_namev(), check not exists, call vn_ops->mkdir()
2. **do_rmdir()** - Check not "." or "..", call vn_ops->rmdir()
3. **do_unlink()** - dir_namev(), call vn_ops->unlink()
4. **do_link()** - open_namev(from), dir_namev(to), call vn_ops->link()
5. **do_rename()** - Call do_link() then do_unlink()
6. **do_mknod()** - Validate mode, dir_namev(), call vn_ops->mknod()
7. **do_chdir()** - open_namev(), swap curproc->p_cwd

### Device Files (`kernel/fs/vn_special.c`)
8. **special_file_read()** - Get vn_cdev, call cd_ops->read()
9. **special_file_write()** - Get vn_cdev, call cd_ops->write()

### Initialization (`kernel/main/kmain.c`)
10. **Set p_cwd** - Line 149: `curproc->p_cwd = vfs_root_vn; vref(vfs_root_vn);`
11. **Create devices** - Line 154: mkdir /dev, mknod null/zero/tty0/tty1/tty2

---

## Shared Setup

### Config.mk
- Set `VFS=1`, `DRIVERS=1`, `S5FS=0`

### Testing (Add to kmain.c - either person)
- kshell command `vfstest` → run vfstest_main() in separate process
- kshell command `faber_fs` → run faber_fs_thread_test() directly
- kshell command `faber_dir` → run faber_directory_test() directly

---

## Key Constants
- `NAME_LEN = 28`
- `MEM_NULL_DEVID = MKDEVID(1,0)`
- `MEM_ZERO_DEVID = MKDEVID(1,1)`
- TTY devices: `MKDEVID(2, minor)` where minor = 0,1,2

## Critical
- Always fput()/vput() to avoid leaks
- Return negative errno codes
- Check error conditions per function comments
