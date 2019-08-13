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
void init_sg_io_hdr(struct sg_io_hdr* io_hdr, unsigned char* cdb,
                    unsigned char *buf)
{
	// sg_io means "SCSI Genric IO"
	memset(io_hdr, 0 , sizeof(struct sg_io_hdr));
	memset(cdb, 0, sizeof(unsigned char) * 16);
    memset(buf, 0, sizeof(unsigned char) * 4096);
	io_hdr->cmdp = cdb; io_hdr->cmd_len = sizeof(unsigned char) * 16;

	//default setting for sg_io
	io_hdr->interface_id = 'S';
	io_hdr->dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr->dxfer_len = 4096;
    io_hdr->dxferp = buf;
    
	io_hdr->timeout = 5000;
	
	//default setting for sg_io 
	//and now we use trim command to implement SWAT()
	//We don't send/recieve any buffer to/from disk
	cdb[0] = 0x85;
	cdb[1] = ((5<<1) | 0x01);
	cdb[2] = 0x06; //transfer data as a block(sector) specified in sector fields
    cdb[6] = 8;
	//cdb[6] = 0xEF;
    cdb[13] = 0x01;
	cdb[14] = 0x35;// trim command number 0x06, test 0x40
    //cdb[15] = 0xEF;
}
/*
	Prototype: swat(file_descriptor, sg_io_hdr, old_offset, new_offset)
						(int)		(struct)	(uint64_t)	(uint64_t)
	Return : 0-no error, 1-failed to ioctl
*/

int swat(int fd, struct sg_io_hdr* io_hdr, 
				uint32_t old_offset, uint32_t new_offset)
{
	unsigned char *cdb = (unsigned char*)(io_hdr->cmdp);

#if 0 //for reference
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
#endif
    uint32_t num = 127;
    memcpy(io_hdr->dxferp, &num, sizeof(uint32_t));
    for (int i = 0 ; i < num; i ++) {
        memcpy( (char*)(io_hdr->dxferp) + 4 + (i * 8), &old_offset, sizeof(uint32_t));
        memcpy( (char*)(io_hdr->dxferp) + 8 + (i * 8), &new_offset, sizeof(uint32_t));
    }

printf("Call share\n");
	//return (-1 == ioctl(fd, SG_IO, io_hdr))? 1 : 0;
}

#endif




int main(int argc, char *args[]) {
    if (argc < 4) {
        printf("Usage: %s DEVICE old_lpn new_lpn\n", args[0]);
        printf("      After SHARE, read(old_lpn) = data of new_lpn\n");
        printf("      Until writing new data to old_lpn\n");
        return 1;
    }
    int sg_fd = open(args[1],  O_RDWR);
    if (sg_fd <= 0) {
        printf("Failed to open devices\n");
        return 2;
    }

    struct sg_io_hdr io_hdr;
    unsigned char cdb[16];
    void *buf;
    posix_memalign(&buf, 4096, 4096);
    init_sg_io_hdr(&io_hdr, cdb, (unsigned char*)buf);
    cdb[13] = 0x01;
	for (int i=0;i < 11486;i++) {
		printf("idx: %d\n", i);
		swat(sg_fd, &io_hdr, atoi(args[2]), atoi(args[3]));
	}
    return 0;
}
