#include <tar.h>
#include <vfs.h>
#include <std_funcs.h>
#include <kmalloc.h>

#include <stdint.h>
#include <stddef.h>

static void* tar_base = NULL;
static size_t tar_limit = 0;

typedef struct tar_node_entry {
    vfs_node_t* node;
    struct tar_node_entry* next;
} tar_node_entry_t;

typedef struct {
    tar_node_entry_t* first_child;
} tar_dir_priv_t;

void tar_init(void* address, size_t size) {
    tar_base  = address;
    tar_limit = size;
}

static uint64_t octal_to_int(const char *s, int size) {
    uint64_t res = 0;
    for (int i = 0; i < size && s[i] >= '0' && s[i] <= '7'; i++) {
        res = res * 8 + (s[i] - '0');
    }
    return res;
}

static vfs_node_t* tar_finddir(vfs_node_t* node, const char* name) {
    if (!(node->flags & VFS_DIRECTORY) || !node->private_data) return NULL;
    tar_dir_priv_t* priv = (tar_dir_priv_t*)node->private_data;
    tar_node_entry_t* curr = priv->first_child;
    
    while (curr) {
        if (strcmp(curr->node->name, name) == 0) {
            return curr->node;
        }
        curr = curr->next;
    }
    return NULL;
}

static uint32_t tar_read_op(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;
    memcpy(buffer, (uint8_t*)node->private_data + offset, size);
    return size;
}

void* tar_lookup(const char* filename, size_t* out_size) {
    tar_header_t* header = (tar_header_t*)tar_base;
    uintptr_t end = (uintptr_t)tar_base + tar_limit;

    while ((uintptr_t)header < end && header->name[0] != '\0') {
        if (memcmp(header->magic, "ustar", 5) == 0) {
            uint64_t size = octal_to_int(header->size, 12);
            
            if (strcmp(header->name, filename) == 0) {
                *out_size = size;
                return (void*)((uintptr_t)header + 512);
            }

            uintptr_t offset = 512 + ((size + 511) & ~511);
            header = (tar_header_t*)((uintptr_t)header + offset);
        } else {
            break;
        }
    }
    return NULL;
}

static vfs_ops_t tar_ops = {
    .read = tar_read_op,
    .finddir = tar_finddir,
    .write = NULL, // RAMdisk read only
};

void tar_vfs_mount(void* address, size_t size) {
    vfs_node_t* tar_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(tar_root, 0, sizeof(vfs_node_t));
    
    strcpy(tar_root->name, "initrd");
    tar_root->flags = VFS_DIRECTORY;
    tar_root->ops = &tar_ops;
    
    tar_dir_priv_t* root_priv = kmalloc(sizeof(tar_dir_priv_t));
    root_priv->first_child = NULL;
    tar_root->private_data = root_priv;

    tar_root->lock = (mutex_t){ .count = 1, .wait_lock = { .last_cpu = -1 } };

    tar_header_t* header = (tar_header_t*)address;
    uintptr_t end = (uintptr_t)address + size;

    while ((uintptr_t)header < end && header->name[0] != '\0') {
        if (memcmp(header->magic, "ustar", 5) == 0) {
            uint64_t f_size = octal_to_int(header->size, 12);
            
            vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
            memset(node, 0, sizeof(vfs_node_t));
            
            strncpy(node->name, header->name, 255);
            size_t n_len = strlen(node->name);
            if (n_len > 0 && node->name[n_len-1] == '/') node->name[n_len-1] = '\0';
            
            node->size = f_size;
            node->flags = (header->typeflag == '5') ? VFS_DIRECTORY : VFS_FILE;
            node->ops = &tar_ops;
            
            node->lock = (mutex_t){.count = 1, .wait_lock = {.last_cpu = -1}};

            if (node->flags == VFS_FILE) {
                node->private_data = (void*)((uintptr_t)header + 512);
            } else {
                tar_dir_priv_t* d_priv = kmalloc(sizeof(tar_dir_priv_t));
                d_priv->first_child = NULL;
                node->private_data = d_priv;
            }

            tar_node_entry_t* entry = kmalloc(sizeof(tar_node_entry_t));
            entry->node = node;
            entry->next = root_priv->first_child;
            root_priv->first_child = entry;

            header = (tar_header_t*)((uintptr_t)header + 512 + ((f_size + 511) & ~511));
        } else break;
    }
    vfs_mount("/", tar_root);
}
