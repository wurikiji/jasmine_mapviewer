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

	io_hdr.cmdp = cdb;  // assign command block to io header
	io_hdr.cmd_len = sizeof(unsigned char) * 16;

	//default setting for sg_io
	io_hdr.interface_id = 'S'; // fixed character, do not change 
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV; // flag to transfer data from host to device
	io_hdr.timeout = 5000; 	// max timeout 
	
	//default setting for sg_io 
	/* do not change this */
	cdb[0] = 0x85; // default to use 'ATA_PASSTHROUGH' command 
	cdb[1] = ((5<<1) | 0x01); // sending flag 
	cdb[2] = 0x07; //transfer data in as a block(sector) specified in sector fields
	cdb[3] = 0x01;
	cdb[14] = 0x25;// trim command number 0x06, write DMA EXT 0x35
    return 0;
}

void dispose_mapview() {
    close(fd);
}

int get_map(int start_sector, int sector_count)
{
    int ret = 0;
	int lba_high = ((start_sector >> 16) & 0x0000ffff);
	int lba_low = (start_sector & 0x0000ffff);
	int requested_bytes = sector_count * 4;
	int i = 0 ;
	char *result;

	if (sector_count > 15) {
		printf("Sector count should be lower than 15");
		return 15;
	}
    buf = malloc(sizeof(char) * requested_bytes);
	result = buf;
    io_hdr.dxferp = buf; 	// data buffer 

    if (buf <= 0) {
        perror("Error while allocating memory\n");
        return errno;
    }

    io_hdr.dxfer_len = requested_bytes; // transferred data length in bytes 

	// cdb[4] = sector_count;
	// cdb[5] = sector_count;
	cdb[6] = (requested_bytes + 511) / 512;
	cdb[8] = (lba_low);
	cdb[10] = (lba_low >> 8);
	cdb[12] = lba_high;
	cdb[13] = sector_count;

	ret = ioctl(fd, SG_IO, &io_hdr);
    if (ret < 0) {
        perror("IOCTL ERROR: errno \n");
		return errno;
    }

	for(i =0 ;i < sector_count;i++) {
		printf("%d: %d\n", start_sector + i, *(int*)result);
		result += 4;
	}

	free(buf);

    return 0;
}
