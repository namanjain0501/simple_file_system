/*
	Naman Jain
	18CS10034
*/

#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

disk* create_disk(int nbytes)
{
	disk *diskptr;
	diskptr = (disk *)malloc(sizeof(disk));

	diskptr->size = nbytes;
	int num_blocks = (nbytes-24)/4096;
	diskptr->blocks = num_blocks;	
	diskptr->reads=0;
	diskptr->writes=0;
	diskptr->block_arr = (char **)malloc(num_blocks*sizeof(char *));

	if(diskptr->block_arr==NULL)
	{
		printf("Create Disk Failed: Memory Allocation Failed\n");
		return NULL;
	}

	for(int i=0;i<num_blocks;i++)
	{
		diskptr->block_arr[i] = (char *)malloc(4096);
		if(diskptr->block_arr[i]==NULL)
		{
			printf("Create Disk Failed: Memory Allocation Failed\n");
			return NULL;
		}
	}

	printf("Create Disk Success:  size %d bytes.\n", nbytes);
	return diskptr;
}

int read_block(disk *diskptr, int blocknr, void *block_data)
{
	if(blocknr<0 || blocknr>(diskptr->blocks-1))
	{
		printf("Read Failed: Invalid Block Access\n");
		return -1;
	}

	memcpy(block_data, diskptr->block_arr[blocknr], 4096);

	if(block_data==NULL)
	{
		printf("Read Failed: Copy to Buffer Failed\n");
		return -1;
	}
	diskptr->reads++;
	return 0;
}

int write_block(disk *diskptr, int blocknr, void *block_data)
{
	if(blocknr<0 || blocknr>(diskptr->blocks-1))
	{
		printf("Write Failed: Invalid Block Access\n");
		return -1;
	}

	memcpy(diskptr->block_arr[blocknr], (char *)block_data, 4096);

	if(diskptr->block_arr[blocknr]==NULL)
	{
		printf("Write Failed: Copy to Disk Failed\n");
		return -1;
	}
	diskptr->writes++;
	return 0;
}

int free_disk(disk *diskptr)
{
	for(int i=0;i<(diskptr->blocks);i++)
	{
		free(diskptr->block_arr[i]);
	}
	free(diskptr->block_arr);

	printf("Free Disk Success\n");
	return 0;
}
