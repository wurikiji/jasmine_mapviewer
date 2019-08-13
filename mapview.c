#include <errno.h>
#include "mapview.h"

// SCSI generic IO header 
static struct sg_io_hdr io_hdr;
static int fd = 0;

// SCSI command block 
static unsigned char cdb[16];

#define DEFAULT_PAGE_SIZE (32 * 1024)
static void *buf;
static int jasmine_page_size;

// sg_io means "SCSI Genric IO"
int init_mapview(const char* dev, int page_size)
{
    int ret;

	if (page_size == 0) {
		page_size = DEFAULT_PAGE_SIZE;
	}
	jasmine_page_size = page_size;

	// OPEN DEVICE to be sent ioctl Command
    fd = open(dev, O_RDWR);

    if (fd < 0) {
        perror("Unable to open\n");
        return errno;
    }
	
	// to use O_DIRECT, O_DIRECT is not necessary 
    // ret = posix_memalign(&buf, 4096, 4096);
    buf = malloc(sizeof(char) * page_size);

    if (buf <= 0) {
        perror("Error while allocating memory\n");
        return errno;
    }


	// INIT 
	memset(&io_hdr, 0 , sizeof(struct sg_io_hdr));
	memset(cdb, 0, sizeof(unsigned char) * 16);
    memset(buf, 0, sizeof(unsigned char) * page_size);

	io_hdr.cmdp = cdb;  // assign command block to io header
	io_hdr.cmd_len = sizeof(unsigned char) * 16;

	//default setting for sg_io
	io_hdr.interface_id = 'S'; // fixed character, do not change 
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV; // flag to transfer data from host to device
    io_hdr.dxfer_len = page_size; // transferred data length in bytes 
    io_hdr.dxferp = buf; 	// data buffer 
	io_hdr.timeout = 5000; 	// max timeout 
	
	//default setting for sg_io 
	/* do not change this */
	cdb[0] = 0x85; // default to use 'ATA_PASSTHROUGH' command 
	cdb[1] = ((5<<1) | 0x01); // sending flag 
	cdb[2] = 0x07; //transfer data in as a block(sector) specified in sector fields
	cdb[3] = 0x1;
	cdb[6] = page_size / 512;
	cdb[14] = 0x25;// trim command number 0x06, write DMA EXT 0x35
    return 0;
}

void dispose_mapview() {
    close(fd);
    free(buf);
}

int get_map(int start_sector, int sector_count)
{
    int ret = 0;
	int max_record_count = (jasmine_page_size / sizeof(int));
	// int max_record_count = 15;
	int lba_low = ((start_sector >> 16) & 0x0000ffff);
	int lba_mid = (start_sector & 0x0000ffff);
	char *result = buf;
	int i = 0 ;
	if (sector_count > max_record_count) { 
		printf("sector_count argument should be lower than or equal to %d\n", 
					max_record_count);
		return -1;
	}

	cdb[4] = sector_count;
	cdb[8] = (lba_low);
	cdb[9] = (lba_low >> 8);
	cdb[10] = lba_mid;
	cdb[11] = (lba_mid >> 8);
	cdb[13] = 0x1;

	ret = ioctl(fd, SG_IO, &io_hdr);
    if (ret < 0) {
        perror("IOCTL ERROR: errno \n");
		return errno;
    }

	for(i =0 ;i < sector_count;i++) {
		printf("%d: %d\n", start_sector + i, *(int*)result);
		result += 4;
	}

    return ret;
}
