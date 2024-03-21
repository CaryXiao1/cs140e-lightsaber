#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

// Print extra tracing info when this is enabled.  You can and should add your
// own.
static int trace_p = 1; 
static int init_p = 0;

fat32_boot_sec_t boot_sector;

void config_fs(fat32_fs_t *fs, pi_dirent_t *root) {
    printk("Reading the MBR.\n");
    mbr_t *mbr = mbr_read();

    printk("Loading the first partition.\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));

    printk("Loading the FAT.\n");
    *fs = fat32_mk(&partition);

    printk("Loading the root directory.\n");
    *root = fat32_get_root(fs);
}

fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // Read the boot sector (of the partition) off the SD card.
  fat32_boot_sec_t *boot_sec = pi_sec_read(partition->lba_start, 1); 
  boot_sector = *boot_sec; 
  // Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  fat32_volume_id_check(&boot_sector); 
  fat32_volume_id_print("Printing boot_sector", &boot_sector); 

  // Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);
  struct fsinfo *fs_info_ptr = pi_sec_read(partition->lba_start + 1, 1); 
  fat32_fsinfo_check(fs_info_ptr); 
  fat32_fsinfo_print("Printing fsinfo: ", fs_info_ptr); 

  // END OF PART 2
  // The rest of this is for Part 3:

  // calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = -1; // from the partition
  unsigned fat_begin_lba = -1; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = -1; // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = -1; // from the boot sector
  unsigned root_first_cluster = -1; // from the boot sector
  unsigned n_entries = -1; // from the boot sector

  lba_start = partition->lba_start; 
  fat_begin_lba = lba_start + boot_sector.reserved_area_nsec; 
  cluster_begin_lba = fat_begin_lba + boot_sector.nfats * boot_sector.nsec_per_fat; 
  sec_per_cluster = boot_sector.sec_per_cluster; 
  root_first_cluster = boot_sector.first_cluster; 
  n_entries = boot_sector.nsec_per_fat * (boot_sector.bytes_per_sec / 4); // FIXME: Might be nsec / sec_per_cluster
  // unimplemented();

  /*
   * Read in the entire fat (one copy: worth reading in the second and
   * comparing).
   *
   * The disk is divided into clusters. The number of sectors per
   * cluster is given in the boot sector byte 13. <sec_per_cluster>
   *
   * The File Allocation Table has one entry per cluster. This entry
   * uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
   *
   * Store the FAT in a heap-allocated array.
   */
  uint32_t *fat = pi_sec_read(fat_begin_lba, boot_sector.nsec_per_fat); 

  // Create the FAT32 FS struct with all the metadata
  fat32_fs_t fs = (fat32_fs_t) {
    .lba_start = lba_start,
      .fat_begin_lba = fat_begin_lba,
      .cluster_begin_lba = cluster_begin_lba,
      .sectors_per_cluster = sec_per_cluster,
      .root_dir_first_cluster = root_first_cluster,
      .fat = fat,
      .n_entries = n_entries,
  };

  if (trace_p) {
    trace("begin lba = %d\n", fs.fat_begin_lba);
    trace("cluster begin lba = %d\n", fs.cluster_begin_lba);
    trace("sectors per cluster = %d\n", fs.sectors_per_cluster);
    trace("root dir first cluster = %d\n", fs.root_dir_first_cluster);
    trace("n_entries = %d\n", fs.n_entries);
  }

  init_p = 1;
  return fs;
}

// Given cluster_number, get lba.  Helper function.
static uint32_t cluster_to_lba(fat32_fs_t *f, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // Calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster
  unsigned lba = f->cluster_begin_lba + ((cluster_num - 2) * f->sectors_per_cluster); 
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // return the information corresponding to the root directory (just
  // cluster_id, in this case)
  pi_dirent_t pi_dirent = (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // fix this
      .is_dir_p = 1,
      .nbytes = 0,
  };

  // unimplemented();
  return pi_dirent; 
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  // Walk the cluster chain in the FAT until you see a cluster where
  // `fat32_fat_entry_type(cluster) == LAST_CLUSTER`.  Count the number of
  // clusters.

  // Base Case: If start_cluster is a valid cluster
  if (start_cluster == LAST_CLUSTER) 
    return 0; 

  unsigned chain_length = 1; // Starts with start_cluster so at least one 
  uint32_t cluster = fs->fat[start_cluster];
  // printk("start_cluster: %x\n", start_cluster); 
  // printk("next cluster: %x\n", cluster);
  // print_bytes("print fat bytes", (uint8_t*)fs->fat,  16*4);
  while(fat32_fat_entry_type(cluster) != LAST_CLUSTER) {
    chain_length++; 
    cluster = fs->fat[cluster]; 
  }

  return chain_length;
}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
static void read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  // Walk the cluster chain in the FAT until you see a cluster where
  // fat32_fat_entry_type(cluster) == LAST_CLUSTER.  For each cluster, copy it
  // to the buffer (`data`).  Be sure to offset your data pointer by the
  // appropriate amount each time.
  if (start_cluster == LAST_CLUSTER) 
    return; 

  // Copy start_cluster
  uint32_t start_cluster_lba = cluster_to_lba(fs, start_cluster); 
  // printk("start_cluster_lba: %x\n, data_ptr: %x\n", start_cluster_lba, data);
  pi_sd_read(data, start_cluster_lba, fs->sectors_per_cluster); 
  data += (512 * fs->sectors_per_cluster); // Number of bytes in the sectors

  // Start while loop for others
  uint32_t cluster_number = fs->fat[start_cluster];
  // Copy cluster to the buffer
  while(fat32_fat_entry_type(cluster_number) != LAST_CLUSTER) {
    uint32_t cluster_lba = cluster_to_lba(fs, cluster_number); 
    pi_sd_read(data, cluster_lba, fs->sectors_per_cluster); 
    // Replace data
    cluster_number = fs->fat[cluster_number]; 
    data += (512 * fs->sectors_per_cluster); // Number of bytes in the sectors
  }
}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
static pi_dirent_t dirent_convert(fat32_dirent_t *d) {
  pi_dirent_t e = {
    .cluster_id = fat32_cluster_id(d),
    .is_dir_p = d->attr == FAT32_DIR,
    .nbytes = d->file_nbytes,
  };
  // can compare this name
  memcpy(e.raw_name, d->filename, sizeof d->filename);
  // for printing.
  fat32_dirent_name(d,e.name);
  return e;
}

// Gets all the dirents of a directory which starts at cluster `cluster_start`.
// Return a heap-allocated array of dirents.
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  // Figure out the length of the cluster chain (see
  // `get_cluster_chain_length`)
  uint32_t n_clusters = get_cluster_chain_length(fs, cluster_start); 

  // Allocate a buffer large enough to hold the whole directory
  uint32_t buffer_size = n_clusters * fs->sectors_per_cluster * 512; // 512 is nbytes per sector
  uint8_t *dirent_arr = kmalloc(buffer_size);
  fat32_dirent_t *fat32_dirents = (fat32_dirent_t *)dirent_arr;

  // read in the whole directory (see `read_cluster_chain`)
  read_cluster_chain(fs, cluster_start, dirent_arr); 
  // printk("dirent_arr: %x\n", dirent_arr);
  // Iterate through each dirent_arr until we encounter 0x00 byte
  // print_bytes("print dirent_arr", dirent_arr, 16*4);
  // TODO: Find a way to get the correct number of dirents
  unsigned i = 0; 
  while (fat32_dirents[i].filename[0] != 0x00) {
    // printk("fat32_dirents[i].filename: %x\n", fat32_dirents[i].filename[0]);
    i++; 
  }
  // printk("num_dirents: %x\n", i); 

  *dir_n = i; // Number of dirents
  return fat32_dirents; 
}

pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);
  // printk("n_dirents: %x\n", n_dirents);
  // allocate space to store the pi_dirent_t return values
  pi_dirent_t *pi_dirents = kmalloc(n_dirents * sizeof(pi_dirent_t)); 
  
  // iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  unsigned idx = 0;
  for(unsigned i = 0; i < n_dirents; i++) {
    // FIXME: Don't include empty dirents, LFNs, or Volume IDs
    fat32_dirent_t *f32_dirent = &dirents[i]; 
    pi_dirent_t pi_dirent = dirent_convert(f32_dirent); 
    // printk("nbytes: %x\n", pi_dirent.nbytes); 
    // uint8_t is_empty = pi_dirent.nbytes == 0; 
    uint8_t is_lfn = (f32_dirent->attr << 4) >> 4 == 0b1111; 
    uint8_t is_unused = (f32_dirent->filename[0] == 0xe5); 
    // unsigned is_lfn = 0 
    uint8_t is_volume_id = (f32_dirent->attr >> 3) & 1; 

    if (is_lfn || is_unused || is_volume_id) {
      continue; 
    }

    pi_dirents[idx] = pi_dirent; 
    idx++; 
  }

  // create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = pi_dirents,
    .ndirents = idx,
  };
}

static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // iterate through the dirents, looking for a file which matches the
  // name; use `fat32_dirent_name` to convert the internal name format to a
  // normal string.
  for (int i = 0; i < n; i++) {
    fat32_dirent_t dirent = dirents[i]; 
    char name; 
    fat32_dirent_name(&dirent, &name); 
    if (strcmp(&name, filename) == 0) {
      printk("filename: %s\n", filename); 
      return i; 
    }
  }
  // unimplemented();
  return -1;
}

pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // use `get_dirents` to read the raw dirent structures from the disk
  uint32_t dir_n; 
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &dir_n); 

  // Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.
  int dirent_idx = find_dirent_with_name(dirents, dir_n, filename);
  if (dirent_idx == -1) 
    return NULL; 
  

  // allocate enough space for the dirent, then convert
  // TODO: Figure out how to "allocate" enough space for the dirent
  pi_dirent_t *pi_dirent = kmalloc(sizeof(pi_dirent_t)); 
  // (`dirent_convert`) the fat32 dirent into a Pi dirent.
  *pi_dirent = dirent_convert(&dirents[dirent_idx]);
  printk("found pi_dirent filename: %s\n", pi_dirent->name); 
  return pi_dirent;
}

pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // read the dirents of the provided directory and look for one matching the provided name
  pi_dirent_t *pi_dirent = fat32_stat(fs, directory, filename); 

  // Figure out the length of the cluster chain
  uint32_t n_clusters = get_cluster_chain_length(fs, pi_dirent->cluster_id); 
  printk("nclusters: %x\n, cluster_id: %x\n", n_clusters, pi_dirent->cluster_id);
  // allocate a buffer large enough to hold the whole file
  uint32_t buf_size = n_clusters * fs->sectors_per_cluster * 512; 
  uint8_t *pi_dirents = kmalloc(buf_size); 

  // read in the whole file (if it's not empty)
  if (pi_dirent->nbytes > 0) {
    read_cluster_chain(fs, pi_dirent->cluster_id, pi_dirents); 
    // TODO: how to get 
  }
  

  // fill the pi_file_t
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  *file = (pi_file_t) {
    .data = (char*)pi_dirents,
    .n_data = pi_dirent->nbytes,
    .n_alloc = buf_size,
  };
  return file;
}

/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: loop through the entries in the FAT until you find a free one
  // (fat32_fat_entry_type == FREE_CLUSTER).  Start from cluster 3.  Panic if
  // there are none left.
  if (start_cluster < 3) start_cluster = 3;
  unimplemented();
  if (trace_p) trace("failed to find free cluster from %d\n", start_cluster);
  panic("No more clusters on the disk!\n");
}

static void write_fat_to_disk(fat32_fs_t *fs) {
  // TODO: Write the FAT to disk.  In theory we should update every copy of the
  // FAT, but the first one is probably good enough.  A good OS would warn you
  // if the FATs are out of sync, but most OSes just read the first one without
  // complaining.
  if (trace_p) trace("syncing FAT\n");
  unimplemented();

}

// Given the starting cluster index, write the data in `data` over the
// pre-existing chain, adding new clusters to the end if necessary.
static void write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
  // Walk the cluster chain in the FAT, writing the in-memory data to the
  // referenced clusters.  If the data is longer than the cluster chain, find
  // new free clusters and add them to the end of the list.
  // Things to consider:
  //  - what if the data is shorter than the cluster chain?
  //  - what if the data is longer than the cluster chain?
  //  - the last cluster needs to be marked LAST_CLUSTER
  //  - when do we want to write the updated FAT to the disk to prevent
  //  corruption?
  //  - what do we do when nbytes is 0?
  //  - what about when we don't have a valid cluster to start with?
  //
  //  This is the main "write" function we'll be using; the other functions
  //  will delegate their writing operations to this.

  // TODO: As long as we have bytes left to write and clusters to write them
  // to, walk the cluster chain writing them out.
  unimplemented();

  // TODO: If we run out of clusters to write to, find free clusters using the
  // FAT and continue writing the bytes out.  Update the FAT to reflect the new
  // cluster.
  unimplemented();

  // TODO: If we run out of bytes to write before using all the clusters, mark
  // the final cluster as "LAST_CLUSTER" in the FAT, then free all the clusters
  // later in the chain.
  unimplemented();

  // TODO: Ensure that the last cluster in the chain is marked "LAST_CLUSTER".
  // The one exception to this is if we're writing 0 bytes in total, in which
  // case we don't want to use any clusters at all.
  unimplemented();
}

int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);
  if (!fat32_is_valid_name(newname)) return 0;

  // TODO: get the dirents and find the right one
  unimplemented();

  // TODO: update the dirent's name
  unimplemented();

  // TODO: write out the directory, using the existing cluster chain (or
  // appending to the end); implementing `write_cluster_chain` will help
  unimplemented();
  return 1;

}

// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  // TODO: read the dirents and make sure there isn't already a file with the
  // same name
  unimplemented();

  // TODO: look for a free directory entry and use it to store the data for the
  // new file.  If there aren't any free directory entries, either panic or
  // (better) handle it appropriately by extending the directory to a new
  // cluster.
  // When you find one, update it to match the name and attributes
  // specified; set the size and cluster to 0.
  unimplemented();

  // TODO: write out the updated directory to the disk
  unimplemented();

  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed)
  // pointer
  unimplemented();
  pi_dirent_t *dirent = NULL;
  return dirent;
}

// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;
  // TODO: look for a matching directory entry, and set the first byte of the
  // name to 0xE5 to mark it as free
  unimplemented();

  // TODO: free the clusters referenced by this dirent
  unimplemented();

  // TODO: write out the updated directory to the disk
  unimplemented();
  return 0;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  // TODO: edit the directory entry of the file to list its length as `length` bytes,
  // then modify the cluster chain to either free unused clusters or add new
  // clusters.
  //
  // Consider: what if the file we're truncating has length 0? what if we're
  // truncating to length 0?
  unimplemented();

  // TODO: write out the directory entry
  unimplemented();
  return 0;
}

int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: Surprisingly, this one should be rather straightforward now.
  // - load the directory
  // - exit with an error (0) if there's no matching directory entry
  // - update the directory entry with the new size
  // - write out the file as clusters & update the FAT
  // - write out the directory entry
  // Special case: the file is empty to start with, so we need to update the
  // start cluster in the dirent

  unimplemented();
}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}
