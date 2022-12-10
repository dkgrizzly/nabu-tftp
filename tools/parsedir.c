#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv) {
    uint8_t header[128];
    uint8_t entry[128];
    int extra = 0;
    uint16_t entries;

    fread(header, 1, 1, stdin);

    if(header[0] < 8) {
        fprintf(stderr, "ERROR: Segment header is too short\r\n");
        return 0;
    }

    extra = header[0] - 0x0e;

    fread(header+1, 1, header[0]-1, stdin);

    if(header[1] != 0x00) {
        fprintf(stderr, "ERROR: Segment type is wrong\r\n");
        return 0;
    }

#if 0
    if(header[7] != (header[0] - 14)) {
        fprintf(stderr, "ERROR: Segment header length and name length don't match\r\n");
        return 0;
    }
#endif

    printf("Directory Name: ");
    fwrite(header+8+extra, 1, header[7+extra], stdout);
    printf("\r\nCode-to-Load: %02x%02x%02x\r\n", header[4+extra], header[5+extra], header[6+extra]);
    entries = extra ? ((header[4] << 8)|header[3]) : header[3];
    printf("Directory Entries: %d\r\n\r\n", entries);

    printf(" Name                Type  Owner  Tier      Segment\r\n");
    printf("----------------------------------------------------\r\n");
    for(int i = 0; i < entries; i++) {
        fread(entry, 1, header[2], stdin);
        printf(" ");
        fwrite(entry+9, 1, 18, stdout);
        printf("  %02x ", entry[0]);
        printf("   %02x ", entry[1]);
        printf("    %02x%02x%02x%02x", entry[2], entry[3], entry[4], entry[5]);
        printf("  %02x%02x%02x", entry[6], entry[7], entry[8]);
        printf("\r\n");
    }

    return 0;
}

