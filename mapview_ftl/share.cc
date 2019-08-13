#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <error.h>
#include <unistd.h>
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
#include <scsi/scsi_ioctl.h>
#if 0
#include <scsi/scsi.h>
#include <linux/fiemap.h>
#endif
#include <sys/ioctl.h>
#include <linux/fs.h>

struct sdata {
		int isize;
		int osize;
		unsigned char cmd[16];
};

#define START_SECTOR 2048

/*	[[ogh 
	SWAT: Swap and Trim command protocol 
*/

/* 
Prototype: init_sg_io_hdr(struct sg_io_hdr* io_hdr)
Description : initialize sg_io_hdr structure to be used as argument of swat()
			  io_hdr->cmdp should be connected with buffer for cdb
Return value : none
*/
void init_sg_io_hdr(struct sg_io_hdr* io_hdr, unsigned char* cdb)
{
	// sg_io means "SCSI Genric IO"
	memset(io_hdr, 0 , sizeof(struct sg_io_hdr));
	memset(cdb, 0, sizeof(cdb));
	io_hdr->cmdp = cdb; io_hdr->cmd_len = 16;

	//default setting for sg_io
	io_hdr->interface_id = 'S';
	io_hdr->dxfer_direction = SG_DXFER_NONE;
    
	io_hdr->timeout = 5000;
	
	//default setting for sg_io 
	//and now we use trim command to implement SWAT()
	//We don't send/recieve any buffer to/from disk
	cdb[0] = 0x85;
	cdb[1] = ((3<<1) | 0x01);
	cdb[2] = 0x00;
	//cdb[6] = 0xEF;
	cdb[14] = 0x06;// trim command number 0x06, test 0x40
    //cdb[15] = 0xEF;
}
/*
	Prototype: swat(file_descriptor, sg_io_hdr, old_offset, new_offset)
						(int)		(struct)	(uint64_t)	(uint64_t)
	Return : 0-no error, 1-failed to ioctl
*/

int swat(int fd, struct sg_io_hdr* io_hdr, 
				uint64_t old_offset, uint64_t new_offset)
{
	unsigned char *cdb = (unsigned char*)(io_hdr->cmdp);

	// Offset that will be pointed
	cdb[13] = (old_offset >> 24);
	cdb[12] = (old_offset >> 16);
	cdb[10] = (old_offset >> 8);
	cdb[8] = (old_offset);

	// Offset that will point physical offset of above 
	cdb[3] = (new_offset >> 24);
	cdb[11] = (new_offset >> 16);
	cdb[9] = (new_offset >> 8);
	cdb[7] = (new_offset);

	//error return 1, no error return 0
#if defined(NO_IOCTL)
	return 0;
#elif defined(USE_SCSI)
	sdata s_hdr;

	s_hdr.isize = 0;
	s_hdr.osize = 0;

	memcpy(s_hdr.cmd, cdb, 16);

	if(-1  == ioctl(fd, SCSI_IOCTL_SEND_COMMAND, &s_hdr)  )
	{
			printf("FAIL TO ioctl %d : %s\n", errno, strerror(errno));
			return 1;
	}
	return 0;
#else
    printf("Call swat single %lu to %lu\n", old_offset, new_offset);
    printf("CDB is below\n");
    for(int i = 0 ;i < 16;i++) {
        printf("0x%02x\n", cdb[i]);
    }
	return (-1 == ioctl(fd, SG_IO, io_hdr))? 1 : 0;
#endif
}

#endif




int main(int argc, char *args[]) {
    if (argc < 4) {
        printf("Usage: %s DEVICE old_lpn new_lpn\n", args[0]);
        printf("      After SHARE, read(old_lpn) = data of new_lpn\n");
        printf("      Until writing new data to old_lpn\n");
        return 1;
    }
    int sg_fd = open(args[1], O_RDWR);
    if (sg_fd <= 0) {
        printf("Failed to open devices\n");
        return 2;
    }

    struct sg_io_hdr io_hdr;
    unsigned char cdb[16];
    init_sg_io_hdr(&io_hdr, cdb);
    return swat(sg_fd, &io_hdr, atoll(args[2]), atoll(args[3]));
}
