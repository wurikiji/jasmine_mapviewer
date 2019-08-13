/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#if defined(USE_LIBAIO) || defined(LIBAIO_TEST)
#include <libaio.h>
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <error.h>
#include <time.h>
#if !defined(WIN32) && !defined(_WIN32)
#include <sys/time.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


#if !defined(__APPLE__)
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <linux/fiemap.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

struct sdata {
		int isize;
		int osize;
		unsigned char cmd[16];
};

#define START_SECTOR 2048


int sg_fd = 0;
int sg_count= 0;

uint64_t get_filesize_(char *filename)
{
	struct stat filestat;
	stat(filename, &filestat);
	return filestat.st_size;
}
/*	[[ogh 
	SWAT: Swap and Trim command protocol 
*/

/* 
Prototype: init_sg_io_hdr(struct sg_io_hdr* io_hdr)
Description : initialize sg_io_hdr structure to be used as argument of swat()
			  io_hdr->cmdp should be connected with buffer for cdb
Calling : After io_hdr.cmdp = cdb; io_hdr.cmd_len=sizeof(cdb); in caller function, 
		  init_sg_io_hdr(&io_hdr);
Return value : none
*/
void init_sg_io_hdr(struct sg_io_hdr* io_hdr, unsigned char* cdb)
{
	// sg_io means "SCSI Genric IO"
	memset(cdb, 0, sizeof(cdb));
	memset(io_hdr, 0 , sizeof(struct sg_io_hdr));
	io_hdr->cmdp = cdb; io_hdr->cmd_len = sizeof(cdb);

	//default setting for sg_io
	io_hdr->interface_id = 'S';
	io_hdr->dxfer_direction = SG_DXFER_NONE;
	io_hdr->timeout = 500;
	
	//default setting for sg_io 
	//and now we use trim command to implement SWAT()
	//We don't send/recieve any buffer to/from disk, 
	//and we don't get any sg_status info.
	cdb[0] = 0x85;
	cdb[1] = ((3<<1) | 0x01);
	cdb[2] = 0x00;
	cdb[6] = 0xEF;
	cdb[14] = 0x06; // trim command number 0x06, test 0x40
}
/*
	Prototype: swat(file_descriptor, sg_io_hdr, old_offset, new_offset)
						(int)		(struct)	(uint64_t)	(uint64_t)
	Description :
		old_offset means "offset that will be pointed"
		new_offset means "offset that will point physical offset of old_offset"
	Calling :
		In the fdb_set() like function, 
			swat(fd, &io_hdr, new_doc's offset, old_doc's offset);
			because "lpn of old_offset will remain."
		In the fdb_compact() like function,
			swat(fd, &io_hdr, old_file_doc's offset, new_file_doc's offset);
			because "lpn of new_file will remain."
	Return : 0-no error, 1-failed to ioctl
*/

int swat(int fd, struct sg_io_hdr* io_hdr, 
				uint64_t old_offset, uint64_t new_offset)
{
	unsigned char *cdb = (unsigned char*)(io_hdr->cmdp);
	sdata s_hdr;

	s_hdr.isize = 0;
	s_hdr.osize = 0;

	// Offset that will be pointed
	cdb[13] = (old_offset >> 24);
	cdb[12] = (old_offset >> 16);
	cdb[10] = (old_offset >> 8);
	cdb[8] = (old_offset);

	// Offset that will point physical offset of above 
	cdb[3] = (new_offset >> 24);
	cdb[11] = (new_offset > 16);
	cdb[9] = (new_offset >> 8);
	cdb[7] = (new_offset);

	//error return 1, no error return 0
#if defined(NO_IOCTL)
	return 0;
#elif defined(USE_SCSI)

	memcpy(s_hdr.cmd, cdb, 16);

	if(-1  == ioctl(fd, SCSI_IOCTL_SEND_COMMAND, &s_hdr)  )
	{
			printf("FAIL TO ioctl %d : %s\n", errno, strerror(errno));
			return 1;
	}
	return 0;
#else
	return (-1 == ioctl(fd, SG_IO, io_hdr))? 1 : 0;
#endif
}

struct fiemap *read_fiemap(int fd)
{
	struct fiemap *fiemap;
	int extents_size;

	if ((fiemap = (struct fiemap*)malloc(sizeof(struct fiemap))) == NULL) {
		fprintf(stderr, "Out of memory allocating fiemap\n");	
		return NULL;
	}
	memset(fiemap, 0, sizeof(struct fiemap));

	fiemap->fm_start = 0;
	fiemap->fm_length = ~0;		/* Lazy */
	fiemap->fm_flags = 0;
	fiemap->fm_extent_count = 0;
	fiemap->fm_mapped_extents = 0;

	/* Find out how many extents there are */
	if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "fiemap ioctl() failed\n");
		return NULL;
	}

	/* Read in the extents */
	extents_size = sizeof(struct fiemap_extent) * 
		(fiemap->fm_mapped_extents);

	/* Resize fiemap to allow us to read in the extents */
	if ((fiemap = (struct fiemap*)realloc(fiemap,sizeof(struct fiemap) + 
					extents_size)) == NULL) {
		fprintf(stderr, "Out of memory allocating fiemap\n");	
		return NULL;
	}

	memset(fiemap->fm_extents, 0, extents_size);
	fiemap->fm_extent_count = fiemap->fm_mapped_extents;
	fiemap->fm_mapped_extents = 0;

	if (ioctl(fd, FS_IOC_FIEMAP, fiemap) < 0) {
		fprintf(stderr, "fiemap ioctl() failed\n");
		return NULL;
	}

	return fiemap;
}
void get_lsn(char* file_name, uint64_t *arr, uint64_t num)
{
	int i,j;
	uint32_t ret;
	int block;
	memset(arr,0,sizeof(arr));
	int fd = open (file_name, O_RDWR);
	struct fiemap *fiemap = read_fiemap(fd);
	int count = 0;
	for(i = 0 ; i < fiemap -> fm_mapped_extents;i++)
	{
		for(j =0 ;j < fiemap->fm_extents[i].fe_length/4096;j++)
		{
			if( count >= num ) {
				close(fd);
				return;
			}
			arr[count] = (fiemap->fm_extents[i].fe_physical/512 + 2048)/8 + j;
			count++;
		}
	}
	close(fd);
}

uint64_t get_single_lsn(int fd, uint64_t offset)
{
	int i;
	int block;
	uint32_t ret ;
	//ioctl(fd, FIGETBSZ, &block);
	ret = offset / 4096; // bid 
	if( ioctl(fd, FIBMAP, &ret) ){
		printf("Error while getting lpn\n");
	}
	
	//return (uint64_t)ret * ( block / 512) + START_SECTOR; // return a logical sector number
	return (uint64_t)ret + START_SECTOR / 8 ; //return a logical page number
}
//ogh]]
#endif




int main(int argc, char *args[]) {
    if (argc < 3) {
    }
    return 0;
}
