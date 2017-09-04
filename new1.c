#include<stdio.h>
#include<sys/types.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<math.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>

//modes
const unsigned short BLOCK_SIZE = 512;
const unsigned short ISIZE = 32;
const unsigned short inode_alloc    = 0100000;
const unsigned short plainfile     = 000000;
const unsigned short largefile		= 010000;
const unsigned short directory     = 040000;

//global variables
int fd ;		//file descriptor 
unsigned int chainarray[256];		//array to link data blocks


//Superblock
typedef struct {
unsigned short isize;
unsigned short fsize;
unsigned short nfree;
unsigned short free[100];
unsigned short ninode;
unsigned short inode[100];
char flock;
char ilock;
char fmod;
unsigned short time[2];
} fs_super;
fs_super super; //instance of superblock

//Inode 
typedef struct {
unsigned short flags;
char nlinks;
char uid;
char gid;
char size0;
unsigned short size1;
unsigned short addr[8];				// since word size = 4 
unsigned short actime[1];
unsigned short modtime[2];
} fs_inode;
fs_inode inoderef;		//instance of inode

//structure definition for directory
typedef struct 
{
        unsigned short inode;
        char filename[13];
}dir;
dir newdir;			// instance of dir
dir dir1;			//second instance for duplicates


//functions used:

int initialize_fs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void create_root();
void read_block_char(char *target, unsigned int blocknum);
void read_block_int(int *target, unsigned int blocknum);
void write_block_int(unsigned int *target, unsigned int blocknum);
void write_block_char(char *target, unsigned int blocknum);
void write_inode(fs_inode inodeinstance, unsigned int inodenumber);
void freeblock(unsigned int block);
void chaindatablocks(unsigned short total_blcks);
unsigned int allocatedatablock();
unsigned short allocateinode();
void mkdirectory(int fd, char* secondword);
int directorywriter(fs_inode rootinode, dir dir);
int makeindirectblocks(int fd,int block_num);
void cpin(char* source, char* destination);
void cpout(char* source, char* destination);
void  parseInputForSlashes(char *line, char **args);


/*
*	initialize the file system 
*	arguments total number of blocks, total inodes
*	
*
*/

int initialize_fs(char* path, unsigned short total_blocks,unsigned short total_inodes )

{

char buffer[BLOCK_SIZE];
int bytes_written;

if((total_inodes%16) == 0)
super.isize = total_inodes/16;
else
super.isize = (total_inodes/16) + 1;

super.fsize = total_blocks;

unsigned short i = 0;

if((fd = open(path,O_RDWR|O_CREAT,0600))== -1)
    {
        printf("\n open() failed with error [%s]\n",strerror(errno));
        return 1;
    }

for (i = 0; i<100; i++)
	super.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.

super.nfree = 0;


super.ninode = 100;
for (i=0; i < 100; i++)
	super.inode[i] = i;		//initializing inode array to store inumbers.

super.flock = 'f'; 					//flock,ilock and fmode are not used.
super.ilock = 'i';					//initializing to fill up block
super.fmod = 'f';
super.time[0] = 0;
super.time[1] = 0;
lseek(fd,BLOCK_SIZE,0);

// Writing to super block 
 if((bytes_written =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE)
 {
 printf("\nERROR : error in writing the super block");
 return 0;
 }

// writing zeroes to all inodes in ilist
for (i=0; i<BLOCK_SIZE; i++)
	buffer[i] = 0;
for (i=0; i < super.isize; i++)
	write(fd,buffer,BLOCK_SIZE);

// calling chaining data blocks procedure
chaindatablocks(total_blocks);	

//filling free array to first 100 data blocks
for (i=0; i<100; i++)
	freeblock(i+2+super.isize);

// Make root directory
create_root();


return 1;
}

//function to read character array from the required block
void read_block_char(char *target, unsigned int blocknum)
{
		
         if (blocknum > super.isize + super.fsize + 2)
              
					printf(" Block number is greater than max file system block size for reading\n"); 
		else{			
        lseek(fd,blocknum*BLOCK_SIZE,SEEK_SET);
        read(fd, target, BLOCK_SIZE);
		}
}
//function to read integer array from the required block
void read_block_int(int *target, unsigned int blocknum)
{
        if (blocknum > super.isize + super.fsize + 2)
              
					printf(" Block number is greater than max file system block size for reading\n"); 
		else{			
        lseek(fd,blocknum*BLOCK_SIZE,SEEK_SET);
        read(fd, target, BLOCK_SIZE);
		}
}
//function to write integer array to the required block
void write_block_int(unsigned int *target, unsigned int blocknum)
{
		int bytes_written;
        if (blocknum > super.isize + super.fsize + 2)
                printf(" Block number is greater than max file system block size for writing\n");
		else{
        lseek(fd,blocknum*BLOCK_SIZE,SEEK_SET);
        if((bytes_written=write(fd, target, BLOCK_SIZE)) < BLOCK_SIZE)
			printf("\n Error in writing block number : %d", blocknum);
		}
}

//function to write character array to the required block
void write_block_char(char *target, unsigned int blocknum)
{
		int bytes_written;
        if (blocknum > super.isize + super.fsize + 2)
                printf(" Block number is greater than max file system block size for writing\n");
		else{
        lseek(fd,blocknum*BLOCK_SIZE,SEEK_SET);
        if((bytes_written=write(fd, target, BLOCK_SIZE)) < BLOCK_SIZE)
			printf("\n Error in writing block number : %d", blocknum);
		}
}
// Data blocks chaining procedure
void chaindatablocks(unsigned short total_blcks)
{
unsigned int emptybuffer[128];   // buffer to fill with zeros to entire blocks. Since integer size is 4 bytes, 512 * 4 = 2048 bytes.
unsigned int blockcounter;
unsigned int no_chunks = total_blcks/100;			//splitting into blocks of 100
unsigned int remainingblocks = total_blcks%100;		//getting remaining/left over blocks
unsigned int i = 0;

for (i=0; i<128; i++)
	emptybuffer[i] = 0;				//setting character array to 0 to remove any bad/junk data
for (i=0; i<128; i++)
	chainarray[i] = 0;				//setting integer array to 0 to remove any bad/junk data
	
//chaining for chunks of blocks 100 blocks at a time
for (blockcounter=0; blockcounter < no_chunks; blockcounter++)
{
	chainarray[0] = 100;
	
	for (i=0;i<100;i++)
	{
		if(blockcounter == (no_chunks - 1) && remainingblocks == 0 && i==0)
		{
		chainarray[i+1] = 0;
		continue;
		}
		chainarray[i+1] = 2+super.isize+i+100*(blockcounter+1);
	}	
	write_block_int(chainarray, 2+super.isize+100*blockcounter);
	
	for (i=1; i<=100;i++)
		write_block_int(emptybuffer, 2+super.isize+i+ 100*blockcounter);
}

//chaining for remaining blocks
chainarray[0] = remainingblocks;
chainarray[1] = 0;
for (i=1;i<=remainingblocks;i++)
		chainarray[i+1] = 2+super.isize+i+(100*blockcounter);

write_block_int(chainarray, 2+super.isize+(100*blockcounter));

for (i=1; i<=remainingblocks;i++)
		write_block_int(chainarray, 2+super.isize+1+i+(100*blockcounter));
		

for (i=0; i<128; i++)
		chainarray[i] = 0;
}
	
//function to write to an inode given the inode number	
void write_inode(fs_inode inodeinstance, unsigned int inodenumber)
{
int bytes_written;
lseek(fd,2*BLOCK_SIZE+inodenumber*ISIZE,SEEK_SET);
if((bytes_written=write(fd,&inodeinstance,ISIZE)) < ISIZE)
	printf("\n Error in writing inode number : %d", inodenumber);
		
}

//function to create root directory and its corresponding inode.
void create_root()
{
unsigned int i = 0;
unsigned short bytes_written;
unsigned int datablock = allocatedatablock();	

for (i=0;i<14;i++)
	newdir.filename[i] = 0; 
		
newdir.filename[0] = '.';			//root directory's file name is .
newdir.filename[1] = '\0';
newdir.inode = 1;					// root directory's inode number is 1.

inoderef.flags = inode_alloc | directory | 000077;   		// flag for root directory 
inoderef.nlinks = 2;
inoderef.uid = '0';
inoderef.gid = '0';
inoderef.size0 = '0';
inoderef.size1 = ISIZE;
inoderef.addr[0] = datablock;

for (i=1;i<8;i++)
	inoderef.addr[i] = 0;
		
inoderef.actime[0] = 0;
inoderef.modtime[0] = 0;
inoderef.modtime[1] = 0;

write_inode(inoderef, 0); 		

lseek(fd, datablock*BLOCK_SIZE, SEEK_SET);

	//filling 1st entry with .
if((bytes_written = write(fd, &newdir, 16)) < 16)
	printf("\n Error in writing root directory \n ");
	
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';
	// filling with .. in next entry(16 bytes) in data block.
	
if((bytes_written = write(fd, &newdir, 16)) < 16)
	printf("\n Error in writing root directory ");
	
}
	
//free data blocks and initialize free array
void freeblock(unsigned int block)
{
super.free[super.nfree] = block;
++super.nfree;
}

//function to get a free data block. Also decrements nfree for each pass
unsigned int allocatedatablock()
{
unsigned int block;

super.nfree--;

block = super.free[super.nfree];
super.free[super.nfree] = 0;

if (super.nfree == 0)
{
int n=0;
		read_block_int(chainarray, block);
		super.nfree = chainarray[0];
		for(n=0; n<100; n++)
				super.free[n] = chainarray[n+1];
}
return block;
}

//getting free inode
//only allocation performed
//if inode reaches 0, error caught but cannot proceed from that point.
unsigned short allocateinode()
{
unsigned short inumber;
unsigned int i = 0;
super.ninode--;
inumber = super.inode[super.ninode];
return inumber;
}

//create a new directory with the given name
//creates all directories under root directory only.
void mkdirectory(char* filename, unsigned int newinode)
{
	int blocks_read;
	unsigned short parentinum = 1;    			//since parent is always root directory for this project, inumber is 1.		
	char buffertemp[BLOCK_SIZE];
	int i =0;
	unsigned short block_num = allocatedatablock();
	strncpy(newdir.filename,filename,14);		//string copy filename contents to directory structure's field
	newdir.inode = newinode;
	lseek(fd,2*BLOCK_SIZE,SEEK_SET);
	
	
	inoderef.nlinks++;
	// set up this directory's inode
	inoderef.flags = inode_alloc | directory | 000777;
	inoderef.nlinks = 2;	
	inoderef.uid = '0';
	inoderef.gid = '0';
	inoderef.size0 = '0';
	inoderef.size1 = 128;
	for (i=1;i<8;i++)
			inoderef.addr[i] = 0;
	inoderef.addr[0] = block_num;
	inoderef.actime[0] = 0;
	inoderef.modtime[0] = 0;
	inoderef.modtime[1] = 0;
	write_inode(inoderef, newinode);
	lseek(fd,2*BLOCK_SIZE,SEEK_SET);
	blocks_read = read(fd,&inoderef,128);
	
	inoderef.nlinks++;
	
	if(directorywriter(inoderef, newdir))
	return;
	for (i=0;i<BLOCK_SIZE	;i++)
			buffertemp[i] = 0;
	// copying to inode numbers and filenames to directory's data block for ".".
	memcpy(buffertemp, &newinode, sizeof(newinode));		//memcpy(used for fixed width character array inbuilt function copies n bytes from memory area newinode to memory  area buffertemp
   	buffertemp[2] = '.';
	buffertemp[3] = '\0';
	// copying to inode numbers and filenames to directory's data block for ".." 
	memcpy(buffertemp+16, &parentinum, sizeof(parentinum));		//memcpy(used for fixed width character array inbuilt function copies n bytes from memory area newinode to memory  area buffertemp
	buffertemp[18] = '.';
	buffertemp[19] = '.';	
	buffertemp[20] = '\0';
	write_block_char(buffertemp, block_num);			//writing character array to newly allocated block
	
	printf("\n Directory created \n");
}

/*
void mkdirectory(int fileDesc, char* secondWord)
{
	//get the directory names --
	//goto root directory --
	//search for the first directory in root
	//continue searching the other directories
	
	char *argument[64];
	parseInputForSlashes(secondWord, argument);	
	
	lseek(fileDesc, 1032, SEEK_CUR); //go to addr[0] of inode #1
	unsigned short bufferForReadingAddrZero[1];
	read(fileDesc, bufferForReadingAddrZero, sizeof(bufferForReadingAddrZero));
	lseek(fileDesc, bufferForReadingAddrZero[0]*512, SEEK_SET);//goto root directory
	
	//search
	
	for(int i = 0; i < 64; i++)
	{
		int endOfBlockByteNo = 0;
		bool isFound = false;
		lseek(fileDesc, 34, SEEK_CUR);
		char bufferForReadingNext14Bytes[14];
		read(fileDesc, bufferForReadingNext14Bytes, sizeof(bufferForReadingNext14Bytes));
		endOfBlockByteNo = lseek(fileDesc, 0, SEEK_CUR);
		for(int j = 2; j <= 30; j++)
		{
			lseek(fileDesc, 2, SEEK_CUR);
			char bufferForReadingNext14Bytes[14];
			read(fileDesc, bufferForReadingNext14Bytes, sizeof(bufferForReadingNext14Bytes));
			
			if(strcmp(argument[i], bufferForReadingNext14Bytes) == 0)
			{
				isFound = true;
			}
			break;
		}
		
		if(isFound)
		{
			lseek(fileDesc, -16, SEEK_CUR);
			unsigned short bufferForReadingINodeNo[1];
			read(fileDesc, bufferForReadingINodeNo, sizeof(bufferForReadingINodeNo));
			lseek(fileDesc, 1064+(bufferForReadingINodeNo[0]-2)*32, SEEK_SET);
			unsigned short bufferForReadingAddrOfZero[1];
			read(fileDesc, bufferForReadingAddrOfZero, sizeof(bufferForReadingAddrOfZero));
			lseek(fileDesc, bufferForReadingAddrOfZero[0]*512, SEEK_SET);
		}
		else
		{
			//go to the super block
			lseek(fileDesc, 512, SEEK_SET);
			unsigned short bufferForReadingISize[1];
			read(fileDesc, bufferForReadingISize, sizeof(bufferForReadingISize));
		
			unsigned short bufferForReadingFSize[1];
			read(fileDesc, bufferForReadingFSize, sizeof(bufferForReadingFSize));
	
			int jValue = (bufferForReadingFSize[0]-(bufferForReadingISize[0]+2))/100;
			
			//find a free inode
			int countForInode = getFreeInode(fileDesc, bufferForReadingISize[0]);
	
			lseek(fileDesc, endOfBlockByteNo-480, SEEK_SET);				
	
			//writing the inode no.
			unsigned short bufferForINodeNo[1];
			memcpy(bufferForINodeNo, &countForInode, sizeof(countForInode));
			write(fileDesc, bufferForINodeNo, sizeof(bufferForINodeNo));

			//writing the native file name (thirdWord)
			char bufferForNativeFileName[14];
			memcpy(bufferForNativeFileName, &argument[i], sizeof(argument[i]));
			write(fileDesc, bufferForNativeFileName, sizeof(bufferForNativeFileName));
		
			lseek(fileDesc, -48, SEEK_CUR);
			unsigned short bufferForReadingInodeOfParent[1]; //parent w.r.t. to child
			read(fileDesc, bufferForReadingInodeOfParent, sizeof(bufferForReadingInodeOfParent));
		
			unsigned short freeDataBlockNo = getFreeDataBlock(fileDesc, jValue, bufferForReadingFSize[0]);
		
			//updated flags of inode #count
			unsigned short i_NodeCountFlags[1];
			i_NodeCountFlags[0] = 0140000;
			lseek(fileDesc, 1056+((countForInode-2)*32), SEEK_SET);
			write(fileDesc, i_NodeCountFlags, sizeof(i_NodeCountFlags));
		
			lseek(fileDesc, 6, SEEK_CUR);
			unsigned short bufferForDataBlockNo[1];
			memcpy(bufferForDataBlockNo, &freeDataBlockNo, sizeof(freeDataBlockNo));
			write(fileDesc, bufferForDataBlockNo, sizeof(bufferForDataBlockNo));
		
			lseek(fileDesc, freeDataBlockNo*512, SEEK_SET);
			//write inode #of itself
			unsigned short bufferForInodeOfItself[1]; // in the data block
			memcpy(bufferForInodeOfItself, &countForInode, sizeof(countForInode));
			write(fileDesc, bufferForInodeOfItself, sizeof(bufferForInodeOfItself));
		
			//write '.'
			char iNodeDot[] = ".";
			char bufferForInodeDot[14];
			memcpy(bufferForInodeDot, &iNodeDot, sizeof(iNodeDot));
			write(fileDesc, bufferForInodeDot, sizeof(bufferForInodeDot));

			//write inode #of parent
			unsigned short bufferForInodeOfParent[1];
			memcpy(bufferForInodeOfParent, &bufferForReadingInodeOfParent[0], sizeof(bufferForReadingInodeOfParent));
			write(fileDesc, bufferForInodeOfParent, sizeof(bufferForInodeOfParent));
		
			//write '..'
			char iNodeDotDot[] = "..";
			char bufferForInodeDotDot[14];
			memcpy(bufferForInodeDotDot, &iNodeDotDot, sizeof(iNodeDotDot));
			write(fileDesc, bufferForInodeDotDot, sizeof(bufferForInodeDotDot));
		}
		break;
	}	
}

void  parseInputForSlashes(char *line, char **args) 
{
        while (*line != '\0') 
        {
                // strip the whitespaces. 
                // use '\0' so that the previous argument is terminated automatically 
                while (*line == '/')
                        *line++ = '\0';

                //save the argument position 
                *args++ = line;

                //skip over the argument 
                while (*line != '\0' && *line != '/')
                        line++;
                }

                //mark the end of argument list  
                *args = '\0';
}

*/

//function to write to directory's data block
//gets inode(always root directory's inode from mkdir) and directory (struct's) reference as inputs.
int directorywriter(fs_inode rootinode, dir dir)
{

int duplicate =0;  		//to find duplicate named directories.
unsigned short addrcount = 0;
char dirbuf[BLOCK_SIZE];		//array to 
int i=0;
 for (addrcount=0;addrcount <= 7;addrcount++)
{
lseek(fd,rootinode.addr[addrcount]*BLOCK_SIZE, SEEK_SET);
for (i=0;i<32;i++)
{		
read(fd, &dir1, 16);
if(strcmp(dir1.filename,dir.filename) == 0)			//check for duplicate named directories
{
printf("Cannot create directory.The directory name already exists.\n");
duplicate=1;
break;
}
}
}
if(duplicate !=1) 		
{
for (addrcount=0;addrcount <= 7;addrcount++)			//for each of the address elements ( addr[0],addr[1] till addr[27]), check which inode is not allocated
{
read_block_char(dirbuf, rootinode.addr[addrcount]);
for (i=0;i<32;i++)										//Looping for each directory entry (2048/16 = 128 entries in total, where 2048 is block size and 16 bytes is directory entry size)
{		
		
	if (dirbuf[16*i] == 0) // if inode is not allocated
	{
	memcpy(dirbuf+16*i,&dir.inode,sizeof(dir.inode));
	memcpy(dirbuf+16*i+sizeof(dir.inode),&dir.filename,sizeof(dir.filename));		//using memcpy function to copy contents of filename and inode number, to store it in directory entry.
	write_block_char(dirbuf, rootinode.addr[addrcount]);
	return duplicate;
	}
}
}
}
return duplicate;
}


//cpin or copy in function : 
//command : cpin <source_file_path_in_foreign_operating_system> <destination_filename_in_v6>
//performs of copy of contents from source file in foreign operating system(external file) to
//existing or newly created destination file given in the command.
void cpin(char* source, char* destination)
{
		int indirect = 0;
		int indirectfn_return =1;
		char reader[BLOCK_SIZE];
		int bytes_read;
		int srcfd;
		int extfilesize =0;
		//open external file
		 if((srcfd = open(source, O_RDONLY)) == -1)
			{
			printf("\nerror opening file: %s \n",source);
			return;
			}
        int inumber = allocateinode();
		if(inumber < 0)
		{
		 printf("Error : ran out of inodes \n");
		 return;
		}
        unsigned int newblocknum;
		
		//preapare new file in V6 file system
		newdir.inode = inumber;
		memcpy(newdir.filename,destination,strlen(destination));
		//write inode for the new file
		inoderef.flags = inode_alloc | plainfile | 000777;
        inoderef.nlinks = 1; 
        inoderef.uid = '0';
        inoderef.gid = '0';
		inoderef.size0='0';
		
		int i =0;
		
		//start reading external file and perform file size calculation simultaneously	
		while(1)
		{
		if((bytes_read=read(srcfd,reader,BLOCK_SIZE)) != 0 )
		{
		newblocknum = allocatedatablock();
		write_block_char(reader,newblocknum);
		inoderef.addr[i] = newblocknum;
		// When bytes returned by read() system call falls below the block size of 
		//2048, reading and writing are complete. Print file size in bytes and exit
		if(bytes_read < BLOCK_SIZE)
		{
		extfilesize = i*BLOCK_SIZE + bytes_read;
		printf("Small file copied\n");
		inoderef.size1 = extfilesize;
		printf("File size = %d bytes\n",extfilesize);
		break;
		}
		i++;
		
		//if the counter i exceeds 27(maximum number of elements in addr[] array,
		//transfer control to new function that creates indirect blocks which
		//handles large files(file size > 56 KB).
		if(i>27)
		{
		
		indirectfn_return=makeindirectblocks(srcfd,inoderef.addr[0]);
		indirect = 1;
		break;
		}
		}
		// When bytes returned by read() system call is 0,
		// reading and writing are complete. Print file size in bytes and exit
		else
		{
		extfilesize = i*BLOCK_SIZE;
		printf("Small file copied\n");
		inoderef.size1 = extfilesize;
		printf("File size = %d bytes\n",extfilesize);
		break;
		}
		
		}
inoderef.actime[0] = 0;
inoderef.modtime[0] = 0;
inoderef.modtime[1] = 0;	

//if call is made to function that creates indirect blocks,
//it is a large file. Set flags for large file to 1.
if(indirect == 1)
{
inoderef.flags = inoderef.flags | largefile;
}
//write to inode and directory data block
if(	indirectfn_return > -1)
{
write_inode(inoderef,inumber);
lseek(fd,2*BLOCK_SIZE,SEEK_SET);
read(fd,&inoderef,ISIZE);
 inoderef.nlinks++;
directorywriter(inoderef,newdir); 
}
if(indirectfn_return == -1)
{
printf("\nExitting as file is large..");
}
}


//function that creates indirect blocks. handles large file (file size > 56 KB)
//largest file size handled : ( 28 * 512 * 2048 ) /1024 = 28672 KB.
int makeindirectblocks(int fd,int block_num)
{
char reader[BLOCK_SIZE];
unsigned int indirectblocknum[512];				//integer array to store indirect blocknumbers
int i=0;
int j=0;
int bytes_read;
int blocks_written = 0;
int extfilesize = 8 * BLOCK_SIZE;    		//filesize is initialized to small file size since data would have been read upto this size.
for(i=0;i<8;i++)
indirectblocknum[i] = inoderef.addr[i];		//transfer existing block numbers in addr[] array to new temporary array

inoderef.addr[0] = allocatedatablock();		//allocate a data block which will be used to store the temporary integer array of indirect block numbers

for(i=1;i<8;i++)
inoderef.addr[i] = 0;

i=8;
while(1)
		{
		if((bytes_read=read(fd,reader,BLOCK_SIZE)) != 0 )
		{
		 indirectblocknum[i] = allocatedatablock();  //allocate a data block which will be used to store the temporary integer array of indirect block numbers
		 write_block_char(reader,indirectblocknum[i]);			
		  i++;
		  
		  // When bytes returned by read() system call falls below the block size of 
		//2048, reading and writing are complete. Print file size in bytes and exit
		 if(bytes_read < BLOCK_SIZE)
		{
		write_block_int(indirectblocknum, inoderef.addr[j]);
		printf("Large File copied\n");
		extfilesize = extfilesize + blocks_written*BLOCK_SIZE + bytes_read;
		inoderef.size1 = extfilesize;
		printf("File size = %d bytes\n",extfilesize);
		break;
		}
		blocks_written++;
		//When counter i reaches 512, first indirect block is full. So reset counters to 0
		//allocate new block to store it in addr[] array that will be the new indirect block.
		
		if(i>511 && j<=7)
		{
		write_block_int(indirectblocknum, inoderef.addr[j]);
		inoderef.addr[++j] = allocatedatablock();
		i=0;
		extfilesize = extfilesize + 512*BLOCK_SIZE;
		blocks_written=0;
		}
		//if all the elements in addr[] array have been exhausted with indirect blocks, maximum capacity of
		//28672 KB has been reached. Throw an error that the file is too large for this file system.
		if(j>7)
		{
		printf("This file copy is not supported by the file system as the file is very large\n");
		return -1;
		break;
		}
		}
		// When bytes returned by read() system call is 0,
		// reading and writing are complete. Print file size in bytes and exit
		else
		{
		write_block_int(indirectblocknum, inoderef.addr[j]);
		inoderef.size1 = extfilesize;
		printf("Large File copied\n");
		printf("File size = %d bytes\n",extfilesize);
		break;
		}
		
}
return 0;
}


//cpout or copy out function : 
//command : cpout <source_file_name_in_v6> <destination_filename_in_foreign_operating_system>
//performs of copy of contents from existing source file in V6 to
//existing or newly created destination file in foreign operating system(external file) 
//given in the command.
void cpout(char* src, char* targ)
{
		int indirect = 0;
		int found_dir = 0;
		int src_inumber;
		char reader[BLOCK_SIZE];						//reader array to read characters (contents of blocks or file contents)
		int reader1[BLOCK_SIZE];						//reader array to read integers (block numbers contained in add[] array)
		int bytes_read;
		int targfd;
		int i=0;
		int j=0;
		int addrcount=0;
		int total_blocks=0;
		int remaining_bytes =0;
		int indirect_block_chunks = 0;						//each chunk of indirect blocks contain 512 elements that point to data blocks
		int remaining_indirectblks=0;
		int indirectblk_counter=0;
		int bytes_written=0;
		
		//open or create external file(target file) for read and write
		 if((targfd = open(targ, O_RDWR | O_CREAT, 0600)) == -1)
			{
			printf("\nerror opening file: %s\n",targ);
			return;
			}
		lseek(fd,2*BLOCK_SIZE,0);
		read(fd,&inoderef,ISIZE);
		
			//find the source V6 file in the root directory
		for (addrcount=0;addrcount <= 7;addrcount++)
		{
		if(found_dir !=1)
		{
		lseek(fd,(inoderef.addr[addrcount]*BLOCK_SIZE), 0);
		for (i=0;i<128;i++)
		{		if(found_dir !=1)
				{
				read(fd, &dir1, 16);
			
				if(strcmp(dir1.filename,src) == 0)
				{
				
				src_inumber = dir1.inode;
				found_dir =1;
				}
				}
			
		}
		}
		}
		
		if(src_inumber == 0)
		{
		printf("File not found in the file system. Unable to proceed\n");
		return;
		}
		lseek(fd, (2*BLOCK_SIZE + ISIZE*src_inumber), 0);	
		read(fd, &inoderef, 128);
		
		//check if file is directory. If so display information and return.
		if(inoderef.flags & directory)
		{
		printf("The given file name is a directory. A file is required. Please retry.\n");
		return;
		}
		
		//check if file is a plainfile. If so display information and return.
		if((inoderef.flags & plainfile))
		{
		printf("The file name is not a plain file. A plain file is required. Please retry.\n");
		return;
		}
		//check if file is a large file
		if(inoderef.flags & largefile)
		{
		indirect = 1;
		}
		
		total_blocks = (int)ceil(inoderef.size1 / 512.0);
		remaining_bytes = inoderef.size1 % BLOCK_SIZE;
		
		
		//read and write small file to external file
		if(indirect == 0)				//check if it is a small file. indirect = 0 implies the function that makes indirect blocks was not called during cpin.
		{
		printf("file size = %d \n",inoderef.size1);
		
		for(i=0 ; i < total_blocks ; i++)
		{
		read_block_char(reader,inoderef.addr[i]);
		//if counter reaches end of the blocks, write remaining bytes(bytes < 2048) and return.
		if( i == (total_blocks - 1))
		{
		write(targfd, reader, remaining_bytes);
		printf("Contents were transferred to external file\n");
		return;
		}
		write(targfd, reader, BLOCK_SIZE);
		}
		}
		
		
		//read and write large file to external file
		if(indirect == 1)			//check if it is a large file. indirect = 1 implies the function that makes indirect blocks was called during cpin.
		{
		total_blocks = inoderef.size1 / 512;
		indirect_block_chunks = (int)ceil(total_blocks/512.0); 	//each chunk of indirect blocks contain 512 elements that point to data blocks
		remaining_indirectblks = total_blocks%512;
		printf("file size = %d \n",inoderef.size1);
	
		//Loop for chunks of indirect blocks
		for(i=0 ;i < indirect_block_chunks; i++)
		{
		read_block_int(reader1,inoderef.addr[i]);				//store block numbers contained in addr[] array in integer reader array )
		
		//if counter reaches last chunk of indirect blocks, program loops the remaining and exits after writing the remaining bytes
		if(i == (indirect_block_chunks - 1))
		total_blocks = remaining_indirectblks;
		for(j=0; j < 512 && j < total_blocks; j++)
		{
			
			read_block_char(reader,reader1[j]);			//store block contents pointed by addr[] array in character  reader array )
			if((bytes_written = write(targfd, reader, BLOCK_SIZE)) == -1)
			{
			printf("\n Error in writing to external file\n");
			return;
			}
			if( j == (total_blocks - 1))
			{
			write(targfd, reader, remaining_bytes);
			printf("Contents were transferred to external file\n");
			return;
			}
		}
		}
	}
}



int main()
{
int fsinit = 0;
char input[256];
char *parser;
unsigned short n = 0;
char dirbuf[BLOCK_SIZE];
int i =0;
unsigned short bytes_written;
unsigned short number_of_blocks =0, number_of_inodes=0;

printf("Enter command:\n");
while(1)
{

scanf(" %[^\n]s", input);
parser = strtok(input," ");

if(strcmp(parser, "initfs")==0)
{
char *filepath;
char *num1, *num2;
filepath = strtok(NULL, " ");
num1 = strtok(NULL, " ");
num2 = strtok(NULL, " ");
	if(access(filepath, F_OK) != -1)
	{
	if((fd = open(filepath,O_RDWR,0600))== -1)
    {
        printf("\n filesystem already exists but open() failed with error [%s]\n",strerror(errno));
         return 1;
    }
	printf("filesystem already exists and the same will be used.\n");
	fsinit=1;
	}
	else
	{
	if (!num1 || !num2)
	  printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
		else
		{
		number_of_blocks = atoi(num1);
		number_of_inodes = atoi(num2);
		
		if(initialize_fs(filepath,number_of_blocks, number_of_inodes))
		{
		printf("The file system is initialized\n");
		fsinit = 1;
		}
		else
		{
		printf("Error initializing file system. Exiting... \n");
		return 1;
		}
		}
	}
 parser = NULL;
}
	else if(strcmp(parser, "mkdir")==0)
	{
		char *dirname;
	
	 if(fsinit == 0)
	 printf("The file system is not initialized. Please retry after initializing file system\n");
	 else
	 {
	  dirname = strtok(NULL, " ");
	  if(!dirname)
	    printf("No directory name entered. Please retry\n");
	  else
	  {
		int dirinum = allocateinode(); // changed from unsigned int to int -- CK
	  if(dirinum < 0)
		{
		 printf("Error : ran out of inodes \n");
		 return 1;
		}
	  mkdirectory(dirname,dirinum);
	  }
	  }
	   parser = NULL;
	}
	else if(strcmp(parser, "cpin")==0)
	{
		char *targname;
		char *srcname;
	  if(fsinit == 0)
	 printf("The file system is not initialized. Please retry after initializing file system\n");
	  else
	  {

	  srcname = strtok(NULL, " ");
	  targname = strtok(NULL, " ");
	 
	  if(!srcname || !targname )
	    printf("Required file names(source and target file names) have not been entered. Please retry\n");
	  else
	  {
	  	
	   cpin(srcname,targname);
	  }
	  }
	   parser = NULL;
	}
	else if(strcmp(parser, "cpout")==0)
	{
		char *targnameout;
		char *srcnameout;
	 if(fsinit == 0)
	  printf("The file system is not initialized. Please retry after initializing file system\n");
	 else
	  {
	  
	  srcnameout = strtok(NULL, " ");
	  targnameout = strtok(NULL, " ");
	 
	  if(!srcnameout || !srcnameout )
	    printf("Required file names(source and target file names) have not been entered. Please retry\n");
	  else
	  {
	  	
	   cpout(srcnameout,targnameout);
	  }
	  }
	   parser = NULL;
	}
	else if(strcmp(parser, "q")==0)
	{
	
	lseek(fd,BLOCK_SIZE,0);

	 if((bytes_written =write(fd,&super,BLOCK_SIZE)) < BLOCK_SIZE)
	 {
	 printf("\nERROR : error in writing the super block");
	 return 1;
	 } 
	
	return 0;
	}
	else
	{
	printf("\nInvalid command\n ");
	}
	}
}

