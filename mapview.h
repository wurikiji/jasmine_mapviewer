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


#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

extern int max_share;

// dev should be /dev/sdx. 'x' can be a, b, c, d, ..., etc. (e.g] /dev/sda)
int init_mapview(const char* dev, int page_size);

void dispose_mapview() ;

//int share_add_pair(uint32_t old_offset, uint32_t new_offset) ;

int get_map(int start_sector, int sector_count);
