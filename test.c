#include "mapview.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int start_sector;
    int sector_count;

    if (argc < 4) {
        printf("Usage: %s [dev] [src_lsn] [# of sectors]\n", argv[0]);
        return 1;
    }
    init_mapview(argv[1], 4096);
    start_sector = atoi(argv[2]);
    sector_count = atoi(argv[3]);

    get_map(start_sector, sector_count);
    return 0;
}
