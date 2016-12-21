#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include "fileManager.h"
#include "disk.h"

int is_valid_fn(char*,char*);
int is_valid_fd(int,char*);
void errmsg(char*,char*);
void print_oft(void);
int file_exists(char*);

oft_t* oft;
directory_t* directory;
superblock_t* superblock;
inode_bmp_t* inode_bmp;
data_bmp_t* data_bmp;
map_t* map;

struct superblock_t {
  uint8_t dir_offset, dir_last, inode_bmp, data_bmp, inode_region, inode_region_end, data_region, data_region_end;
  uint64_t unused; };
struct directory_t {
  uint8_t status, inode_blk_num;
  char fn[4];
  unsigned short fl;
  uint64_t unused; };
struct oft_t {
  int status, offset, dir_index; };
struct inode_bmp_t {
  uint8_t imap;
  uint8_t unused[15]; };
struct data_bmp_t {
  uint64_t dmap;
  double unused; };
struct map_t {
  uint8_t data_block[64]; };


int make_fs(char *disk_name) {
  // valid disk name
  if(!is_valid_fn(disk_name,"make_fs")) return -1;
  // make disk
  if(make_disk(disk_name) == -1) { return -1; }
  // open disk
  if(open_disk(disk_name) == -1) { return -1; }
  // initialize superblock, directory, i-node map, and data map on disk 
  superblock = (superblock_t*)calloc(16,1);
  superblock->dir_offset = 1;
  superblock->dir_last = 8;
  superblock->inode_bmp = 9;
  superblock->data_bmp = 10;
  superblock->inode_region = 11;
  superblock->inode_region_end = 42;
  superblock->data_region = 64;
  superblock->data_region_end = 127;
  if(block_write(0, (char*)superblock) == -1) { return -1; }
  free(superblock);
  // close disk
  if(close_disk(disk_name) == -1) { return -1; }
  return 0; }


int mount_fs(char *disk_name) {
  // open disk
  if(open_disk(disk_name) == -1) { return -1; }
  // create OFT in memory
  oft = (oft_t*)calloc(4,sizeof(oft_t));
  if(!oft) { return -1; }
  // load superblock
  superblock = calloc(16,1);
  if(block_read(0,(char*)superblock) == -1) { return -1; }
  // load directory into memory
  directory = calloc(8,16);
  int i,j;
  for(i = 0; i < 8; i++) {
    if(block_read((int)superblock->dir_offset + i, (char*)(directory + i)) == -1) { return -1; } }
  // load inode bitmap
  inode_bmp = calloc(1,sizeof(inode_bmp_t));
  if(block_read((int)superblock->inode_bmp,(char*)inode_bmp) == -1) { return -1; }
  // load data bitmap
  data_bmp = calloc(1,sizeof(data_bmp_t));
  if(block_read((int)superblock->data_bmp,(char*)data_bmp) == -1) { return -1; }
  // load inode-data map
  map = calloc(8,sizeof(map_t));
  for(i = 0; i < 32; i++) {
    //printf("reading data into map %d from block %x\n",superblock->inode_region+i,(char*)map+i*16);
    if(block_read((int)superblock->inode_region + i, (char*)(map) + i*16) == -1) { return -1; } }
  return 0; }


int dismount_fs(char *disk_name) {
  int i,j;
  // write superblock to disk
  if(block_write(0, (char*)superblock) == -1) { return -1; }
  // write data bitmap to disk
  if(block_write((int)superblock->data_bmp, (char*)data_bmp) == -1) { return -1; }
  // write inode bitmap to disk
  if(block_write((int)superblock->inode_bmp, (char*)inode_bmp) == -1) { return -1; }
  // write directory to disk
  for(i = 0; i < 8; i++) {
    if(block_write((int)superblock->dir_offset + i, (char*)(directory + (i) )) == -1) { return -1; } }
  // write inode-data map to disk
  for(i = 0; i < 32; i++) {
    if(block_write((int)superblock->inode_region + i, (char*)(map) + i*16 ) == -1) { return -1; } }
  // destroy data structures
  free(directory);
  free(inode_bmp);
  free(data_bmp);
  free(oft);
  free(map);
  free(superblock);
  if(close_disk(disk_name) == -1) { return -1; }
  return 0; }


int fs_create(char *name) {
  if(!is_valid_fn(name,"fs_create")) return -1;
  int i, dir_index = -1;
  // check if the directory is already full, or if
  // the file to create already exists, and return
  // an index in the directory
  for(i = 0; i < 8; i++) {
    if(strncmp(directory[i].fn, name, 4) == 0) { return -1; }
    if(directory[i].status == 0) {
      if(dir_index == -1) { dir_index = i; } } }
  // no empty directory slots
  if(dir_index == -1) { return -1; }
  // find an available inode
  for(i = 7; i > -1; i--) {
    if( ((inode_bmp->imap >> i) & 1) == 0) {
      inode_bmp->imap |= 1 << i;
      break; } }
  strncpy(directory[dir_index].fn, name, 4);
  directory[dir_index].status = 1;
  directory[dir_index].inode_blk_num = (uint8_t)superblock->inode_region + (4 * (7 - i));
  directory[dir_index].fl = 0;
  return 0; }


int fs_open(char *name) {
  int i, k, found = 0;
  // check if file has been created
  for(i = 0; i < 8; i++) {
    if(strncmp(name, directory[i].fn, 4) == 0)  {
      found++; k = i; break; } }
  if(found == 0) { return -1; }
  // check if file isn't already open
  if(file_is_open(name)) { return -1; }
  //Checking if there are already 4 entries in the OFT
  // if not, return file descriptor
  for(i = 0; i < 4; i++) {
    if(oft[i].status == 0) {
      oft[i].status = 1;
      oft[i].dir_index = k;
      oft[i].offset = 0;
      return i; }
  }
  return -1; }


int fs_close(int fildes) {
  if(!is_valid_fd(fildes,"fs_close")) return -1;
  int index, i;
  oft[fildes].status = 0;
  oft[fildes].offset = 0;
  oft[fildes].dir_index = 0;
  // writing directory to disk
  for(i = 0; i < 8; i++) {
    if(block_write((int)superblock->dir_offset + i, (char*)(directory + i)) == -1) { return -1; } }
  return 0; }


int fs_delete(char *name) {
  int i, dir_index = file_exists(name);
  if(dir_index == -1 || file_is_open(name)) { return -1; }
  // metadata collection
  int inode_blk_num = directory[dir_index].inode_blk_num;
  int length = directory[dir_index].fl;
  int inode = (inode_blk_num - superblock->inode_region) / 4;
  // free inode
  inode_bmp->imap ^= 1 << (7 - inode);
  // free directory entry
  directory[dir_index].inode_blk_num = 0;
  directory[dir_index].fl = 0;
  memset(directory[dir_index].fn, 0, 4); 
  directory[dir_index].status = 0;
  // clear data blocks
  char buf[BLOCK_SIZE];
  memset(buf, 0, BLOCK_SIZE);
  int block;
  for(i = 0; i < (int)ceil((double)length/16); i++) {
    block = map[inode].data_block[i];
    // clear data
    block_write(block, buf);
    // clear data bitmap entry
    data_bmp->dmap ^= (uint64_t)1 << (uint64_t)(63 - block); // correct
    // clear inode mapping entry
    map[inode].data_block[i] = 0; }
  return 0; }


int fs_get_filesize(int fildes) {
  if(!is_valid_fd(fildes,"fs_get_filesize")) return -1;
  // return the length (easy)
  return directory[oft[fildes].dir_index].fl; }


int fs_lseek(int fildes, off_t offset) {
  if(!is_valid_fd(fildes,"fs_lseek")) return -1;
  int len = directory[file_exists(directory[oft[fildes].dir_index].fn)].fl;
  if((int)(len + offset) < 0 || (len + offset) > len) { return -1; }
  // update offset
  oft[fildes].offset += (int)offset;
  return 0; }


int fs_read(int fildes, void *buf, size_t nbyte) {
  int i=0, bytes_read = 0, buffer_offset=0, j;
  int file_len = fs_get_filesize(fildes);
  char temp_buf[file_len];
  char temp_data_buf[nbyte];
  memset(temp_buf, 0 , nbyte);
  if(!is_valid_fd(fildes,"fs_read")) return -1;
  if(nbyte > 0) {
    if (oft[fildes].status == 1) {
      uint8_t index_dir = oft[fildes].dir_index;
      if(file_is_open(directory[index_dir].fn)) {
	while (bytes_read <= file_len){
	  int data_block = 64+map[index_dir].data_block[i];//+(int)ceil(((double)oft[fildes]offset)/16);
	  i += 1;
	  //printf("Block %d read\n", data_block);
	  block_read(data_block, temp_buf+buffer_offset);
	  buffer_offset += 16;
	  bytes_read += 16;
	}
	//printf("buffer content: %s\n", temp_buf);
	for (j = oft[fildes].offset; j < oft[fildes].offset+nbyte; j++) {
	  temp_data_buf[j - oft[fildes].offset] = temp_buf[j];
	}
	memcpy(buf, temp_data_buf, sizeof(temp_data_buf));
	//updating the offset
	if(bytes_read == 0) return 0;
	else {
	  int file_len = fs_get_filesize(fildes);
	  if (oft[fildes].offset + nbyte > file_len)
	    oft[fildes].offset = file_len;
	  else
	    oft[fildes].offset += nbyte-1;
	  return sizeof(temp_data_buf);
	}
      }
      else return -1;
    }
    else return -1;
  }
  else return -1; }


int fs_write(int fildes, void *buf, size_t nbyte) {
  if(!is_valid_fd(fildes,"fs_write")) return -1;
  int dir_index = file_exists(directory[oft[fildes].dir_index].fn);
  int inode_blk_num = directory[dir_index].inode_blk_num;
  int inode = (inode_blk_num - superblock->inode_region) / 4;
  int offset = oft[fildes].offset, len = directory[dir_index].fl, i, j = len/16, written = 0, bytes_to_write = 0, block_offset, block;
  int blocks = (int)ceil((double)nbyte / 16), nbytes = (int)nbyte;
  char tempBuf[16];
  // write data until nothing is left in write buffer
  while(nbytes > 0) {
    // does the file need another block?
    if( (offset == len) && (len % 16 == 0) ) {
      for(i = 63; i > -1; i--)
	if( ((data_bmp->dmap >> i) & 1) == 0) { data_bmp->dmap |= ((uint64_t)1 << i); break; }
      // if no blocks are available, fail
      if(i == -1) { break; }
      // otherwise, give the block to the file
      //printf("giving file %d block %d\n",inode,63-i);
      map[inode].data_block[len/16] = (uint8_t) (63-i); }
    // offset calculation and setup
    block = superblock->data_region + (int)map[inode].data_block[(int)(offset / 16)];
    bytes_to_write = (int)ceil( (double)(offset+1) / 16) * 16 - offset;
    if(bytes_to_write > nbytes) { bytes_to_write = nbytes; }
    block_offset = offset % 16;
    memset(tempBuf, 0, 16);
    // write (always guaranteed to succeed)
    block_read(block, tempBuf);
    strncpy(tempBuf + (offset % 16), buf + written, bytes_to_write);
    block_write(block, tempBuf);
    // update counters and offsets
    written += bytes_to_write;
    offset += bytes_to_write;
    nbytes -= bytes_to_write;
    if(offset > len) { len = offset; }
  }
  // update offset
  oft[fildes].offset = offset;
  // update length
  directory[dir_index].fl = len;
  return written; }


int fs_truncate(int fildes, off_t length) {
  if(!is_valid_fd(fildes,"fs_read")) return -1;
  if(length < 0 || length > fs_get_filesize(fildes)) return -1;
  int file_length = fs_get_filesize(fildes);
  int actual_num_of_blocks = (int)ceil((double)file_length / 16);
  int num_of_blocks = (int)ceil((double)length / 16);
  int partial_block_bytes = length % 16;
  int num_of_partial_blocks = 0;
  uint8_t index_dir = oft[fildes].dir_index;
  char empty_buf[16];
  char temp_buf[partial_block_bytes];
  memset(empty_buf, 0, 16);
  if(partial_block_bytes > 0) {
    num_of_blocks -= 1;
    num_of_partial_blocks = 1;
  }
  int i, j;
  for(i = 1; i <= actual_num_of_blocks; i++) {
    if (i > num_of_blocks) {
      if (num_of_partial_blocks > 0) {
	int data_block = 64+map[index_dir].data_block[i-1];
	//printf("%d datablock read, partial_bytes: %d\n", data_block, partial_block_bytes);
	block_read(data_block, temp_buf);
	//printf("data: %s\n", temp_buf);
	for (j = 0; j < 16; j++) {
	  if (j > partial_block_bytes-1)
	    temp_buf[j]='\0';
	}
	block_write(data_block, temp_buf);
	num_of_partial_blocks--;

      }
      else {
	//Emptying the excess data and setting the inode region accordingly
	int to_empty_block = 64+map[index_dir].data_block[i-1];
	map[index_dir].data_block[i-1] = 0;
	//printf("block to empty: %d\n",to_empty_block-64);
	data_bmp->dmap ^= (uint64_t)1 << (uint64_t)63-to_empty_block; // correct
	block_write(to_empty_block, empty_buf);
      }
    }
  }
  //updating the file length in directory and setting offset to zero
  oft[fildes].offset = 0;
  directory[index_dir].fl = length;
  return 0; }






// HELPER METHODS
// is the name valid?
int is_valid_fn(char *f, char* n) {
  if(strlen(f) > 4 || strlen(f) < 1) {
    return 0;
  } else {
    int i = 0;
    while(f[i]) {
      if(isalpha(f[i++])) { continue; }
      else { return 0; } } }
  return 1; }
// is the file in the OFT?
int is_valid_fd(int fd, char* n) {
  if(fd > 3 || fd < 0) { return -1; }
  if(oft[fd].status == 1) {
    return 1;
  } else {
    return 0; } }
// custom perror
void errmsg(char *func, char *buf) {
  fprintf(stderr,"ERROR in function %s -> %s\n",func,buf); }
// display OFT
void print_oft() {
  int i;
  printf("fd\t\toffset\t\tdir index\n");
  for(i = 0; i < 4; i++) {
    printf("%d\t\t%d\t\t%d\n",oft[i].status,oft[i].offset,oft[i].dir_index);
  } }
// if file exists, return its directory index
int file_exists(char *name) {
  int i, k, found = 0;
  for(i = 0; i < 8; i++) {
    if(strncmp(name, directory[i].fn, 4) == 0)  {
      found++; k = i; break; } }
  if(found == 0) { return -1; }
  return k; }
// TRUE if file is in OFT
int file_is_open(char* name) {
  int i;
  for(i = 0; i < 4; i++) {
    if(strncmp(name,directory[oft[i].dir_index].fn, 4) == 0 && oft[i].status == 1) {
      return 1; } }
  return 0; }
