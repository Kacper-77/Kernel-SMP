#include <vfs.h>
#include <kmalloc.h>
#include <std_funcs.h>
#include <panic.h>

/* Global root of the Virtual File System hierarchy */
static vfs_node_t* vfs_root = NULL;

/*
 * Initializes the VFS by creating the root directory node.
 * Sets up the initial mutex to manage concurrent access to the root.
 */
void vfs_init() {
    vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(vfs_root, 0, sizeof(vfs_node_t));
    
    strcpy(vfs_root->name, "/");
    vfs_root->flags = VFS_DIRECTORY;
    
    // Initialize root mutex: count=1 indicates the lock is available
    vfs_root->lock = (mutex_t){ .count = { .counter = 1 } };
}

vfs_node_t* vfs_get_root() {
    return vfs_root;
}


/*
 * Resolves a path to a VFS node using the "Lock Crawling" (Hand-over-Hand) technique.
 * This ensures SMP safety by locking the current directory before acquiring the next,
 * preventing race conditions during concurrent path resolutions or mount operations.
 */
static vfs_node_t* vfs_find_path(const char* path) {
    if (!path || path[0] != '/') return NULL;
    if (path[1] == '\0') return vfs_root;

    vfs_node_t* current = vfs_root;
    
    // Use a stack-allocated buffer for thread-safety
    char buffer[256];
    strncpy(buffer, path + 1, 256);
    
    char* saveptr;
    char* token = strtok_r(buffer, "/", &saveptr);

    while (token != NULL) {
        mutex_lock(&current->lock);
        
        // Check if the current node is a gateway to another filesystem (Mount Point)
        if (current->ptr) {
            vfs_node_t* mounted = current->ptr;
            // Transition to the mounted root. Ideally, we should lock 'mounted' 
            // before unlocking 'current' to ensure a seamless transition.
            mutex_unlock(&current->lock);
            current = mounted;
            mutex_lock(&current->lock);
        }

        if (current->ops && current->ops->finddir) {
            vfs_node_t* next = current->ops->finddir(current, token);

            mutex_unlock(&current->lock);
            
            if (!next) return NULL;
            current = next;
        } else {
            mutex_unlock(&current->lock);
            return NULL;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    return current;
}

/*
 * Opens a file by resolving its path and executing the driver-specific open routine.
 */
vfs_node_t* vfs_open(const char* path, uint32_t flags) {
    vfs_node_t* node = vfs_find_path(path);
    
    if (node && node->ops && node->ops->open) {
        node->ops->open(node, flags);
    }
    
    return node;
}

/*
 * Closes a file and executes the driver-specific cleanup.
 */
void vfs_close(vfs_node_t* node) {
    if (node && node->ops && node->ops->close) {
        node->ops->close(node);
    }
}

/*
 * Generic read wrapper. 
 * Serializes access to the node via mutex to prevent interleaved I/O operations
 * from multiple CPUs, ensuring data consistency for the caller.
 */
uint32_t vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buffer) {
    if (node && (node->flags & (VFS_FILE | VFS_CHARDEVICE | VFS_PIPE)) && node->ops->read) {
        // Block the node for the reading time
        mutex_lock(&node->lock);
        uint32_t bytes = node->ops->read(node, offset, size, buffer);
        mutex_unlock(&node->lock);
        return bytes;
    }
    return 0;
}

/*
 * Generic write wrapper.
 * Provides atomic write semantics per-node in an SMP environment.
 */
uint32_t vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buffer) {
    if (node && (node->flags & (VFS_FILE | VFS_CHARDEVICE | VFS_PIPE)) && node->ops->write) {
        mutex_lock(&node->lock);
        uint32_t bytes = node->ops->write(node, offset, size, buffer);
        mutex_unlock(&node->lock);
        return bytes;
    }
    return 0;
}

/*
 * Mounts a new filesystem root onto an existing VFS path.
 * Marks the target node as a mount point and links it to the local_root of the new FS.
 */
int vfs_mount(const char* path, vfs_node_t* local_root) {
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;

    mutex_lock(&node->lock);
    node->flags |= VFS_MOUNTPOINT;
    node->ptr = local_root;
    mutex_unlock(&node->lock);

    return 0;
}
