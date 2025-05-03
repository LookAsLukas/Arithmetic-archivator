#include "coders.h"

uint32_t re[256];
uint64_t ce[256], de[256], Re;

void read_r(FILE *in, flags_t flags) {
    int pos = ftell(in);
    uint8_t curr;
    while (fread(&curr, 1, 1, in)) {
        if (curr >= 128 && flags.ascii) {
            continue;
        }
        re[curr]++;
    }
    fseek(in, pos, SEEK_SET);
}

void write_r(FILE *out, flags_t flags) {
    int notnullcnt = 0, canbeshort = 1;
    for (int i = 0; i < 256; i++) {
        notnullcnt += re[i] != 0;
        if (re[i] >= (1 << 16)) {
            canbeshort = 0;
        }
    }

    if (notnullcnt > 1024 / 5) {
        uint8_t f = 1 | (canbeshort << 1);
        fwrite(&f, 1, 1, out);
        for (int i = 0; i < 256 / (flags.ascii + 1); i++) {
            fwrite(re + i, 4 / (canbeshort + 1), 1, out);
        }
        return;
    }

    uint8_t f = 0 | (canbeshort << 1);
    fwrite(&f, 1, 1, out);
    fwrite(&notnullcnt, 4, 1, out);
    for (int i = 0; i < 256; i++) {
        if (!re[i]) {
            continue;
        }
        uint8_t buf = i;
        fwrite(&buf, 1, 1, out);
        fwrite(re + i, 4 / (canbeshort + 1), 1, out);
    }
}

void encode_(FILE *in, FILE *out, flags_t flags) {
    read_r(in, flags);
    fwrite(&flags.ascii, 1, 1, out);
    write_r(out, flags);
    
    de[0] = re[0];
    for (int i = 1; i < 256; i++) {
        ce[i] = ce[i - 1] + re[i - 1];
        de[i] = ce[i] + re[i];
    }
    Re = de[255];

    uint8_t curr;
    uint64_t a = 0, b = WHOLE, s = 0;
    while (fread(&curr, 1, 1, in)) {
        if (curr >= 128 && flags.ascii) {
            continue;
        }

        uint64_t w = b - a;
        b = a + w * de[curr] / Re;
        a = a + w * ce[curr] / Re;

        while (b < HALF || a > HALF) {
            if (b < HALF) {
                emit(out, s, 0);
                s = 0;
                a <<= 1;
                b <<= 1;
            } else if (a > HALF) {
                emit(out, s, 1);
                s = 0;
                a = 2 * (a - HALF);
                b = 2 * (b - HALF);
            }
        }

        while (a > QUARTER && b < 3 * QUARTER) {
            s++;
            a = 2 * (a - QUARTER);
            b = 2 * (b - QUARTER);
        }
    }

    s++;
    // printf("s: %d\n", s);
    curr = a <= QUARTER ? emit(out, s, 0) : emit(out, s, 1);
    // bits(curr);
    fwrite(&curr, 1, 1, out);
}

void encode(char **filenames, int filecnt, char *archivename, flags_t flags) {
    FILE *out = fopen(archivename, "wb");
    if (!out) {
        printf("ERROR: No such directory: ");
        print_directory(archivename);
        printf("\n");
        return;
    }

    fwrite(&filecnt, 4, 1, out);
    int failcnt = 0;
    double totalsizein = 0, totalsizeout = 0;
    for (int i = 0; i < filecnt; i++) {
        FILE *in = fopen(filenames[i], "rb");
        if (!in) {
            printf("WARNING: Skipping %s: No such file\n", filenames[i]);
            failcnt++;
            continue;
        }
        printf("Encoding %s...\n", filenames[i]);
        
        emit(NULL, 0, 0);
        for (int i = 0; i < 256; i++) {
            re[i] = 0;
            ce[i] = 0;
            de[i] = 0;
        }
        encode_(in, out, flags);
        totalsizein += ftell(in);
        fclose(in);
        printf("Successfully encoded %s!\n", filenames[i]);
    }
    totalsizeout = ftell(out);

    if (failcnt) {
        printf("Failed to encode %d file(s)\n", failcnt);
        fseek(out, 0, SEEK_SET);
        filecnt -= failcnt;
        fwrite(&filecnt, 4, 1, out);
    }

    printf("Successfully encoded into %s!\n", archivename);
    printf("Total input size: ");
    print_size(totalsizein);
    printf("\nOutput size: ");
    print_size(totalsizeout);
    printf("\nCompression rate: %.1lf%%\n", totalsizeout / totalsizein * 100);
    fclose(out);
}