#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv) {
    uint8_t header[128];
    uint8_t entry[128];

    fread(header, 1, 1, stdin);

    if(header[0] < 8) {
        fprintf(stderr, "ERROR: Segment header is too short\r\n");
        return 0;
    }

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
    fwrite(header+8, 1, header[7], stdout);
    printf("\r\nCode-to-Load: %02x%02x%02x\r\n", header[4], header[5], header[6]);
    printf("Directory Entries: %d\r\n\r\n", header[3]);

    printf(" Name                Type  Owner  Tier      Segment\r\n");
    printf("----------------------------------------------------\r\n");
    for(int i = 0; i < header[3]; i++) {
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

