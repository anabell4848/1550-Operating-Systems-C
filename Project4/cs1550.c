/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
	
	Hanyu Xiong
	CS1550
	Project 4

	cd /u/OSLab/hax12/p4/fuse-2.7.0/example
	
	./cs1550 -d testmount
	
	fusermount -u testmount
	
	getattr 	x-0 	stat yo 	correctly set structure		
				x-2 	stat y		ENOENT file is not found		
	readdir 	x-0 	ls yo		on success		
				x-2 	ls y 		ENOENT directory is not valid or found		
	mkdir  		x-0 	mkdir yoyo			on success 		
				x-36 	mkdir yoyoyoyoo		ENAMETOOLONG 8 chars		
				x-1 	mkdir yo/yo 		EPERM directory not under the root dir only 		
			o-17 		mkdir yo			EEXIST if the directory already exists		
	mknod		x-0		echo "yo"> yo/new  			on success 
				x-36 	echo "yo">yo/newnewnew		ENAMETOOLONG name beyond 8.3 chars 
				x-1 	echo "yo">new 				EPERM file is trying to be created in the root dir 
			o-17 			EEXIST file already exists
	write 		x-				size on success 
				-27 		EFBIG offset beyond file size (but handle appends)
	read		x-		cat yo/new 				size read on success
			o-21 			EISDIR path is a directory
	unlink		x-0 	unlink yo/new			read on success				
			o-21	 		EISDIR path is a directory		
				x-2 	unlink yo/y 			ENOENT file is not found	
*/ 

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define	MAX_FILES_IN_DIR (BLOCK_SIZE - (MAX_FILENAME + 1) - sizeof(int)) / \
	((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK BLOCK_SIZE

//How many pointers in an inode?
#define NUM_POINTERS_IN_INODE ((BLOCK_SIZE - sizeof(unsigned int) - sizeof(unsigned long)) / sizeof(unsigned long))

struct cs1550_directory_entry
{
	char dname[MAX_FILENAME	+ 1];	//the directory name (plus space for a nul)
	int nFiles;			//How many files are in this directory. 
					//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;			//file size
		long nStartBlock;		//where the first block is on disk
	} files[MAX_FILES_IN_DIR];		//There is an array of these
};

typedef struct cs1550_directory_entry cs1550_directory_entry;

struct cs1550_disk_block
{
	//And all of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;


//------------------------------my functions--------------------------------

//looks for a directory and returns index of directory entry in .directories
int get_dir(cs1550_directory_entry* dir_struct, char* dir_name){
	int read, result, returnVal;
	FILE* directories = fopen(".directories","rb");
	fread(dir_struct, sizeof(*dir_struct), 1, directories);
    //not at end of directories & name is not correct
    while (!feof(directories)){
		if (strcmp(dir_name,dir_struct->dname)==0){
			break;
		}
        read = fread(dir_struct, sizeof(*dir_struct), 1, directories);
        if (read == 0) {
			break;
		}
    }
	//if the name was correct then the directory was found at sizeof(dir_struct) bytes ago
	if (strcmp(dir_struct->dname,dir_name)!=0){
		returnVal= -1;
	}
	else{ 
		result = ftell(directories) - sizeof(*dir_struct);
		returnVal = result;
	}
	fclose(directories);
	return returnVal;
}

//returns the index of a file in a directory
int find_file(cs1550_directory_entry* dir, char* filename, char* extension){
	int i=0;
	for (i = 0; i < dir->nFiles; i++){
		if (strcmp(filename, dir->files[i].fname) == 0 && strcmp(extension, dir->files[i].fext) == 0){
			return i;
		}
	}
	return -1;
}

//calculates size (in blocks) of bitmap:
int get_bitmap_size()
{
	FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
	FILE* f = fopen(".disk", "rb+");
	fseek(f, 0, SEEK_END);
	
	int bytes_on_disk, blocks_on_disk, bitmap_blocks_needed;
	bytes_on_disk = ftell(f);
	fclose(f);
	
	blocks_on_disk = bytes_on_disk / 512; //keep as is to round down so you don't have a half sized block at end
	bitmap_blocks_needed = (blocks_on_disk / 8 + 1) / 512 + 1;
	
	return bitmap_blocks_needed;
}

void change_bit(int i, int sign)
{	
	int byte_to_set;
	//printf("changing bit %d to %d\n",i,sign);
	//opening the disk
	FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
	FILE* f = fopen(".disk", "rb+");
	//we were given a bit to set. we must adjust this to a bit within a byte
	byte_to_set = i / 8;
	i = i % 8;
	unsigned char operand = 1 << i;
	//now we are setting the ith bit in the byte_to_set byte
	fseek(f, byte_to_set, SEEK_SET);
	//to set a single bit, we must read what is already there and or on the bit we want to set
	unsigned char curr_byte = 0;
	fread(&curr_byte, sizeof(char), 1, f);
	if(sign != -1) //if we are unsetting the bit
	{
		curr_byte = curr_byte | operand;
	}
	else //if we are setting the bit
	{
		operand = ~operand;
		curr_byte = curr_byte & operand;
	}
	fseek(f, -1, SEEK_CUR);
	fwrite(&curr_byte, sizeof(char), 1, f);
	fclose(f);
	return;
}

//Returns 1 if the block is allocated
//Returns 0 otherwise
int get_state(int block_idx){
	int byte_idx, bit_idx;
	unsigned char target_byte;
	
	byte_idx = block_idx / 8;
	bit_idx = block_idx % 8;
	
	FILE* f = fopen(".disk", "rb+");
	fseek(f,byte_idx,SEEK_SET);
	fread(&target_byte,sizeof(char),1,f);
	unsigned char operand = 1 << bit_idx;
	unsigned char result = target_byte & operand;
	result = result >> bit_idx;
	fclose(f);
	return result;
}

//find num_blocks contiguous free blocks using the next fit algorithm
int find_free_space(int num_blocks)
{
	int this_bit, run_start = -1, run_length = 0, cur_block = 0;
    //we start by openning up the .disk and reading through it bitwise
    FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
    while(cur_block <= last_bitmap_index())
    {
		this_bit = get_state(cur_block);

		if(this_bit == 1)
        {
        	run_start = -1;
        	run_length = 0;
        }
		else if(this_bit == 0)
        {
        	if(run_start == -1) //if this is the first in a run of 0s, record this location as the run's start
            {
            	run_start = cur_block; //combining the byte we are in with the bit in that byte to get the overall bit in the map
            }
            run_length++;
            if(run_length >= num_blocks) //we found a big enough run of 0s!
            {
            	return run_start;
            }
        }

		cur_block++;
    }
    return -1;
}
//makes the bitmap if it doesn't exist
void check_bitmap()
{
	//opening the disk
	FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
	FILE* f = fopen(".disk", "rb+");
	fseek(f, 0, SEEK_SET);
	char first_char = 0;
	fread(&first_char, sizeof(char), 1, f);
	int i = 0;
	if(first_char == 0) //then the beginning of the bitmap is zero and thus the bitmap does not exist
	{
		int bitmap_size = get_bitmap_size();
		//printf("DEBUG: creating new bitmap of size %d\n",bitmap_size);
		for(i = 0; i < bitmap_size; i++)
		{
			change_bit(i, 1);
		}
	}
	fclose(f);
	return;
}

int last_bitmap_index()
{
    FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
    FILE* f = fopen(".disk", "rb+");
    fseek(f, 0, SEEK_END);
    int bytes_on_disk = ftell(f);
	fclose(f);
    int blocks_on_disk = (bytes_on_disk / 512) - 1; //keep as is to round down so you don't have a half sized block at end
    
    return blocks_on_disk;
}


//------------------------------write these functions--------------------------------
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	FILE* temp = fopen(".directories","a");
	fclose(temp);
	struct cs1550_directory_entry cur_dir;
    FILE* f = fopen(".directories","r");
    fread(&cur_dir, sizeof(cur_dir), 1, f);

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	directory[0] = 0;
	filename[0] = 0; 
	extension[0] = 0;
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

    memset(stbuf, 0, sizeof(struct stat));
	
    //while not at end of directories, while name is not correct
    while (!feof(f)){
		if (strcmp(cur_dir.dname,directory)==0)
			break;
        int read = fread(&cur_dir, sizeof(cur_dir), 1, f);
        if (read == 0) 
			break;
    }
    fclose(f);

	//this triggers when we find the directory
    if (strcmp(path,"/")==0 || strcmp(cur_dir.dname,directory)==0){
        //if we are looking for directory attributes
		if (strlen(filename) == 0){
			res = 0;
       		stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		//if we are looking for a file which is there
		else{
			int file_index = find_file(&cur_dir,filename,extension);
			
			if (file_index != -1){
				res = 0;
				stbuf->st_blocks = cur_dir.files[file_index].fsize / 512;
				stbuf->st_size = cur_dir.files[file_index].fsize;
				stbuf->st_blksize = 512;
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 1;
				if (cur_dir.files[file_index].fsize % 512 != 0){
					stbuf->st_blocks++;
				}
			}
			else{	//-1 means not found
				res = -ENOENT;
			}
		}
    }
    else{
        res = -ENOENT;
    }
    return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	
	int found = 0, i;
	char name[13];
	name[0] = 0;
	
	FILE* temp = fopen(".directories","a");
	fclose(temp);
	FILE* f = fopen(".directories","r");		
	struct cs1550_directory_entry cur_dir;

	//Check if we want to read the root, if so read it
	if (strcmp(path, "/") != 0){
		while(fread(&cur_dir,sizeof(cur_dir),1,f) > 0 && !feof(f)){
			int number_of_files = cur_dir.nFiles;
			if (strcmp(cur_dir.dname,path + 1)==0){
				found = 1;
				for (i = 0; i < number_of_files; i++){
					strcat(name,cur_dir.files[i].fname);
					if (strlen(cur_dir.files[i].fext) > 0){ 
						strncat(name,".", 1);
					}
					strcat(name,cur_dir.files[i].fext);
					filler(buf,name,NULL,0);
					
					name[0] = 0;
				}
			}
		}
		if (!found){
			fclose(f);
			return -ENOENT;
		}
	}
	else{
		while(fread(&cur_dir,sizeof(cur_dir),1,f) > 0 && !feof(f)){
			filler(buf, cur_dir.dname, NULL, 0);
		}
	}
	//if we arent trying to read the root, see if the directory we want to read exists
	fclose(f);
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	//check if directory exists
	FILE* temp = fopen(".directories","a");
	fclose(temp);
	
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	file[0] = 0;	//make sure file starts as zero length
	struct cs1550_directory_entry cur_dir;
    struct cs1550_directory_entry next_dir;

	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext); 
	
	if (strlen(dir) > MAX_FILENAME) 
		return -ENAMETOOLONG;
	if (strlen(file) > 0) 
		return -EPERM;
	
	FILE* g = fopen(".directories","r");
	fread(&cur_dir, sizeof(cs1550_directory_entry), 1, g);
	while (strcmp(cur_dir.dname,path + 1) && fread(&cur_dir, sizeof(cs1550_directory_entry), 1, g) > 0);
	fclose(g);
	
	if (strcmp(cur_dir.dname,path + 1) == 0) {
		return -EEXIST;
	}
	
    strcpy(next_dir.dname, path + 1);
    next_dir.nFiles = 0;
	
    FILE* f = fopen(".directories", "a");
    fwrite(&next_dir, sizeof(cs1550_directory_entry),  1, f);
    fclose(f);

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	int dir_idx, file_idx, new_file_idx;
	
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	extension[0] = 0;
	filename[0] = 0;
	directory[0] = 0;
	cs1550_directory_entry cur_dir;
	

	check_bitmap();

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	
	if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION ) 
		return -ENAMETOOLONG;
	if (strlen(filename) == 0) 
		return -EPERM;		//if we are trying to create a file in root, parse path returns null for file and ext strings
	
	dir_idx = get_dir(&cur_dir,directory);
	file_idx = find_file(&cur_dir,filename,extension);
	new_file_idx = cur_dir.nFiles; 
	
	//find directory, make sure it exists
	if (dir_idx == -1) 
		return -ENOENT;
	
	//make sure file does not exist
	if (file_idx != -1) 
		return -EEXIST;

	//now actually make the file
	if (new_file_idx > MAX_FILES_IN_DIR) 
		return -EPERM; //if the directory is full return a permission error

	FILE* f = fopen(".directories","rb+");
	
	strcpy(cur_dir.files[new_file_idx].fext,extension);
	strcpy(cur_dir.files[new_file_idx].fname,filename);
	cur_dir.nFiles++;
	//int new_file_block = -1;// find_free_space(1);
	cur_dir.files[new_file_idx].nStartBlock = -1;
	cur_dir.files[new_file_idx].fsize = 0;


	fseek(f,dir_idx,SEEK_SET);
	fwrite(&cur_dir,sizeof(cs1550_directory_entry),1,f);

	fclose(f);
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
	int res = 0;
    char directory[MAX_FILENAME+1];
    char filename[MAX_FILENAME+1];
    char extension[MAX_EXTENSION+1];
    directory[0] = 0;
    filename[0] = 0; //initialize file target to null string
    extension[0] = 0;
	
    struct stat stbuf;
    int attr = cs1550_getattr(path, &stbuf);
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	
    if(!(strlen(filename) > 0))//it is a directory
    {
        return -EISDIR;
    }
	
    cs1550_directory_entry dir_struct;
    int dir_idx = get_dir(&dir_struct, directory);
    if(dir_idx == -1)
    {
        return -ENOENT; //somehow missed this earlier, shouldn't happen
    }
	if(attr != 0)
    {
        return attr; //file was not found, return -ENOENT.
    }
    //so to delete, we must unset the bits it takes up in the bitmap and remove its entry in the listing
    //first we use directory to find its directory in .directories, then we find the file in that directory's list of files
    //that we can use and change to delete the file
    //so we have the directory itself on hand, now to get the file.
    int file_index = find_file(&dir_struct, filename, extension);
    
	//Bitmap management- only neccecary for files that take up non zero space
	if (dir_struct.files[file_index].fsize > 0){
		//printf("Deleting a file of size %d\n",dir_struct.files[file_index].fsize);
		int num_blocks = dir_struct.files[file_index].fsize / 512;
    	if(dir_struct.files[file_index].fsize % 512 != 0)
    	{
        	num_blocks++;
   		}
  		//free the blocks it used
		int i;
		for (i = dir_struct.files[file_index].nStartBlock; i < dir_struct.files[file_index].nStartBlock + num_blocks; i++){
			change_bit(i, -1);
		}

	}
	
	FILE* f = fopen(".directories","rb+");
	
	//removing all references to it in the .directories file
    //its data in .disk will eventually be overwritten since it is no longer represented in the bitmap (see call to unallocate).
    dir_struct.nFiles--;
	dir_struct.files[file_index] = dir_struct.files[dir_struct.nFiles - 1]; //moves the last file to where the dead one is
    
	fwrite(&dir_struct,sizeof(cs1550_directory_entry),1,f);
	fclose(f);
    return res;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;
	int dir_exists, file_index, max_read, read_index, bytes_read;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	directory[0] = 0;
	filename[0] = 0;
	extension[0] = 0;

	FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//check to make sure path exists
	cs1550_directory_entry cur_dir;
	dir_exists = get_dir(&cur_dir,directory);
	file_index = find_file(&cur_dir,filename,extension);
	if (dir_exists == -1 || file_index == -1) 
		return -ENOENT;
		
	//check that size > 0
	if (size <= 0) 
		return 0;		//Sure, why not, have your zero bytes
	//check that offset is <= to the file size
	if (offset > cur_dir.files[file_index].fsize) 
		return 0; //nothing left
	
	//figure out where to read from
	FILE* f = fopen(".disk","rb");
	max_read = cur_dir.files[file_index].fsize - offset;
	read_index = cur_dir.files[file_index].nStartBlock * 512 + offset;
	
	
	//read in data
	//get bytes read and return, or error
	fseek(f,read_index,SEEK_SET);
	if (max_read < size) 
		size = max_read;
	bytes_read = fread(buf,1,size,f);
	fclose(f);
	printf("Read %d bytes\n", bytes_read);
	return bytes_read;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	FILE* temp = fopen(".disk","a");	//just in case it wasn't precreated
	fclose(temp);
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	directory[0] = 0;
	filename[0] = 0;
	extension[0] = 0;
	cs1550_directory_entry cur_dir;
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//check to make sure path exists
	int dir_index = get_dir(&cur_dir,directory);
	int file_index = find_file(&cur_dir,filename,extension);
	
	if (dir_index == -1 || file_index == -1) 
		return -ENOENT;
		
	//check that size is > 0
	if (size <= 0) 
		return 0;		
	//check that offset is <= to the file size
	if (offset > cur_dir.files[file_index].fsize) 
		return 0; //nothing left
	
	//calculate number of bytes the file is growing by, it is difference of old filesize and last byte read
	int new_bytes = (offset + size) - cur_dir.files[file_index].fsize;
	int new_blocks = 0;
	
	//printf("New bytes needed: %d\n",new_bytes);
	
	int blocks_used = 0;
	while (blocks_used * BLOCK_SIZE < cur_dir.files[file_index].fsize){
		blocks_used++;
	}
	int bytes_left = (BLOCK_SIZE * blocks_used) - cur_dir.files[file_index].fsize;
	//printf("Bytes availible in current block: %d\n",bytes_left);
	int i = new_bytes;
	while (i > bytes_left){
		new_blocks++;
		i -= BLOCK_SIZE;
	}
	
	//If we have to move the file, some stuff has to happen
	//Otherwise
	//printf("New blocks needed: %d\n",new_blocks); 
	if (new_blocks > 0){
		int cur_blocks = cur_dir.files[file_index].fsize / BLOCK_SIZE;
		if (cur_dir.files[file_index].fsize % BLOCK_SIZE != 0){
			cur_blocks++;
		}
		//printf("Current blocks: %d\n",cur_blocks);	
		
		//only unallocate space for files that have been allocated space
		//must unallocate here to make sure the file does not prevent its own elongation
		int j;
		if (cur_dir.files[file_index].nStartBlock != -1){
			for (j = cur_dir.files[file_index].nStartBlock; j < cur_dir.files[file_index].nStartBlock + cur_blocks; j++){
				change_bit(j, -1);
			}
		}

		int new_start_loc = find_free_space(new_blocks + cur_blocks);
		//printf("Attempting to put file at %d\n",new_start_loc);
		//If get free space returned -1, then the space request is unsatisfiable via contiguous allocation
		if (new_start_loc == -1){
			int k;
			for (k = cur_dir.files[file_index].nStartBlock; k < cur_dir.files[file_index].nStartBlock + cur_blocks; k++){
				change_bit(k, 1);
			}
			return -ENOSPC;
		}
		
		int k;
		for (k = new_start_loc; k < new_start_loc + new_blocks + cur_blocks; k++){
			change_bit(k, 1);
		}
		cur_dir.files[file_index].nStartBlock = new_start_loc;
	}


	//now that we have that all worked out, write the data
	//printf("Bitmap ends at %d.  Writing at %d.\n",get_bitmap_size(),write_index);
	
	FILE* f = fopen(".disk","rb+");
	FILE* g = fopen(".directories","rb+");
	
	int write_index = cur_dir.files[file_index].nStartBlock * BLOCK_SIZE + offset;
	
	fseek(f,write_index,SEEK_SET);
	fwrite(buf,size,1,f);
	if (new_bytes > 0){
		cur_dir.files[file_index].fsize += new_bytes;
	}
	
	fseek(g,dir_index,SEEK_SET);
	fwrite(&cur_dir,sizeof(cs1550_directory_entry),1,g);
	
	fclose(f);
	fclose(g);
	printf("Wrote %d bytes\n", size);
	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
