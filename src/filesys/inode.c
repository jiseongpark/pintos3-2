#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_MAPPING_SECTOR 123
#define INDIRECT_MAPPING_SECTOR 128




/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t direct[DIRECT_MAPPING_SECTOR];   /* Direct mapping sector. */
    disk_sector_t indirect;                        /* Indirect mapping sector. */
    disk_sector_t dindirect;                       /* Doubly indirect mapping sector */

    bool directory;                                /* Check whether it is for directory */ 
    off_t length;                                  /* File size in bytes. */
    unsigned magic;                                /* Magic number. */
  };

struct inode_indirect
{
  disk_sector_t sec_num[INDIRECT_MAPPING_SECTOR];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

static void inode_indirect_free(struct inode_disk *inode_disk, size_t sectors, int level);
static void inode_disk_free(struct inode_disk *inode_disk, size_t sectors);
static bool inode_indirect_alloc(struct inode_disk *inode_disk, size_t sectors, int level);
static bool inode_disk_allocate(struct inode_disk *inode_disk, size_t sectors);
/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  // if (pos < inode->data.length)
  //   return inode->data.start + pos / DISK_SECTOR_SIZE;
  // else
  //   return -1;
  int index = pos/DISK_SECTOR_SIZE;
  struct inode_indirect *temp;
  disk_sector_t result;
  struct inode_indirect *iid = calloc(1, sizeof *temp);
  struct inode_indirect *diid = calloc(1, sizeof *temp);

  if(inode->data.length < pos)
  {
    result = -1;
  }
  else if(index >= 0 && index < 123)
  {
    // printf("direct : direct[%d] = %d\n", index, inode->data.direct[index]);
    result = inode->data.direct[index];
  }
  else if(index >= 123 && index < 123+128)
  {

    buffer_cache_read(inode->data.indirect, (void *) iid);
    result = iid->sec_num[index - 123];
  }
  else if(index >= 123+128 && index < 123+128+128*128)
  {
    // printf("sec_num = %d\n", inode->data.indirect);
    buffer_cache_read(inode->data.dindirect, (void*) iid);
    buffer_cache_read(iid->sec_num[(index-123-128)/128], (void*) diid);
    result = diid->sec_num[(index-123-128)%128];
  }

  free(iid);
  free(diid);
  return result;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool directory)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);

      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->directory = directory;

      if (inode_disk_allocate(disk_inode, sectors))
        {
          // disk_write (filesys_disk, sector, disk_inode);
          buffer_cache_write(sector, disk_inode);
          // if (sectors > 0) 
          //   {
          //     static char zeros[DISK_SECTOR_SIZE];
          //     size_t i;
              
            // for (i = 0; i < sectors; i++) 
              // buffer_cache_write(disk_inode->start + i, zeros);
                // disk_write (filesys_disk, disk_inode->start + i, zeros); 
            // }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  // disk_read (filesys_disk, inode->sector, &inode->data);
  buffer_cache_read(inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          
          free_map_release (inode->sector, 1);
          inode_disk_free(&inode->data, bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      // printf("ID %d\n", sector_idx);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          // disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
          buffer_cache_read(sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // disk_read (filesys_disk, sector_idx, bounce);
          buffer_cache_read(sector_idx, bounce);  
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;


  /* file extension */
  if(byte_to_sector(inode, offset + size - 1) == -1)
  {
    if(!inode_disk_allocate(&inode->data, bytes_to_sectors(offset + size)))
    {
      return 0;
    }
    inode->data.length = offset + size;
    buffer_cache_write(inode->sector, &inode->data);
  }


  while (size > 0) 
    {

      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          // disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
          
          buffer_cache_write(sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
          {
            buffer_cache_read(sector_idx, bounce); 
            // disk_read (filesys_disk, sector_idx, bounce);
          }
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // disk_write (filesys_disk, sector_idx, bounce);
          buffer_cache_write(sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

static bool inode_disk_allocate(struct inode_disk *inode_disk, size_t sectors)
{
  size_t di = sectors > 123 ? 123 : sectors;
  int i=0;
  static char zeros[DISK_SECTOR_SIZE];
  // printf("SECTOR NUM :%d\n", sectors);
  /* direct */
  for(; i < di; i++){
    if(inode_disk->direct[i] == 0){
      if(!free_map_allocate(1, &inode_disk->direct[i]))
        return false;
      buffer_cache_write(inode_disk->direct[i], zeros);

      // printf("%d : direct alloc succeed\n", inode_disk->direct[i]);
    }
  }

  if(sectors <= 123)
    return true;

  /* single indirect */
  size_t idi = sectors - 123 > 128 ? 128 : sectors - 123;

  if(! inode_indirect_alloc(inode_disk, idi, 0))
      return false;

  if(sectors - 123 <= 128)
    return true;

  /* doubly indirect */
  size_t didi = sectors - 123 - 128;
  ASSERT(didi <= 128*128);

  if(!inode_indirect_alloc(inode_disk, didi, 1))
    return false;

  return true;
}

static bool inode_indirect_alloc(struct inode_disk *inode_disk, size_t sectors, int level)
{
  struct inode_indirect * temp;
  struct inode_indirect *iid = calloc(1, sizeof * temp);
  struct inode_indirect *diid = calloc(1, sizeof * temp);
  bool result;
  int i = 0;
  static char zeros[DISK_SECTOR_SIZE];

  // printf("new\n");

  if(level == 0)
  {
    if(inode_disk->indirect != 0){
      buffer_cache_read(inode_disk->indirect, iid);
      goto A;
    }
     
    if(!free_map_allocate(1, &inode_disk->indirect)){
      free(iid);
      free(diid);
      return false;

    }
    
    
    
    // buffer_cache_read(inode_disk->indirect, (void*) iid);
A:

    for(; i < sectors; i++){
      // printf("SEC_NUM[%d] : %d\n",i, iid->sec_num[i]);
      if(iid->sec_num[i] == 0){        
        if(!free_map_allocate(1, &iid->sec_num[i])){
          free(iid);
          free(diid);
          return false;
        }
        // printf("sec_num[%d] = %d\n", i, iid->sec_num[i]);
        
        buffer_cache_write(iid->sec_num[i], zeros);
      }
    }

    buffer_cache_write(inode_disk->indirect, iid);
  }
  


  if(level == 1)
  {
    // printf("entered here mangham\n");
    // printf("dindirect : %x\n", inode_disk->dindirect);
    buffer_cache_write(inode_disk->dindirect, zeros);
    if(!free_map_allocate(1, &inode_disk->dindirect)){
      free(iid);
      free(diid);
      return false;
    }
    // buffer_cache_write(inode_disk->dindirect, zeros);

    int i = 0, j = 0;
    
    
    buffer_cache_read(inode_disk->dindirect, (void*) iid);

    for(; i < sectors / 128 + 1; i++)
    {
      
      // buffer_cache_write(iid->sec_num[i], zeros);
      if(!free_map_allocate(1, &iid->sec_num[i])){
        free(iid);
        free(diid);
        return false;
      }
      // printf("IID SEC : %d\n",iid->sec_num[i]);
      buffer_cache_write(iid->sec_num[i], zeros);
      buffer_cache_read(iid->sec_num[i], (void*) diid);
      int wtf = i == sectors/128 ? sectors%128 : 128;
      // printf("WTH : %d\n", wtf);
      for(j = 0; j < wtf; j++)
      {
        
        if(diid->sec_num[j] == 0){
          if(!free_map_allocate(1,&diid->sec_num[j])){
            free(iid);
            free(diid);
            return false;
          }
          buffer_cache_write(diid->sec_num[j], zeros);
          // printf("diid : %d\n", diid->sec_num[j]);  
        }
      }
      
      buffer_cache_write(iid->sec_num[i], diid);  
    }
    buffer_cache_write(inode_disk->dindirect, iid);
    
  }
  
  // printf("COME HERE\n");
  free(iid);
  free(diid);
  return true;
}

static void inode_disk_free(struct inode_disk *inode_disk, size_t sectors)
{
  size_t di = sectors > 123 ? 123 : sectors;
  int i=0;

  /* direct */
  for(; i < di; i++)
    free_map_release(inode_disk->direct[i], 1);

  if(sectors <= 123)
    return;
  
  /* single indirect */
  size_t idi = sectors - 123 > 128 ? 128 : sectors - 123;

  inode_indirect_free(inode_disk, idi, 0);

  if(idi <= 128)
    return;

  /* doubly indirect */
  size_t didi = sectors - 123 - 128;
  ASSERT(didi <= 128*128);

  inode_indirect_free(inode_disk, didi, 1);
}

static void inode_indirect_free(struct inode_disk *inode_disk, size_t sectors, int level)
{
  struct inode_indirect * temp;
  struct inode_indirect *iid = calloc(1, sizeof * temp);
  struct inode_indirect *diid = calloc(1, sizeof * temp);

  if(level == 0)
  {
    int i = 0;

    buffer_cache_read(inode_disk->indirect, (void*) iid);
    for(; i < sectors; i++){
      free_map_release(iid->sec_num[i], 1);
    }
  }

  if(level == 1)
  {
    int i = 0, j = 0;
    buffer_cache_read(inode_disk->indirect, (void*) iid);
    for(; i < sectors / 128; i++)
    {
      buffer_cache_read(iid->sec_num[j], (void*) diid);
      int wtf = i == sectors/128 -1 ? sectors%128 : 128;
      for(j = 0; j < wtf; j++)
      {
        free_map_release(diid->sec_num[j], 1);
      }
    }
  }

  free(iid);
  free(diid);
}