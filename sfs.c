/*
	Naman Jain
	18CS10034
*/

#include "sfs.h"

int is_mounted = 0;
disk *gl_diskptr = NULL;

char *gl_inode_bitmap;
char *gl_data_bitmap;

void unset_bitmap(char *b, int in)
{
	b[in/8] &= ~(1<<(in&7));
}

void set_bitmap(char *b, int in)
{
	b[in/8] |= (1<<(in&7));
}

int get_bitmap(char *b, int in)
{
	return b[in/8] & (1<<(in&7)) ? 1 : 0;
}

int min(int a,int b)
{
	return (a<b) ? a : b;
}

int max(int a,int b)
{
	return (a<b) ? b : a;
}

inode *get_inode(super_block *sup, int in)
{
	inode *ind = (inode *)malloc(sizeof(inode));
	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, sup->inode_block_idx+in/128, buffer)<0)
	{
		free(buffer);
		free(ind);
		return NULL;
	}
	memcpy(ind, buffer+(in%128)*32, sizeof(inode));
	free(buffer);
	return ind;
}

int update_inode(super_block *sup, int in, inode *ind)
{
	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, sup->inode_block_idx+in/128, buffer)<0)
	{
		printf("Update Inode Failed: Read Inode Failed\n");
		free(buffer);
		return -1;
	}
	memcpy(buffer+(in%128)*32, ind, sizeof(inode));
	if(write_block(gl_diskptr, sup->inode_block_idx+in/128, buffer)<0)
	{
		printf("Update Inode Failed: Write Inode Failed\n");
		free(buffer);
		return -1;
	}
	return 0;
}

int update_data_bitmap(super_block *sup, int in, int val)
{
	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, sup->data_block_bitmap_idx + in/(8*4096), buffer)<0)
	{
		free(buffer);
		return -1;
	}
	if(val==1)
		set_bitmap(buffer, in%(8*4096));
	else
		unset_bitmap(buffer, in%(8*4096));
	if(write_block(gl_diskptr, sup->data_block_bitmap_idx + in/(8*4096), buffer)<0)
	{
		free(buffer);
		return -1;
	}
	free(buffer);
	return 0;
}

int update_inode_bitmap(super_block *sup, int in, int val)
{
	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, sup->inode_bitmap_block_idx + in/(8*4096), buffer)<0)
	{
		free(buffer);
		return -1;
	}
	if(val==1)
		set_bitmap(buffer, in%(8*4096));
	else
		unset_bitmap(buffer, in%(8*4096));
	if(write_block(gl_diskptr, sup->inode_bitmap_block_idx + in/(8*4096), buffer)<0)
	{
		free(buffer);
		return -1;
	}
	free(buffer);
	return 0;
}

int format(disk *diskptr)
{
	// inode bitmap -> data bitmap -> inodes -> data blocks
	int N = diskptr->blocks;
	int M = N-1;
	int I = M/10;
	int IB = (I*128+8*4096-1)/(8*4096);
	int R = M-I-IB;
	int DBB = (R+8*4096-1)/(8*4096);
	int DB = R-DBB;

	super_block sup;
	sup.magic_number=12345;
	sup.blocks = M;
	sup.inode_blocks=I;
	sup.inodes=I*128;
	sup.inode_bitmap_block_idx=1;
	sup.inode_block_idx=1+IB+DBB;
	sup.data_block_bitmap_idx=1+IB;
	sup.data_block_idx=1+IB+DBB+I;
	sup.data_blocks=DB;

	char *buffer = (char *)malloc(4096);
	memcpy(buffer, &sup, sizeof(super_block));
	if(write_block(diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Format Error: Super Block write to disk failed\n");
		return -1;
	}

	// Inode Bitmap
	char *inode_bitmap = (char *)malloc((sup.inodes+7)/8);
	for(int i=0;i<sup.inodes;i++)
		unset_bitmap(inode_bitmap, i);

	int bytes_to_write = (sup.inodes+7)/8;
	int cur_blocknr = sup.inode_bitmap_block_idx;

	for(int i=0;i<IB;i++)
	{
		if(i==(IB-1))
		{
			memcpy(buffer, inode_bitmap+i*4096, bytes_to_write);
			if(write_block(diskptr, cur_blocknr, buffer)<0)
			{
				free(buffer);
				printf("Format Error: Inode bitmap copy to disk failed\n");
				return -1;
			}
			cur_blocknr++;
			bytes_to_write=0;
			break;
		}
		memcpy(buffer, inode_bitmap+i*4096, 4096);
		if(write_block(diskptr, cur_blocknr, buffer)<0)
		{
			free(buffer);
			printf("Format Error: Inode bitmap copy to disk failed\n");
			return -1;
		}
		cur_blocknr++;
		bytes_to_write-=4096;
	}
	// update inode bitmap in memory
	if(is_mounted)
	{
		free(gl_inode_bitmap);
		gl_inode_bitmap = (char *)malloc((sup.inodes+7)/8);
		memcpy(gl_inode_bitmap, inode_bitmap, (sup.inodes+7)/8);
	}
	free(inode_bitmap);

	//Data Bitmap
	char *data_bitmap = (char *)malloc((sup.data_blocks+7)/8);
	for(int i=0;i<sup.data_blocks;i++)
		unset_bitmap(data_bitmap, i);

	bytes_to_write = (sup.data_blocks+7)/8;
	cur_blocknr = sup.data_block_bitmap_idx;

	for(int i=0;i<DBB;i++)
	{
		if(bytes_to_write<4096)
		{
			memcpy(buffer, data_bitmap+i*4096, bytes_to_write);
			if(write_block(diskptr, cur_blocknr, buffer)<0)
			{
				free(buffer);
				printf("Format Error: Data bitmap copy to disk failed\n");
				return -1;
			}
			cur_blocknr++;
			bytes_to_write=0;
			break;
		}
		memcpy(buffer, data_bitmap+i*4096, 4096);
		if(write_block(diskptr, cur_blocknr, buffer)<0)
		{
			free(buffer);
			printf("Format Error: Data bitmap copy to disk failed\n");
			return -1;
		}
		cur_blocknr++;
		bytes_to_write-=4096;
	}
	// update data bitmap in memory
	if(is_mounted)
	{
		free(gl_data_bitmap);
		gl_data_bitmap = (char *)malloc((sup.data_blocks+7)/8);
		memcpy(gl_data_bitmap, data_bitmap, (sup.data_blocks+7)/8);
	}
	free(data_bitmap);

	//Inodes
	for(int i=0;i<I;i++)
	{
		inode *block_inodes = (inode *)malloc(128*sizeof(inode));
		for(int j=0;j<128;j++)
		{
			block_inodes[j].valid = 0;
		}
		if(write_block(diskptr, sup.inode_block_idx+i, block_inodes)<0)
		{
			free(block_inodes);
			free(buffer);
			printf("Format Error: Inode copy to disk failed\n");
			return -1;
		}
		free(block_inodes);
	}	

	free(buffer);

	return 0;
}

int mount(disk *diskptr)
{
	char *buffer = (char *)malloc(4096);
	if(read_block(diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Mount Error: Super block read failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(sup));

	if(sup.magic_number != 12345)
	{
		free(buffer);
		printf("Mount Error: Magic Number not matching\n");
		return -1;
	}

	// bring bitmaps in memory

	int cur_blocknr = 0;
	gl_inode_bitmap = (char *)malloc((sup.inodes+7)/8);
	int bytes_to_read = (sup.inodes+7)/8;

	for(int i=sup.inode_bitmap_block_idx;i<sup.data_block_bitmap_idx;i++)
	{
		if(read_block(diskptr, i, buffer)<0)
		{
			free(buffer);
			printf("Mount Error: Inode Bitmap Read Failed\n");
			return -1;
		}
		if(bytes_to_read==0)
			break;
		else if(bytes_to_read<4096)
		{
			memcpy(gl_inode_bitmap+cur_blocknr*4096, buffer, bytes_to_read);
			break;
		}
		memcpy(gl_inode_bitmap+cur_blocknr*4096, buffer, 4096);
		bytes_to_read-=4096;
		cur_blocknr++;
	}

	cur_blocknr=0;
	gl_data_bitmap = (char *)malloc((sup.data_blocks+7)/8);
	bytes_to_read = (sup.data_blocks+7)/8;

	for(int i=sup.data_block_bitmap_idx;i<sup.inode_block_idx;i++)
	{
		if(read_block(diskptr, i, buffer)<0)
		{
			free(buffer);
			printf("Mount Error: Data Bitmap Read Failed\n");
			return -1;
		}
		if(bytes_to_read==0)
			break;
		else if(bytes_to_read<4096)
		{
			memcpy(gl_data_bitmap+cur_blocknr*4096, buffer, bytes_to_read);
			break;
		}
		memcpy(gl_data_bitmap+cur_blocknr*4096, buffer, 4096);
		bytes_to_read-=4096;
		cur_blocknr++;
	}

	free(buffer);
	gl_diskptr=diskptr;
	is_mounted=1;
	create_dir("/");
	return 0;
}

int create_file()
{
	if(!is_mounted)
	{
		printf("Create File Error: Not Mounted\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Create File Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	//search for free inode
	int in=-1;
	for(int i=1;i<sup.inodes;i++)
	{
		if(!get_bitmap(gl_inode_bitmap, i))
		{
			in=i;
			break;
		}
	}

	if(in==-1)
	{
		free(buffer);
		printf("Create File Error: No Inode Empty\n");
		return -1;
	}

	inode *ind = get_inode(&sup, in);
	if(ind==NULL)
	{
		free(buffer);
		printf("Create File Error: Inode cannot be found\n");
		return -1;
	}

	ind->valid=1;
	ind->size=0;
	for(int i=0;i<5;i++)
		ind->direct[i]=-1;
	ind->indirect=-1;
	set_bitmap(gl_inode_bitmap, in);

	//update inode bitmap
	read_block(gl_diskptr, sup.inode_bitmap_block_idx + in/(8*4096), buffer);
	memcpy(buffer+(in%(8*4096))/8, gl_inode_bitmap+in/8, 1);
	write_block(gl_diskptr, sup.inode_bitmap_block_idx+in/(8*4096), buffer);

	//update inode
	if(update_inode(&sup, in, ind)<0)
	{
		free(buffer);
		printf("Create File Error: Update Inode Failed\n");
		return -1;
	}

	printf("Create File Success\n");
	free(buffer);
	return in;
}

int remove_file(int inumber)
{
	if(!is_mounted)
	{
		printf("Remove File Error: Not Mounted\n");
		return -1;
	}

	if(!get_bitmap(gl_inode_bitmap, inumber))
	{
		printf("Remove File Error: Inode Not Present\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Remove File Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	inode *ind = get_inode(&sup, inumber);
	if(ind==NULL)
	{
		free(buffer);
		printf("Remove File Error: Get Inode Failed\n");
		return -1;
	}

	ind->valid=0;
	//update memory inode bitmap
	unset_bitmap(gl_inode_bitmap, inumber);

	//update inode
	if(update_inode(&sup, inumber, ind)<0)
	{
		free(buffer);
		printf("Remove File Error: Inode Update Failed\n");
		return -1;
	}

	//update memory and disk data bitmap
	for(int i=0;i<5;i++)
	{
		if(ind->direct[i]!=-1)
		{
			unset_bitmap(gl_data_bitmap, ind->direct[i]);
			update_data_bitmap(&sup, ind->direct[i], 0);
		}
	}
	if(ind->indirect!=-1)
	{
		if(read_block(gl_diskptr, ind->indirect, buffer)<0)
		{
			free(buffer);
			printf("Remove File Error: Indirect Pointer Read Failed\n");
			return -1;
		}
		int *data_ptr = buffer;
		for(int i=0;i<((ind->size)/4096 - 5);i++)
		{
			unset_bitmap(gl_data_bitmap, *data_ptr);
			update_data_bitmap(&sup, *data_ptr, 0);
			data_ptr++;
		}
	}

	update_inode_bitmap(&sup, inumber, 0);
	free(buffer);
	printf("Remove File Success\n");
	return 0;
}

int stat(int inumber)
{
	if(!is_mounted)
	{
		printf("Stat Error: Not Mounted\n");
		return -1;
	}

	if(!get_bitmap(gl_inode_bitmap, inumber))
	{
		printf("Stat Error: Inode Not Present\n");
		return -1;
	}	

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Stat Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	inode *ind = get_inode(&sup, inumber);
	if(ind==NULL)
	{
		free(buffer);
		printf("Stat Error: Get Inode Failed\n");
		return -1;
	}

	printf("Logical Size: %d\n", ind->size);
	printf("Number of Data Blocks in use: %d\n", (ind->indirect == -1) ? (ind->size+4095)/4096 : (ind->size+4095)/4096 + 1);
	printf("Number of Direct Pointers in use: %d\n", ((ind->size+4095)/4096 > 5) ? 5 : (ind->size+4095)/4096);
	printf("Number of Indirect Pointers in use: %d\n", (ind->size<=5*4096) ? 0 : ((ind->size+4095)/4096 - 5));

	free(buffer);
	return 0;
}

int read_i(int inumber, char *data, int length, int offset)
{
	if(!is_mounted)
	{
		printf("Readi Error: Not Mounted\n");
		return -1;
	}

	if(!get_bitmap(gl_inode_bitmap, inumber))
	{
		printf("Readi Error: Inode Not Present\n");
		return -1;
	}	

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Readi Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	if(inumber<0 || inumber>=sup.inodes)
	{
		printf("Readi Error: Invalid Request\n");
		free(buffer);
		return -1;
	}

	inode *ind = get_inode(&sup, inumber);
	if(ind==NULL)
	{
		free(buffer);
		printf("Readi Error: Get Inode Failed\n");
		return -1;
	}

	if(offset<0 || offset>=ind->size || length<0 || !ind->valid)
	{
		printf("Readi Error: Invalid Request\n");
		free(buffer);
		return -1;
	}

	if(length > (ind->size-offset))
		length=ind->size-offset;

	int bytes_read=0;

	if(offset<5*4096)
	{
		int first_offset = offset%4096;
		for(int i=offset/4096;i<5;i++)
		{
			if(length==0)
				break;
			if(read_block(gl_diskptr,sup.data_block_idx + ind->direct[i], buffer)<0)
			{
				printf("Readi Error: Read Block Failed\n");
				free(buffer);
				return -1;
			}
			memcpy(data, buffer+first_offset, min(length, 4096-first_offset));
			data+=min(length, 4096-first_offset);
			bytes_read+=min(length, 4096-first_offset);
			offset+=min(length, 4096-first_offset);
			length-=min(length, 4096-first_offset);
			first_offset=0;
		}
	}
	if(length>0)
	{
		char *indirect_block = (char *)malloc(4096);
		if(read_block(gl_diskptr, ind->indirect, indirect_block)<0)
		{
			printf("Readi Error: Read Block Failed\n");
			free(buffer);
			free(indirect_block);
			return -1;
		} 
		int *block_ptr = indirect_block;
		block_ptr+=(offset/4096 - 5);
		int first_offset=offset%4096;
		while(length>0)
		{
			if(read_block(gl_diskptr, sup.data_block_idx + *block_ptr, buffer)<0)
			{
				printf("Readi Error: Read Block Failed\n");
				free(buffer);
				free(indirect_block);
				return -1;
			}
			memcpy(data, buffer+first_offset, min(length, 4096-first_offset));
			bytes_read+=min(length, 4096-first_offset);
			data+=min(length, 4096-first_offset);
			offset+=min(length, 4096-first_offset);
			length-=min(length, 4096-first_offset);
			block_ptr++;
			first_offset=0;
		} 
		free(indirect_block);
	}

	free(buffer);
	return bytes_read;
}

int write_i(int inumber, char *data, int length, int offset)
{
	if(!is_mounted)
	{
		printf("Writei Error: Not Mounted\n");
		return -1;
	}

	if(!get_bitmap(gl_inode_bitmap, inumber))
	{
		printf("Writei Error: Inode Not Present\n");
		return -1;
	}	

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Writei Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));


	if(inumber<0 || inumber>=sup.inodes)
	{
		printf("Writei Error: Invalid Request\n");
		free(buffer);
		return -1;
	}

	inode *ind = get_inode(&sup, inumber);
	if(ind==NULL)
	{
		free(buffer);
		printf("Writei Error: Get Inode Failed\n");
		return -1;
	}

	// 1024+5
	if(offset<0 || offset>=1029*4096 || length<0 || !ind->valid)
	{
		printf("Writei Error: Invalid Request\n");
		free(buffer);
		return -1;
	}

	length = min(length, 1029*4096-offset);

	//update data bitmap and inode
	if((length+offset)>ind->size)
	{
		//to allocate
		int st_idx = (ind->size+4095)/4096;
		int end_idx = (length+offset+4095)/4096 - 1;

		if(end_idx>=st_idx)
		{
			int req_blocks = end_idx-st_idx+1;
			if(end_idx>=5 && st_idx<=5)
			{
				req_blocks++;
			}
			int *block_ids = (int *)malloc(req_blocks*sizeof(int));
			int in=0;
			for(int i=0;i<sup.data_blocks;i++)
			{
				if(!get_bitmap(gl_data_bitmap, i))
				{
					block_ids[in++]=i;
					if(in==req_blocks)
						break;
				}
			}
			if(in!=req_blocks)
			{
				free(buffer);
				printf("Writei Error: Disk Full\n");
				return -1;
			}
			in=0;
			for(int i=st_idx;i<5 && i<=end_idx;i++)
			{
				set_bitmap(gl_data_bitmap, block_ids[in]);
				update_data_bitmap(&sup, block_ids[in], 1);
				ind->direct[i]=block_ids[in++];
			}
			if(end_idx>=5)
			{
				char *indirect_block = (char *)malloc(4096);
				int ind_blk_idx;
				if(end_idx>=5 && st_idx<=5)
				{
					ind_blk_idx=block_ids[in++];
					set_bitmap(gl_data_bitmap, ind_blk_idx);
					update_data_bitmap(&sup, ind_blk_idx, 1);
					ind->indirect=sup.data_block_idx+ind_blk_idx;
				}
				if(read_block(gl_diskptr, ind_blk_idx, indirect_block)<0)
				{
					free(buffer);
					free(indirect_block);
					printf("Writei Error: Indirect Block Read Failed\n");
					return -1;
				}
				int *ptr = indirect_block;
				for(int i=max(st_idx-5, 0);i<=end_idx-5;i++)
				{
					set_bitmap(gl_data_bitmap, block_ids[in]);
					update_data_bitmap(&sup, block_ids[in], 1);
					*(ptr+i)=block_ids[in++];
				}
				if(write_block(gl_diskptr, ind_blk_idx, indirect_block)<0)
				{
					free(buffer);
					free(indirect_block);
					printf("Writei Error: Indirect Block Update Failed\n");
					return -1;
				}
				free(indirect_block);
			}
		}
		ind->size = offset+length;
	}




	int bytes_written=0;

	if(offset<5*4096)
	{
		int first_offset = offset%4096;
		for(int i=offset/4096;i<5;i++)
		{
			if(length==0)
				break;
			if(read_block(gl_diskptr,sup.data_block_idx + ind->direct[i], buffer)<0)
			{
				printf("Writei Error: Read Block Failed\n");
				free(buffer);
				return -1;
			}
			memcpy(buffer+first_offset, data, min(length, 4096-first_offset));
			data+=min(length, 4096-first_offset);
			bytes_written+=min(length, 4096-first_offset);
			offset+=min(length, 4096-first_offset);
			length-=min(length, 4096-first_offset);
			first_offset=0;
			if(write_block(gl_diskptr, sup.data_block_idx+ind->direct[i], buffer)<0)
			{
				printf("Writei Error: Write Block Failed\n");
				free(buffer);
				return -1;
			}
		}
	}

	if(update_inode(&sup, inumber, ind)<0)
	{
		free(buffer);
		printf("Writei Error: Inode update Failed\n");
		return -1;
	}

	if(length>0)
	{
		char *indirect_block = (char *)malloc(4096);
		if(read_block(gl_diskptr, ind->indirect, indirect_block)<0)
		{
			printf("Writei Error: Indirect Block Read Failed\n");
			free(buffer);
			return -1;
		} 
		int *block_ptr = indirect_block;
		block_ptr+=(offset/4096 - 5);
		int first_offset=offset%4096;
		while(length>0)
		{
			if(read_block(gl_diskptr, sup.data_block_idx + *block_ptr, buffer)<0)
			{
				printf("Writei Error: Read Block Failed\n");
				free(buffer);
				free(indirect_block);
				return -1;
			}
			memcpy(buffer+first_offset, data, min(length, 4096-first_offset));
			if(write_block(gl_diskptr, sup.data_block_idx + *block_ptr, buffer)<0)
			{
				printf("Writei Error: Write Block Failed\n");
				free(buffer);
				free(indirect_block);
				return -1;
			}
			bytes_written+=min(length, 4096-first_offset);
			data+=min(length, 4096-first_offset);
			offset+=min(length, 4096-first_offset);
			length-=min(length, 4096-first_offset);
			block_ptr++;
			first_offset=0;
		}
		free(indirect_block); 
	}

	free(ind);
	return bytes_written;
}

int fit_to_size(int inumber, int size)
{
	if(!is_mounted)
	{
		printf("Fit to Size Error: Not Mounted\n");
		return -1;
	}

	if(!get_bitmap(gl_inode_bitmap, inumber))
	{
		printf("Fit to Size Error: Inode Not Present\n");
		return -1;
	}	

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Fit to Size Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));


	if(inumber<0 || inumber>=sup.inodes)
	{
		printf("Fit to Size Error: Invalid Request\n");
		free(buffer);
		return -1;
	}

	inode *ind = get_inode(&sup, inumber);
	if(ind==NULL)
	{
		free(buffer);
		printf("Fit to Size Error: Get Inode Failed\n");
		return -1;
	}

	int block_already = (ind->size+4095)/4096;
	int block_after = (size+4095)/4096;
	if(block_after<block_already)
	{
		//update data bitmap and inode
		//to remove
		int st_idx = block_after;
		int end_idx = block_already;

		for(int i=st_idx;i<5 && i<=end_idx;i++)
		{
			unset_bitmap(gl_data_bitmap, ind->direct[i]);
			update_data_bitmap(&sup, ind->direct[i], 0);
			ind->direct[i]=-1;
		}
		if(end_idx>=5)
		{
			char *indirect_block = (char *)malloc(4096);
			if(read_block(gl_diskptr, ind->indirect, indirect_block)<0)
			{
				free(buffer);
				free(indirect_block);
				printf("Fit to Size Error: Indirect Block Read Failed\n");
				return -1;
			}
			int *ptr = indirect_block;
			for(int i=max(st_idx-5, 0);i<=end_idx-5;i++)
			{
				unset_bitmap(gl_data_bitmap, *(ptr+i));
				update_data_bitmap(&sup, *(ptr+i), 0);
				*(ptr+i)=-1;
			}
			if(st_idx<=5)
			{
				unset_bitmap(gl_data_bitmap, ind->indirect-sup.data_block_idx);
				update_data_bitmap(&sup, ind->indirect-sup.data_block_idx, 0);
				ind->indirect=-1;
			}
			else
			{
				if(write_block(gl_diskptr, ind->indirect, indirect_block)<0)
				{
					free(buffer);
					free(indirect_block);
					printf("Fit to Size Error: Indirect Block Update Failed\n");
					return -1;
				}
			}
			free(indirect_block);
		}
	}
	ind->size = min(ind->size, size);
	if(update_inode(&sup, inumber, ind)<0)
	{
		free(buffer);
		printf("Fit to Size Error: Inode update Failed\n");
		return -1;
	}
	printf("Fit to Size in %d bytes Successful\n", size);
	return ind->size;
}

int get_files_from_inode(inode *ind, dir_info *dir_files, super_block *sup)
{
	int num_files = ind->size/sizeof(dir_info);
	int entries_in_one_block = 4096/sizeof(dir_info);
	char *buffer = (char *)malloc(4096);
	int cur_in=0;
	for(int i=0;i<5;i++)
	{
		if(cur_in >= num_files)
			break;
		if(read_block(gl_diskptr,sup->data_block_idx + ind->direct[i], buffer)<0)
		{
			free(buffer);
			return -1;
		}
		for(int j=0;j<entries_in_one_block;j++)
		{
			if(cur_in >= num_files)
				break;
			memcpy(&dir_files[cur_in], buffer+j*sizeof(dir_info), sizeof(dir_info));
			cur_in++;
		}
	}
	free(buffer);
	return 0;
}

inode *get_parent_inode(inode *root_dir_inode, char dirpath[], super_block *sup, char **name, int *parent_inumber)
{
	*parent_inumber=0;
	char rest[1000];
	strcpy(rest,dirpath);
	char *token = strtok(rest, "/");
	if(token==NULL)
	{
		return NULL;
	}

	inode *cur_ind = root_dir_inode;
	while(token!=NULL)
	{
		int num_files = cur_ind->size / sizeof(dir_info);
		dir_info *dir_files = (dir_info *)malloc(num_files * sizeof(dir_info));

		if(get_files_from_inode(cur_ind, dir_files, sup)<0)
		{
			return NULL;
		}

		int found=0;

		//search for file
		for(int i=0;i<num_files;i++)
		{
			if(dir_files[i].valid && dir_files[i].type==1 && strcmp(dir_files[i].name, token)==0)
			{
				*parent_inumber=dir_files[i].inumber;
				cur_ind=get_inode(sup, dir_files[i].inumber);
				if(cur_ind<0)
					return NULL;
				found=1;
				break;
			}
		}

		if(!found)
		{
			// check if it is parent inode
			*name = token;
			token = strtok(NULL, "/");

			if(token!=NULL)
			{
				return NULL;
			}

			return cur_ind;
		}

		token = strtok(NULL, "/");
	}
	return NULL;
}

int update_dir_info(inode *parent_ind, int file_idx, dir_info *info, super_block *sup, int parent_inumber)
{
	int entries_in_one_block = 4096/sizeof(dir_info);
	int block_num = file_idx/entries_in_one_block;
	int block_off = file_idx%entries_in_one_block;

	//assuming file entries are stored only in direct pointer
	if(block_num>=5)
		return -1;
	
	if(parent_ind->size/sizeof(dir_info) <= file_idx)
	{
		if(parent_ind->direct[block_num]==-1)
		{
			int in;
			for(int i=0;i<sup->data_blocks;i++)
			{
				if(!get_bitmap(gl_data_bitmap, i))
				{
					in=i;
					break;
				}
			}
			
			set_bitmap(gl_data_bitmap, in);
			update_data_bitmap(sup, in, 1);
			parent_ind->direct[block_num]=in;
		}
		parent_ind->size += sizeof(dir_info);
		update_inode(sup, parent_inumber, parent_ind);
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, sup->data_block_idx + parent_ind->direct[block_num], buffer)<0)
		return -1;
	memcpy(buffer+sizeof(dir_info)*block_off, info, sizeof(dir_info));
	if(write_block(gl_diskptr, sup->data_block_idx + parent_ind->direct[block_num], buffer)<0)
		return -1;
	free(buffer);
	return 0;
}

int add_file_into_dir_list(inode *parent_ind, int inumber, char *name, int type, super_block *sup, int parent_inumber)
{
	int num_files = parent_ind->size / sizeof(dir_info);
	dir_info *dir_files = (dir_info *)malloc(num_files * sizeof(dir_info));

	if(get_files_from_inode(parent_ind, dir_files, sup)<0)
	{
		return -1;
	}
	//search for invalid
	int invalid_found=0;
	for(int i=0;i<num_files;i++)
	{
		if(dir_files[i].valid==0)
		{
			invalid_found=1;
			dir_files[i].valid=1;
			dir_files[i].type=type;
			strcpy(dir_files[i].name, name);
			dir_files[i].length=strlen(name);
			dir_files[i].inumber=inumber;
			update_dir_info(parent_ind, i, &dir_files[i], sup, parent_inumber);
			break;
		}
	}
	if(!invalid_found)
	{
		dir_info *cur_info = (dir_info *)malloc(sizeof(dir_info));
		cur_info->valid=1;
		cur_info->type=type;
		strcpy(cur_info->name, name);
		cur_info->length=strlen(name);
		cur_info->inumber=inumber;
		update_dir_info(parent_ind, num_files, cur_info, sup, parent_inumber);
	}
	return 0;
}

int create_dir(char *dirpath)
{
	if(!is_mounted)
	{
		printf("Create Dir Error: Not Mounted\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Create Dir Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	// if no root directory
	if(get_bitmap(gl_inode_bitmap,0)==0)
	{
		if(strcmp(dirpath, "/") == 0)
		{
			// create root directory
			inode *ind = get_inode(&sup, 0);
			ind->valid=1;
			ind->size=0;
			for(int i=0;i<5;i++)
				ind->direct[i]=-1;
			ind->indirect=-1;
			set_bitmap(gl_inode_bitmap,0);

			//update inode bitmap
			read_block(gl_diskptr, sup.inode_bitmap_block_idx, buffer);
			memcpy(buffer, gl_inode_bitmap, 1);
			write_block(gl_diskptr, sup.inode_bitmap_block_idx, buffer);

			//update inode
			if(update_inode(&sup, 0, ind)<0)
			{
				free(buffer);
				printf("Create Dir Error: root dir inode update failed\n");
				return -1;
			}

			printf("Create Dir Success: %s\n", dirpath);
			free(buffer);
			return 0;
		}
		else
		{
			free(buffer);
			printf("Create Dir Error: No root directory\n");
			return -1;
		}
	}

	if(strcmp(dirpath, "/")==0)
	{
		free(buffer);
		printf("Create Dir Error: Root directory already exist\n");
		return -1;
	}

	inode *root_dir_inode = get_inode(&sup, 0);
	if(root_dir_inode==NULL)
	{
		free(buffer);
		printf("Create dir Error: Cannot get root directory inode\n");
		return -1;
	}

	char *name;
	int parent_inumber;
	inode *parent_dir = get_parent_inode(root_dir_inode, dirpath, &sup, &name, &parent_inumber);
	if(parent_dir==NULL || strlen(name)>=MAX_NAME_LEN)
	{
		free(buffer);
		printf("Create dir Error: Cannot get parent directory or dir name big\n");
		return -1;
	}

	int new_ind = create_file();
	if(add_file_into_dir_list(parent_dir, new_ind, name, 1 /*dir*/, &sup, parent_inumber) < 0)
	{
		free(buffer);
		printf("Create dir Error: cannot add file to dir list\n");
		return -1;
	}


	free(buffer);
	printf("Create Dir Success: %s\n", dirpath);
	return 0;
}

inode *get_file_inode_from_path(inode *root_dir_inode, char dirpath[], super_block *sup, char **name, int *parent_inumber)
{
	inode *parent_ind = get_parent_inode(root_dir_inode, dirpath, sup, name, parent_inumber);

	if(parent_ind==NULL)
		return NULL;

	int num_files = parent_ind->size / sizeof(dir_info);
	dir_info *dir_files = (dir_info *)malloc(num_files * sizeof(dir_info));

	if(get_files_from_inode(parent_ind, dir_files, sup)<0)
	{
		return NULL;
	}

	//search for file
	for(int i=0;i<num_files;i++)
	{
		if(dir_files[i].valid && dir_files[i].type==0 && strcmp(dir_files[i].name, *name)==0)
		{
			*parent_inumber=dir_files[i].inumber;
			return get_inode(sup, *parent_inumber);
		}
	}

	return NULL;
}

inode *get_file_inode_from_path_write(inode *root_dir_inode, char dirpath[], super_block *sup, char **name, int *parent_inumber)
{
	inode *parent_ind = get_parent_inode(root_dir_inode, dirpath, sup, name, parent_inumber);
	if(parent_ind==NULL)
		return NULL;

	int num_files = parent_ind->size / sizeof(dir_info);
	dir_info *dir_files = (dir_info *)malloc(num_files * sizeof(dir_info));

	if(get_files_from_inode(parent_ind, dir_files, sup)<0)
	{
		return NULL;
	}

	//search for file
	for(int i=0;i<num_files;i++)
	{
		if(dir_files[i].valid && dir_files[i].type==0 && strcmp(dir_files[i].name, *name)==0)
		{
			*parent_inumber=dir_files[i].inumber;
			return get_inode(sup, *parent_inumber);
		}
	}

	int new_ind = create_file();
	if(add_file_into_dir_list(parent_ind, new_ind, *name, 0, sup, *parent_inumber)<0)
	{
		return NULL;
	}

	*parent_inumber= new_ind;
	return get_inode(sup, new_ind);
}

int read_file(char *filepath, char *data, int length, int offset)
{
	if(!is_mounted)
	{
		printf("Read File Error: Not Mounted\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Read File Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	inode *root_dir_inode = get_inode(&sup, 0);
	if(root_dir_inode==NULL)
	{
		free(buffer);
		printf("Read File Error: Cannot get root directory inode\n");
		return -1;
	}

	char *name;
	int parent_inumber;
	inode *parent_dir = get_file_inode_from_path(root_dir_inode, filepath, &sup, &name, &parent_inumber);

	if(parent_dir==NULL || strlen(name)>=MAX_NAME_LEN)
	{
		free(buffer);
		printf("Read File Error: Cannot get parent directory or dir name big\n");
		return -1;
	}

	int ret = read_i(parent_inumber, data, length, offset);

	if(ret<0)
	{
		free(buffer);
		printf("Read File Error: cannot read fom file\n");
		return -1;
	}

	free(buffer);
	return ret;
}

int write_file(char *filepath, char *data, int length, int offset)
{
	if(!is_mounted)
	{
		printf("Write File Error: Not Mounted\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Write File Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	inode *root_dir_inode = get_inode(&sup, 0);
	if(root_dir_inode==NULL)
	{
		free(buffer);
		printf("Write File Error: Cannot get root directory inode\n");
		return -1;
	}

	char *name;
	int parent_inumber;
	inode *parent_dir = get_file_inode_from_path_write(root_dir_inode, filepath, &sup, &name, &parent_inumber);

	if(parent_dir==NULL || strlen(name)>=MAX_NAME_LEN)
	{
		free(buffer);
		printf("Write File Error: Cannot get parent directory or dir name big\n");
		return -1;
	}

	int ret = write_i(parent_inumber, data, length, offset);

	if(ret<0)
	{
		free(buffer);
		printf("Write File Error: cannot write to file\n");
		return -1;
	}

	free(buffer);
	return ret;
}

inode *rem_top_inode_and_contents(inode *root_dir_inode, char dirpath[], super_block *sup, char **name, int *parent_inumber)
{
	*parent_inumber=0;
	char rest[1000];
	strcpy(rest,dirpath);
	char *token = strtok(rest, "/");
	if(token==NULL)
	{
		return NULL;
	}

	inode *cur_ind = root_dir_inode;
	int prev_inumber=0;
	while(token!=NULL)
	{
		*name=token;
		int num_files = cur_ind->size / sizeof(dir_info);
		dir_info *dir_files = (dir_info *)malloc(num_files * sizeof(dir_info));

		if(get_files_from_inode(cur_ind, dir_files, sup)<0)
		{
			return NULL;
		}

		int found=-1;

		//search for file
		for(int i=0;i<num_files;i++)
		{
			if(dir_files[i].valid && dir_files[i].type==1 && strcmp(dir_files[i].name, token)==0)
			{
				found = i;
				cur_ind=get_inode(sup, dir_files[i].inumber);
				if(cur_ind<0)
					return NULL;
				break;
			}
		}

		if(found==-1)
		{
			return NULL;	
		}

		token = strtok(NULL, "/");

		if(token==NULL)
		{
			dir_files[found].valid=0;
			*parent_inumber=prev_inumber;
			inode *par_temp =  get_inode(sup, prev_inumber);
			update_dir_info(par_temp, found, &dir_files[found], sup, prev_inumber);

			return par_temp;
		}
		prev_inumber=dir_files[found].inumber;
	}
	return NULL;
}

int remove_dir(char *dirpath)
{
	if(!is_mounted)
	{
		printf("Remove Dir Error: Not Mounted\n");
		return -1;
	}

	char *buffer = (char *)malloc(4096);
	if(read_block(gl_diskptr, 0, buffer)<0)
	{
		free(buffer);
		printf("Remove Dir Error: Super Block Read Failed\n");
		return -1;
	}

	super_block sup;
	memcpy(&sup, buffer, sizeof(super_block));

	if(strcmp(dirpath, "/")==0)
	{
		free(buffer);
		printf("Remove Dir Error: Root directory cannot be removed\n");
		return -1;
	}

	inode *root_dir_inode = get_inode(&sup, 0);
	if(root_dir_inode==NULL)
	{
		free(buffer);
		printf("Remove dir Error: Cannot get root directory inode\n");
		return -1;
	}

	char *name;
	int parent_inumber;
	inode *parent_dir = rem_top_inode_and_contents(root_dir_inode, dirpath, &sup, &name, &parent_inumber);
	if(parent_dir==NULL)
	{
		free(buffer);
		printf("Remove dir Error: cannot remove dir\n");
		return -1;
	}

	free(buffer);
	printf("Remove Dir Success: %s\n", dirpath);
	return 0;
}