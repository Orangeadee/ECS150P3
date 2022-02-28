#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define SIGNATURE "ECS150FS"
#define FILE_SIZE 4
#define FAT_EOC 0xFFFF

enum type {FAT,ROOT,CURR_FILE,FREE_FD};

/*
 * 								*
 * Superblock:							*
 * - Using "__attribute__" to pack data	so that	insist particu-	*
 *   lar sized padding, also avoid meaningless bytes allocated	*
 * - Superblock contains 7 pieces of data: Signature, total amt *
 *   of blocks of virtual disk, root dir block index, data block*
 *   start index, amt of data blocks, num of blocks for FAT, as	*
 *   well as unused/padding.					*
 *   								*
 */

// attribute ref: 
typedef struct {
	uint8_t sig[8];
	uint16_t amt_blk;
	uint16_t root_dir_index;
	uint16_t data_blk_index;
	uint16_t amt_blk_data;
	uint8_t amt_blk_FAT;
	uint8_t unused[4079];
}__attribute__((packed)) SuperBlock;

uint16_t *fat;

/*	Root Directory 		*/
typedef struct {
	uint8_t fileName[FS_FILENAME_LEN];
	uint32_t size;
	uint16_t first_data_blk_index;
	uint8_t unused[10];
}__attribute__((packed)) RootDirectory;

/*	File Descriptor		*/
typedef struct {
	int offset;
	uint8_t fileName[FS_FILENAME_LEN];
	uint16_t index;
} FileDescriptor;

SuperBlock *super;
RootDirectory *root_dir;
FileDescriptor fd_arr[FS_OPEN_MAX_COUNT];

/* TODO: Phase 1 */

/* helper functions section */

/* https://stackoverflow.com/questions/3437404/min-and-max-in-c */
 #define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// Follow the fat table and clear all data
int remove_file(int index)
{
	uint16_t curr = root_dir[index].first_data_blk_index;
	while(curr != FAT_EOC)
	{
		uint16_t temp = curr;
		fat[temp] = 0;
		curr = fat[curr];
	}

	// clear all data from root directory
	fat[curr] = 0;
	root_dir[index].size = 0;
	root_dir[index].fileName[0] = '\0';
	root_dir[index].first_data_blk_index = 0;
	return 0;
}

// return the first free index in root dir, fat table, file descriptor
// and the index of matched filename in root directory.
int get_index(enum type t, const char* file)
{
	int index = -1;
	int i = 0;
	switch(t)
	{
		case ROOT:
			for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
			{
				if(root_dir[i].fileName[0] != '\0'){continue;}
				else
				{
					index = i;
					break;
				}
			}
			break;
			
		case FAT:
			for(int i = 0; i < super->amt_blk_data; i++)
			{
				if(fat[i] != 0){continue;}
				else
				{
					fat[i] = FAT_EOC;
					index = i;
					break;
				}
			}
			break;
		
		case CURR_FILE:
			for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
			{
				if(!strcmp(file,(char*)root_dir[i].fileName))
				{
					index = i;
					break;
				}
			}

		case FREE_FD:
			while(i < FS_OPEN_MAX_COUNT)
			{
				char file_ch = fd_arr[i].fileName[0];
				if(file_ch == '\0')
				{
					index = i;
					break;
				}
				i++;
			}
	}
	return index;
}

/* function that returns the index of the DB corresponding
 to the fd’s offset          						*/
int get_DBindex_offset(int fd)
{
	enum type t = CURR_FILE;
	int root_index = get_index(t, (const char *)fd_arr[fd].fileName);
	if (root_index == -1) {
		perror("get_root_index failed");
		exit(1);
	}
	uint16_t curr = root_dir[root_index].first_data_blk_index;
	int num_offset_block = fd_arr[fd].offset / BLOCK_SIZE;

	for (int i = 0; i < num_offset_block; i++)
		curr = fat[curr];
	return curr;
}

// counting number of free fat and free root dir
int get_ratio(enum type r)
{
	int count = -1;
	switch(r)
	{
		case ROOT:
			count = 0;
			for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
			{
				if(root_dir[i].fileName[0] != '\0'){}
				else // if empty, increment count
				{
					count++;
				}

			}
			break;

		case FAT:
			count = 0;
			for(int i = 0; i < super->amt_blk_data; i++)
			{
				if(fat[i] == 0)
				{
					count++;
				}
			}
			break;

		case CURR_FILE:
			break;

		case FREE_FD:
			break;
	}
	return count;
}

// checking errors before initializing
int init_super_check()
{
	int num_fat_blk;
	uint16_t fat_size = super->amt_blk_data*2;
	assert(super->amt_blk == block_disk_count());
	num_fat_blk = fat_size/4096;
	if(fat_size % 4096 == 0){}
	else
	{
		num_fat_blk++;
	}
	if(super->amt_blk_FAT == num_fat_blk && 
	   super->root_dir_index == 1+super->amt_blk_FAT &&
	   super->data_blk_index == 1+super->root_dir_index &&
	   super->amt_blk_data == super->amt_blk - super->data_blk_index)
	{
		return 0;
	}
	else
	{
		printf("init error\n");
		return -1;
	}
}


int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	assert(block_disk_open(diskname) != -1);
	super = (SuperBlock*)malloc(sizeof(SuperBlock));
	fat = (uint16_t*)malloc(super->amt_blk_FAT*sizeof(SuperBlock));
	puts("\0"); // if remove this, it gets error
	
	root_dir = (RootDirectory*)malloc(sizeof(RootDirectory));
	assert(super);
	assert(fat);
	assert(root_dir);

	assert(block_read(0,super) != -1);
	// check if signature matches "ECS150FS"
	int i = 0;
	while(SIGNATURE[i] != '\0')
	{
		char current_ch = super->sig[i];
		if(current_ch == SIGNATURE[i]){}
		else
		{
			return -1;
		}
		i++;
	}
	int status = init_super_check();
	if(status == -1)
	{
		return -1;
	}
	int index = 1;
	i = 0;
	while(i <= super->amt_blk_FAT-1)
	{
		// read thru every fat block and make sure they can be read
		assert(block_read(index,fat+((BLOCK_SIZE/2)*i)) != -1);
		i++;
		index++;
	}
	// check if we can read the root directory
	assert(block_read(super->root_dir_index,root_dir) != -1);
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	// check if we can write back super block to disk
	assert(block_write(0,super) != -1);
	if(block_disk_count() == -1) return -1;
	int status;
	status = block_write(super->root_dir_index,root_dir);
	if(status == -1) return -1;
	status = block_write(0,super);
	if(status == -1) return -1;
	status = block_write(1,fat);
	if(status == -1) return -1;
	status = block_write(2,fat+4096);
	if(status == -1) return -1;
	assert(block_disk_close() != -1);
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	if(block_disk_count() == -1 || super == NULL)
	{
		return -1;
	}
	enum type t;
	t = FAT;
	int fat_free = get_ratio(t);
	t = ROOT;
	int root_free = get_ratio(t);

	// printing info
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", (int)super->amt_blk);
	printf("fat_blk_count=%d\n", (int)super->amt_blk_FAT);
	printf("rdir_blk=%d\n", (int)super->root_dir_index);
	printf("data_blk=%d\n", (int)super->data_blk_index);
	printf("data_blk_count=%d\n", (int)super->amt_blk_data);
	printf("fat_free_ratio=%d/%d\n", fat_free, (int)super->amt_blk_data);
	printf("rdir_free_ratio=%d/%d\n", root_free, (int)FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	if(filename == NULL)
	{
		return -1;
	}
	if(strlen(filename) <= 0)
	{
		perror("filename too short\n");
		return -1;
	}
	if(strlen(filename) > FS_FILENAME_LEN)
	{
		perror("filename too long\n");
		return -1;
	}

	// loop through root dir to see if filename exists already or not
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(strcmp(filename,(char*)root_dir[i].fileName)){}
		else
		{
			return -1;
		}
	}

	// if filename does not exist in root dir, get the first available
	// free entry in root dir and fat
	const char* file = filename;
	enum type t;
	t = ROOT;
	int root_index = get_index(t,file);
	t = FAT;
	int fat_index = get_index(t,file);

	// if no available entries anymore, return -1
	if(root_index == -1 || fat_index == -1)
	{
		return -1;
	}

	// if an empty entry is found, update new entry and modify its size
	else
	{
		root_dir[root_index].size = 0;
		strcpy((char*)root_dir[root_index].fileName,filename);
		root_dir[root_index].first_data_blk_index = fat_index;
		return 0;
	}
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	if(filename == NULL)
	{
		perror("file not exist\n");
		return -1;
	}
	if(strlen(filename) <= 0 || strlen(filename) > FS_FILENAME_LEN)
	{
		perror("filename too short/long\n");
		return -1;
	}
	const char* file = filename;
	enum type t = CURR_FILE;
	int index = get_index(t,file);
	if(index == -1) return -1;

	// go thru file descriptors to see if file is already opened
	int i = 0;
	while(i < FS_OPEN_MAX_COUNT)
	{
		if(strcmp(filename,(char*)fd_arr[i].fileName))
		{
			i++;
		}
		else
		{
			return -1;
		}
	}

	// finish checking all senarios, start deleting the file
	int status = remove_file(index);
	if(status == -1)
	{
		return -1;
	}
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	if(root_dir == NULL || super == NULL || block_disk_count() == -1)
	{
		return -1;
	}
	// printing
	printf("FS Ls:\n");
	int i = 0;
	while(i < FS_FILE_MAX_COUNT)
	{
		if(root_dir[i].fileName[0] != '\0')
		{
			int size = (int)root_dir[i].size;
			char *fileName = (char*)root_dir[i].fileName;
			int data_blk_index = (int)root_dir[i].first_data_blk_index;
			printf("file: %s, size: %d, data_blk: %d\n",fileName,size,data_blk_index);
		}
		i++;
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	if (filename == NULL) {
		perror("file not exist\n");
		return -1;
	}

	if (strlen(filename) <= 0 || strlen(filename) > FS_FILENAME_LEN) {
		perror("filename too short/long\n");
		return -1;
	}

	/* there is no file named @filename to open */
	enum type t = CURR_FILE;
	if (get_index(t, filename) == -1)
		return -1;
	
	/* there are already %FS_OPEN_MAX_COUNT files currently open */
	t = FREE_FD;
	int free_fd_index = get_index(t, filename);
	if (free_fd_index == -1)
		return -1;
	
	fd_arr[free_fd_index].offset = 0;
	strcpy((char*)fd_arr[free_fd_index].fileName, filename);
	fd_arr[free_fd_index].index = free_fd_index;
	return free_fd_index;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		perror("fd out of bounds\n");
		return -1;
	}

	if (fd_arr[fd].fileName[0] == '\0') {
		perror("fd not currently open\n");
		return -1;
	}

	fd_arr[fd].offset = 0;
	const char* default_file_name = '\0';
	strcpy((char*)fd_arr[fd].fileName, default_file_name);
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		perror("fd out of bounds\n");
		return -1;
	}

	if (fd_arr[fd].fileName[0] == '\0') {
		perror("fd not currently open\n");
		return -1;
	}

	enum type t = CURR_FILE;
	int root_index = get_index(t, (const char *)fd_arr[fd].fileName);
	return root_dir[root_index].size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		perror("fd out of bounds\n");
		return -1;
	}

	if (fd_arr[fd].fileName[0] == '\0') {
		perror("fd not currently open\n");
		return -1;
	}

	/* if @offset is larger than the current file size */
	if (offset > (long unsigned int)fs_stat(fd)) {
		perror("offset is larger than the current file size\n");
		return -1;
	}

	fd_arr[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		perror("fd out of bounds\n");
		return -1;
	}

	if (fd_arr[fd].fileName[0] == '\0') {
		perror("fd not currently open\n");
		return -1;
	}

	if (buf == NULL) {
		perror("buf cannot be NULL\n");
		return -1;
	}

	size_t num_bytes_written = 0;
	enum type t = CURR_FILE;
	int root_index = get_index(t, (const char *)fd_arr[fd].fileName);
	size_t file_size = root_dir[root_index].size;
	/* Index of where the offset is at in terms of DB */
	uint16_t offset_DB = get_DBindex_offset(fd);

	if (count == 0)
		return num_bytes_written = 0;

	int num_blk_to_write;
	size_t front_mismatch = fd_arr[fd].offset % BLOCK_SIZE;
	size_t rear_mismatch;
	
	if ((count + front_mismatch) % BLOCK_SIZE == 0)
		/* bytes needed to be written match excatly for whole blocks of BLOCK_SIZE */
		num_blk_to_write = (count + front_mismatch) / BLOCK_SIZE;
	else
		/* write one more block to include the last portion of user buf for later 
		insertion */
		num_blk_to_write = (count + front_mismatch) / BLOCK_SIZE + 1;

	if (num_blk_to_write > 1)
		rear_mismatch = 0;
	
	void *bounce_buf = (void *)malloc(BLOCK_SIZE);
	int i = 1;
	while (i <= num_blk_to_write) {
		/* While loop hit the last block, update rear mismatch for possible
		extraction */
		if (i == num_blk_to_write) 
			rear_mismatch = num_blk_to_write * BLOCK_SIZE - (count + front_mismatch);
		
		/* For block that doesn't span the whole block */
		if (front_mismatch != 0 || rear_mismatch != 0) {
		    size_t num_bytes = BLOCK_SIZE - front_mismatch - rear_mismatch;
			block_read(super->data_blk_index + offset_DB, bounce_buf);
			memcpy(buf + num_bytes_written, bounce_buf + front_mismatch, num_bytes);
			num_bytes_written += num_bytes;
		} else { /* For whole block */
			block_read(super->data_blk_index + offset_DB, buf + num_bytes_written);
			num_bytes_written += BLOCK_SIZE;
		}

		offset_DB = fat[offset_DB];

		/* If there is more than 1 data block to be read, the front mismatch for
		 all following blocks doesn't exist */
		if (i == 1)
			front_mismatch = 0;
		i++;
	}
	
	return num_bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		perror("fd out of bounds\n");
		return -1;
	}

	if (fd_arr[fd].fileName[0] == '\0') {
		perror("fd not currently open\n");
		return -1;
	}

	if (buf == NULL) {
		perror("buf cannot be NULL\n");
		return -1;
	}

	size_t num_bytes_read;
	enum type t = CURR_FILE;
	int root_index = get_index(t, (const char *)fd_arr[fd].fileName);
	size_t file_size = root_dir[root_index].size;
	/* Index of where the offset is at in terms of DB */
	uint16_t offset_DB = get_DBindex_offset(fd);

	/* The number of bytes read can be smaller than @count if there are less than
     * @count bytes until the end of the file (it can even be 0 if the file offset
     * is at the end of the file)*/
	count = MIN(file_size - fd_arr[fd].offset, count);
	if (count == 0)
		return num_bytes_read = 0;
	
	int num_blk_to_read;
	/* Front mismatch off fd.offset in the first block for later calculation of 
	num_blk_to_read */
	size_t front_mismatch = fd_arr[fd].offset % BLOCK_SIZE;
	size_t rear_mismatch;
	
	if ((count + front_mismatch) % BLOCK_SIZE == 0)
		/* bytes needed to be read match excatly for whole blocks of BLOCK_SIZE */
		num_blk_to_read = (count + front_mismatch) / BLOCK_SIZE;
	else
		/* read one more block to include the last portion of data for later 
		extraction */
		num_blk_to_read = (count + front_mismatch) / BLOCK_SIZE + 1;

	/* If there is more than 1 data block to be read, the rear mismatch for 
	first block doesn't exist */
	if (num_blk_to_read > 1)
		rear_mismatch = 0;

	/* Use block_read to read data block into buf one block at a time and 
	extract non-whole block based on the scenarios */ 
	void *bounce_buf = (void *)malloc(BLOCK_SIZE);
	int i = 1;
	while (i <= num_blk_to_read) {
		/* While loop hit the last block, update rear mismatch for possible
		extraction */
		if (i == num_blk_to_read) 
			rear_mismatch = num_blk_to_read * BLOCK_SIZE - (count + front_mismatch);
		
		/* For block that doesn't span the whole block */
		if (front_mismatch != 0 || rear_mismatch != 0) {
		    size_t num_bytes = BLOCK_SIZE - front_mismatch - rear_mismatch;
			block_read(super->data_blk_index + offset_DB, bounce_buf);
			memcpy(buf + num_bytes_read, bounce_buf + front_mismatch, num_bytes);
			num_bytes_read += num_bytes;
		} else { /* For whole block */
			block_read(super->data_blk_index + offset_DB, buf + num_bytes_read);
			num_bytes_read += BLOCK_SIZE;
		}

		offset_DB = fat[offset_DB];

		/* If there is more than 1 data block to be read, the front mismatch for
		 all following blocks doesn't exist */
		if (i == 1)
			front_mismatch = 0;
		i++;
	}
	// free(bounce_buf);
	/* The file offset of the file descriptor is implicitly incremented by the 
	   number of bytes that were actually read*/
	fd_arr[fd].offset += num_bytes_read;
	return num_bytes_read;
}

