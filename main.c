#include "disk.h"
#include "sfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>



#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define RESET "\x1B[0m"


int check_disk_stat(disk *diskptr, int disk_size, int reads, int writes){
	printf("Testing disk stats..\n");
	printf("Size %d bytes\nBlocks: %d\nReads: %d\nWrites: %d\n\n", diskptr->size, diskptr->blocks, diskptr->reads, diskptr->writes);

	if(diskptr->size == disk_size){
		printf(GRN "size Match, " RESET);
	} else { printf(RED "size Mismatch, " RESET); }

	if(diskptr->blocks == ((disk_size - 24)/(4096)) ){
		printf(GRN "blocks Match, " RESET);
	} else { printf(RED "blocks Mismatch, " RESET); }

	if(diskptr->reads == reads){
		printf(GRN "reads Match, " RESET);
	} else { printf(RED "reads Mismatch, " RESET); }

	if(diskptr->writes == writes){
		printf(GRN "writes Match \n" RESET);
	} else { printf(RED "writes Mismatch \n" RESET); }

	return 0;
}


int disk_creation_test(int disk_size){
	printf("\n=========================================================\n");
	printf("Disk test with size = %d  ---------\n", disk_size);
	printf("=========================================================\n");

	printf("Creating disk of size %d .... \n", disk_size);
	// disk mydisk;
	// disk *diskptr = &mydisk;
	// int ret;
	// ret = create_disk(diskptr, disk_size);

	int ret;
	disk *diskptr = create_disk(disk_size);

	if(ret == -1){
		printf(RED "Error\n" RESET);
	}
	else
	{
		printf(GRN "Success\n" RESET);
	}
	

	check_disk_stat(diskptr, disk_size, 0, 0);

	printf("Freeing disk...\n");
	ret = free_disk(diskptr);
	if(!ret){
		printf(GRN "Success\n" RESET);
	}
	else
	{
		printf(RED "Error\n" RESET);
	}

}


// ========================================================================


int read_write_test(int blocks){
	char *block_content_buffer = (char *) malloc((long)(4*1024));
	char *read_buffer = (char *) malloc((long)(4*1024));
	memset(block_content_buffer, 'x', 4*1024);


	printf("\n=========================================================\n");
	printf("Block read write test with %d blocks ---------\n", blocks);
	printf("=========================================================\n");

	int disk_size = (blocks * 4096) + 24;
	printf("Creating disk of size %d .... \n", disk_size);

	// disk mydisk;
	// disk *diskptr = &mydisk;
	// int ret;
	// ret = create_disk(diskptr, disk_size);

	int ret;
	disk *diskptr = create_disk(disk_size);

	if(ret == -1){
		printf(RED "Error\n" RESET);
	}
	else
	{
		printf(GRN "Success\n" RESET);
	}

	check_disk_stat(diskptr, disk_size, 0,0);

	// Write one block test
	printf("One block write.. \n");
	write_block(diskptr, 1, block_content_buffer);

	check_disk_stat(diskptr, disk_size, 0,1);

	read_block(diskptr, 1, read_buffer);

	ret = memcmp(read_buffer, block_content_buffer, 4096);
	if(!ret){
		printf(GRN "Block Matched\n" RESET);
	} else { printf(RED "Block Mismatch\n" RESET); }
	check_disk_stat(diskptr, disk_size, 1,1);
	

	// write all blocks
	for (int iii = 0; iii < blocks; iii++)
	{
			write_block(diskptr, iii, block_content_buffer);
			read_block(diskptr, iii, read_buffer);

			ret = memcmp(read_buffer, block_content_buffer, 4096);
			if(!ret){
				printf(GRN "Block Matched, " RESET);
			} else { printf(RED "Block Mismatch " RESET); }
	}
	printf("\n");
	check_disk_stat(diskptr, disk_size, blocks+1, blocks+1);
	
	ret = free_disk(diskptr);
}

int sfs_test(){
	printf("\n=========================================================\n");
	printf("Test SFS \n");
	printf("=========================================================\n");

	// disk mydisk;
	// disk *diskptr = &mydisk;
	// int ret;
	// ret = create_disk(diskptr, (50 * 4096) + 24);

	int ret;
	disk *diskptr = create_disk((2048 * 4096) + 24);

	// mount
	printf("Formatting..\n");
	ret = format(diskptr);
	if(!ret) {printf(GRN "Format Success\n" RESET);}
	else {printf(RED "Format Error\n" RESET);}
	
	printf("Mounting..\n");
	ret = mount(diskptr);
	if(!ret){printf(GRN "Mount Success\n" RESET);}
	else{printf(RED "Mount Error\n" RESET);}

	// File read write test
	int file_inode =  create_file();
	if(!ret) {printf(GRN "File Creation Success\n" RESET);}
	else {printf(RED "File Creation Error\n" RESET);}

	printf("\n-------------------\n");
	stat(file_inode);
	printf("\n-------------------\n");

	printf("\n-------------------------\n");
	printf("Writing..4096 byte file..\n");
	printf("-------------------------\n");

	// Test with file size 4096*5
	char *databuff = (char *) malloc((long)(4096));
	memset(databuff, 'x', 4096);

	int wb = write_i(file_inode, databuff, 4096, 0);
	if(wb != 4096){
		printf(RED "ERROR could not write to file 4096 bytes of data | Return value: %d\n" RESET, wb);
		printf("Return value: %d\n", wb);
	} else {printf(GRN "Write Success\n" RESET);}

	// Read
	char *databuff_r = (char *) malloc((long)(4096));
	memset(databuff_r, 0, 4096);	
	
	int rb = read_i(file_inode, databuff_r, 4096, 0);
	if(rb != 4096){
		printf(RED "ERROR could not read from file 4096 bytes of data\n" RESET);
		printf("Return value: %d\n", rb);
	} else {printf(GRN "Read Success\n" RESET);}

	if(memcmp(databuff, databuff_r, 4096)){
		printf(RED "ERROR read mismatch for  4096 bytes of data\n");
		printf(RESET);
		for (int jjj = 0; jjj < rb; jjj++)
		{
			if(databuff[jjj]!=databuff_r[jjj]){
				printf("Mismatch %c,%c, pos:%d |", databuff[jjj], databuff_r[jjj], jjj);
			}
		}
	} else {printf(GRN "Content matched\n" RESET);}
	printf("\n-------------------\n");
	stat(file_inode);
	printf("\n-------------------\n");



	printf("\n-------------------------\n");
	printf("Writing..4096+100 byte file..\n");
	printf("-------------------------\n");
	file_inode =  create_file();
	// Test with file size 4096+100
	char *databuff2 = (char *) malloc((long)(4196));
	memset(databuff2, 'x', 4196);

	wb = write_i(file_inode, databuff2, 4196, 0);
	if(wb != 4196){
		printf(RED "ERROR could not write to file 4196 bytes of data | Return value: %d\n" RESET, wb);
		printf("Return value: %d\n", wb);
	} else {printf(GRN "Write Success\n" RESET);}

	// Read
	char *databuff_r2 = (char *) malloc((long)(4196));
	memset(databuff_r2, 0, 4196);	
	rb = read_i(file_inode, databuff_r2, 4196, 0);
	if(rb != 4196){
		printf(RED "ERROR could not read from file 4196 bytes of data | Return value: %d\n" RESET, rb);
	} else {printf(GRN "Read Success\n" RESET);}

	if(memcmp(databuff2, databuff_r2, rb)){
		printf(RED "ERROR read mismatch for  4196 bytes of data\n");
		printf(RESET);
		for (int jjj = 0; jjj < rb; jjj++)
		{
			if(databuff2[jjj]!=databuff_r2[jjj]){
				printf("Mismatch %c,%c, pos:%d", databuff2[jjj], databuff_r2[jjj], jjj);
			}
		}
	} else {printf(GRN "Content matched\n" RESET);}
	printf("\n-------------------\n");
	stat(file_inode);
	printf("\n-------------------\n");




	printf("\n-------------------------\n");
	printf("Writing..4096 * (5 + 1024) = 4214784 byte file.. -- INDIRECT POINTERS\n");
	printf("-------------------------\n");
	int bigdatasize = 4096 * (5 + 1024);
	char *bigdatabuff = (char *) malloc((long)(bigdatasize+100));
	char *bigdatabuff_r = (char *) malloc((long)(bigdatasize+100));
	memset(bigdatabuff, 'x', bigdatasize+100);
	memset(bigdatabuff_r, 0, bigdatasize+100);


	file_inode =  create_file();


	wb = write_i(file_inode, bigdatabuff, bigdatasize, 0);
	if(wb != bigdatasize){
		printf(RED "ERROR could not write to file %d bytes of data | Return value: %d\n" RESET,bigdatasize, wb);
		printf("Return value: %d\n", wb);
	} else {printf(GRN "Write Success\n" RESET);}

	// Read
	rb = read_i(file_inode, bigdatabuff_r, bigdatasize, 0);
	if(rb != bigdatasize){
		printf(RED "ERROR could not read from file %d bytes of data | Return value: %d\n" RESET, bigdatasize, rb);
	} else {printf(GRN "Read Success\n" RESET);}

	if(memcmp(bigdatabuff, bigdatabuff_r, rb)){
		printf(RED "ERROR read mismatch for  bigdatasize bytes of data\n");
		printf(RESET);
		for (int jjj = 0; jjj < rb; jjj++)
		{
			if(bigdatabuff[jjj]!=bigdatabuff_r[jjj]){
				printf("Mismatch %c,%c, pos:%d", bigdatabuff[jjj], bigdatabuff_r[jjj], jjj);
			}
		}
	} else {printf(GRN "Content matched\n" RESET);}
	printf("\n-------------------\n");
	stat(file_inode);
	printf("\n-------------------\n");

	printf("\n-------------------------\n");
	printf("Fit to Size\n");
	printf("-------------------------\n");
	int changed_data_size = 2096 * (5 + 1024);
	int state = fit_to_size(file_inode,changed_data_size);
	stat(file_inode);

	ret = free_disk(diskptr);
	return 0;
}



int file_dir_test(){
	printf("\n===========================================================================\n");
	printf("Test File and Directory Structure \n");
	printf("=============================================================================\n");

	// disk mydisk;
	// disk *diskptr = &mydisk;
	// int ret;
	// ret = create_disk(diskptr, (50 * 4096) + 24);

	int ret;
	disk *diskptr = create_disk((2048 * 4096) + 24);

	// mount
	printf("Formatting..\n");
	ret = format(diskptr);
	if(!ret) {printf(GRN "Format Success\n" RESET);}
	else {printf(RED "Format Error\n" RESET);}
	
	printf("Mounting..\n");
	ret = mount(diskptr);
	if(!ret){printf(GRN "Mount Success\n" RESET);}
	else{printf(RED "Mount Error\n" RESET);}

	// dummy file..
	int a = create_file();

	// File read write test
	int filedatasize = 4096 * 5;
	char *filedatabuff = (char *) malloc((long)(filedatasize+100));
	char *filedatabuff_r = (char *) malloc((long)(filedatasize+100));
	memset(filedatabuff, 'x', filedatasize+100);
	memset(filedatabuff_r, 0, filedatasize+100);

	char *dirpath = "dir1";
	char *filepath = "dir1/abc.txt";

	printf("Create directory...\n");
	create_dir(dirpath);
	// create_dir(filepath);


	printf("Writing file...\n");
	ret = write_file(filepath, filedatabuff, filedatasize, 0);
	if(ret != filedatasize){
		printf(RED "ERROR could not write to file %d bytes of data | Return value: %d\n" RESET,filedatasize, ret);
		printf("Return value: %d\n", ret);
	} else {printf(GRN "Write Success\n" RESET);}


	printf("Reading file...\n");
	// Read
	ret = read_file(filepath, filedatabuff_r, filedatasize, 0);
	if(ret != filedatasize){
		printf(RED "ERROR could not read from file %d bytes of data | Return value: %d\n" RESET, filedatasize, ret);
	} else {printf(GRN "Read Success\n" RESET);}

	if(memcmp(filedatabuff, filedatabuff_r, ret)){
		printf(RED "ERROR read mismatch for  bigdatasize bytes of data\n");
		printf(RESET);
		for (int jjj = 0; jjj < ret; jjj++)
		{
			if(filedatabuff[jjj]!=filedatabuff_r[jjj]){
				printf("Mismatch %c,%c, pos:%d", filedatabuff[jjj], filedatabuff_r[jjj], jjj);
			}
		}
	} else {printf(GRN "Content matched\n" RESET);}



ret = free_disk(diskptr);
return 0;
}


// ========================================================================

int main(){


	// Disk creation test

	disk_creation_test((4096*100)+24);
	disk_creation_test((4096*100));
	disk_creation_test((4096*999));


	// Read write test
	read_write_test(100);


	// SFS Tests
	sfs_test();


	// File, director test
	file_dir_test();

	return 0;
}
