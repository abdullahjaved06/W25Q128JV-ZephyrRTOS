#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// Declare default LittleFS config for partition "storagefs"
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storagefs);

static struct fs_mount_t fs_mnt = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .fs_data = &storagefs,
    .storage_dev = (void *)FIXED_PARTITION_ID(storagefs),
};

static void append_to_file(const char *filename, const char *data)
{
    struct fs_file_t file;
    char path[128];

    snprintf(path, sizeof(path), "/lfs/logs/%s", filename);
    fs_file_t_init(&file);

    int rc = fs_open(&file, path, FS_O_CREATE | FS_O_APPEND | FS_O_WRITE);
    if (rc < 0) {
        LOG_ERR("Failed to open %s for writing (%d)", path, rc);
        return;
    }

    rc = fs_write(&file, data, strlen(data));
    if (rc < 0) {
        LOG_ERR("Write failed (%d)", rc);
    } else {
        LOG_INF("Appended to %s", path);
    }

    fs_close(&file);
}

static void read_file(const char *filename)
{
    struct fs_file_t file;
    char path[128], buf[128];
    ssize_t len;

    snprintf(path, sizeof(path), "/lfs/logs/%s", filename);
    fs_file_t_init(&file);

    int rc = fs_open(&file, path, FS_O_READ);
    if (rc < 0) {
        LOG_ERR("Failed to open %s for reading (%d)", path, rc);
        return;
    }

    len = fs_read(&file, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        LOG_INF("Read from %s: %s", path, buf);
    } else {
        LOG_ERR("Read failed or empty");
    }

    fs_close(&file);
}

static void list_files(const char *dirpath)
{
    struct fs_dir_t dir;
    struct fs_dirent entry;
    fs_dir_t_init(&dir);

    int rc = fs_opendir(&dir, dirpath);
    if (rc < 0) {
        LOG_ERR("Failed to open dir %s (%d)", dirpath, rc);
        return;
    }

    LOG_INF("Files in %s:", dirpath);

    while (fs_readdir(&dir, &entry) == 0 && entry.name[0] != 0) {
        LOG_INF("  %c %s (%u bytes)",
                entry.type == FS_DIR_ENTRY_FILE ? 'F' : 'D',
                entry.name, entry.size);
    }

    fs_closedir(&dir);
}

void main(void)
{
    int rc;
    //  LOG_INF("Erasing flash partition...");
    // erase_full_flash_partition();
    // erase_full_flash_partition();
    LOG_INF("Mounting LittleFS...");
    rc = fs_mount(&fs_mnt);
    if (rc < 0) {
        LOG_ERR("Mount failed: %d", rc);
        return;
    }

    LOG_INF("Mounted LittleFS at %s", fs_mnt.mnt_point);

    // Ensure directory exists
    rc = fs_mkdir("/lfs/logs");
    if (rc < 0 && rc != -EEXIST) {
        LOG_ERR("mkdir failed: %d", rc);
    } else {
        LOG_INF("Directory /lfs/logs is ready.");
    }

    // // Append sample data
    // append_to_file("data.txt", "Hello from Zephyr!\n");

    // // Read the file
    // read_file("data.txt");

    // // List files in logs dir
    // list_files("/lfs/logs");
}

void erase_full_flash_partition(void)
{
    const struct flash_area *fa;
    const struct device *flash_dev;
    int rc;
    off_t offset = 0;
    size_t erase_size = 4096; // W25Q128JV sector size = 4KB

    // Open your partition (example: storagefs)
    rc = flash_area_open(FIXED_PARTITION_ID(storagefs), &fa);
    if (rc < 0) {
        LOG_ERR("Failed to open flash area (%d)", rc);
        return;
    }

    flash_dev = flash_area_get_device(fa);
    if (!flash_dev) {
        LOG_ERR("Failed to get flash device");
        flash_area_close(fa);
        return;
    }

    LOG_INF("Erasing flash area: Offset=0x%08x, Size=%u bytes", (uint32_t)fa->fa_off, (uint32_t)fa->fa_size);

    // Erase block-by-block
    while (offset < fa->fa_size) {
        rc = flash_erase(flash_dev, fa->fa_off + offset, erase_size);
        if (rc != 0) {
            LOG_ERR("Failed to erase at 0x%08x (err %d)", (uint32_t)(fa->fa_off + offset), rc);
            flash_area_close(fa);
            return;
        }
        offset += erase_size;
    }

    flash_area_close(fa);

    LOG_INF("Flash erase completed successfully!");
}