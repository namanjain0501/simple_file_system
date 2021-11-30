/*
	Naman Jain
	18CS10034
*/

#ifndef SFS_H
#define SFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "disk.h"

// align max_name_len + 16 to 4096
#define MAX_NAME_LEN 112

typedef struct super_block {
	uint32_t magic_number;	// File system magic number
	uint32_t blocks;	// Number of blocks in file system (except super block)

	uint32_t inode_blocks;	// Number of blocks reserved for inodes == 10% of Blocks
	uint32_t inodes;	// Number of inodes in file system == length of inode bit map
	uint32_t inode_bitmap_block_idx;  // Block Number of the first inode bit map block
	uint32_t inode_block_idx;	// Block Number of the first inode block

	uint32_t data_block_bitmap_idx;	// Block number of the first data bitmap block
	uint32_t data_block_idx;	// Block number of the first data block
	uint32_t data_blocks;  // Number of blocks reserved as data blocks
} super_block;


typedef struct inode {
	uint32_t valid; // 0 if invalid
	uint32_t size; // logical size of the file
	uint32_t direct[5]; // direct data block pointer
	uint32_t indirect; // indirect pointer
} inode;

typedef struct dir_info{
	uint32_t valid; //(unset when a file or directory is deleted and can be replace by some other entry later
	uint32_t type; // file(0)/directory(1)
	char name[MAX_NAME_LEN]; // name of file or directory
	uint32_t length; // length of the name
	uint32_t inumber; //inumber of the file or directory
} dir_info;

int format(disk *diskptr);

int mount(disk *diskptr);

int create_file();

int remove_file(int inumber);

int stat(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int fit_to_size(int inumber, int size);

int read_file(char *filepath, char *data, int length, int offset);
int write_file(char *filepath, char *data, int length, int offset);
int create_dir(char *dirpath);
int remove_dir(char *dirpath);


#endif