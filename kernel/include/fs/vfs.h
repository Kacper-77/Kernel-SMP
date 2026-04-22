#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <mutex.h>
#include <spinlock.h>

/* Node types */
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06
#define VFS_MOUNTPOINT  0x08

/* Flags */
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      0x0003
#define O_CREAT     0x0100
#define O_DIRECTORY 0x0200

struct vfs_node;

// V-Table
typedef struct vfs_ops {
    uint32_t (*read)(struct vfs_node* node, uint64_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node* node, uint64_t offset, uint32_t size, uint8_t* buffer);
    void     (*open)(struct vfs_node* node, uint32_t flags);
    void     (*close)(struct vfs_node* node);
    
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    struct vfs_dirent* (*readdir)(struct vfs_node* node, uint32_t index);
    
    // !!!
    int (*mkdir)(struct vfs_node* node, const char* name, uint16_t mode);
    int (*create)(struct vfs_node* node, const char* name, uint16_t mode);
} vfs_ops_t;

typedef struct vfs_dirent {
    char name[256];
    uint64_t inode_num;
} vfs_dirent_t;

typedef struct vfs_node {
    char name[256];
    uint32_t flags; // Type (VFS_FILE, etc.)
    uint32_t mask;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;  // Size in bytes
    uint64_t inode_num;
    
    mutex_t lock;

    vfs_ops_t* ops;
    
    void* private_data;
    
    struct vfs_node* ptr;
} vfs_node_t;

typedef struct {
    vfs_node_t* node;
    uint64_t offset;
    uint32_t flags;
    mutex_t fd_lock;
} file_t;

/* VFS API */
void vfs_init();
vfs_node_t* vfs_get_root();

vfs_node_t* vfs_open(const char* path, uint32_t flags);
void vfs_close(vfs_node_t* node);

uint32_t vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buffer);

int vfs_mount(const char* path, vfs_node_t* local_root);

#endif
