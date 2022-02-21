#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define BLOCK_SIZE 4096
#define SIGNATURE "ECS150FS"
#define NUM_ROOT_ENTRIES 128
#define ENTRY_SIZE 32
#define FILENAME_LEN 16
#define FILE_SIZE 4
#define FAT_EOC 0xFFF

enum ratio {FAT,ROOT};

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

uint16_t *fat = NULL;

/*	Root Directory 		*/
typedef struct {
	uint8_t fileName[FILENAME_LEN];
	uint32_t size;
	uint16_t first_data_blk_index;
	uint8_t unused[10];
}__attribute__((packed)) RootDirectory;

SuperBlock *super;
RootDirectory *root_dir;

/* TODO: Phase 1 */

/* helper functions section */
int get_ratio(enum ratio r)
{
	int count = -1;
	switch(r)
	{
		case ROOT:
			count = 0;
			for(int i = 0; i < NUM_ROOT_ENTRIES; i++)
			{
				if(root_dir[i].fileName[0] != '\0'){}
				else
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
	}
	return count;
}
int init_super_check()
{
	int num_fat_blk;
	uint16_t fat_size = super->amt_blk_data*2;
	assert(super->amt_blk == block_disk_count());
	num_fat_blk = fat_size/4096;
	if(fat_size % 4097 == 0){}
	else
	{
		num_fat_blk++;
	}
	if(super->amt_blk_FAT == num_fat_blk && 
	   super->root_dir_index == ++super->amt_blk_FAT &&
	   super->data_blk_index == ++super->root_dir_index &&
	   super->amt_blk_data == super->amt_blk - super->data_blk_index)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}


int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	assert(block_disk_open(diskname) == -1);
	super = (SuperBlock*)malloc(sizeof(SuperBlock));
	assert(super);
	fat = (uint16_t*)malloc(super->amt_blk_FAT*sizeof(SuperBlock));
	assert(fat);
	root_dir = (RootDirectory*)malloc(sizeof(RootDirectory));
	assert(root_dir);

	assert(block_read(0,super) == -1);
	int i = 0;
	while(SIGNATURE[i] != '\0')
	{
		char current_ch = super->sig[i];
		if(current_ch == SIGNATURE[i])
		{
			continue;
		}
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
		assert(block_read(index,fat+((BLOCK_SIZE/2)*i)) != -1);
		i++;
		index++;
	}
	assert(block_read(super->root_dir_index,root_dir) != -1);
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	assert(block_write(0,super) != -1);
	if(block_disk_count() == -1) return -1;
	int i = 0;
	int index = 1;
	while(i <= super->amt_blk_FAT-1)
	{
		assert(block_write(index,fat+((BLOCK_SIZE/2)*i)) != -1);
		i++;
		index++;
	}
	assert(block_write(super->root_dir_index,root_dir) != -1);
	free(super);
	if(super)
		return -1;
	free(fat);
	if(fat)
		return -1;
	free(root_dir);
	if(root_dir)
		return -1;
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
	enum ratio r;
	r = FAT;
	int fat_free = get_ratio(r);
	r = ROOT;
	int root_free = get_ratio(r);

	// printing info
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", super->amt_blk);
	printf("fat_blk_count=%d\n", super->amt_blk_FAT);
	printf("rdir_blk=%d\n", super->root_dir_index);
	printf("data_blk=%d\n", super->data_blk_index);
	printf("data_blk_count=%d\n", super->amt_blk_data);
	printf("fat_free_ratio=%d/%d\n", fat_free, super->amt_blk_data);
	printf("rdir_free_ratio=%d/%d\n", root_free, NUM_ROOT_ENTRIES);

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
	if(strlen(filename) > FILENAME_LEN)
	{
		perror("filename too long\n");
		return -1;
	}
	for(int i = 0; i < 
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

