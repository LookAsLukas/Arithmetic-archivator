#include "coders.h"

uint32_t rate_encode[256];
uint64_t left_border_encode[256], right_border_encode[256], total_encode;

void calc_rate(FILE *in, flags_t flags) {
    int pos = ftell(in);
    uint8_t curr;
    while (fread(&curr, 1, 1, in)) {
        if (curr >= 128 && flags.ascii) {
            continue;
        }
        rate_encode[curr]++;
    }
    fseek(in, pos, SEEK_SET);
}

void write_prerequisites(FILE *out, flags_t flags) {
    int notnullcnt = 0, canbeshort = 1;
    for (int i = 0; i < 256; i++) {
        notnullcnt += rate_encode[i] != 0;
        if (rate_encode[i] >= (1 << 16)) {
            canbeshort = 0;
        }
    }

    if (notnullcnt > 1024 / 5) {
        uint8_t flag = 1 | (canbeshort << 1);
        fwrite(&flag, 1, 1, out);
        for (int i = 0; i < 256 / (flags.ascii + 1); i++) {
            fwrite(rate_encode + i, 4 / (canbeshort + 1), 1, out);
        }
        return;
    }

    uint8_t flag = 0 | (canbeshort << 1);
    fwrite(&flag, 1, 1, out);
    fwrite(&notnullcnt, 4, 1, out);
    for (int i = 0; i < 256; i++) {
        if (!rate_encode[i]) {
            continue;
        }
        uint8_t buf = i;
        fwrite(&buf, 1, 1, out);
        fwrite(rate_encode + i, 4 / (canbeshort + 1), 1, out);
    }
}

void encode_(FILE *in, FILE *out, flags_t flags) {
    calc_rate(in, flags);
    fwrite(&flags.ascii, 1, 1, out);
    write_prerequisites(out, flags);
    
    right_border_encode[0] = rate_encode[0];
    for (int i = 1; i < 256; i++) {
        left_border_encode[i] = left_border_encode[i - 1] + rate_encode[i - 1];
        right_border_encode[i] = left_border_encode[i] + rate_encode[i];
    }
    total_encode = right_border_encode[255];

    if (!total_encode) {
        return;
    }

    uint8_t curr;
    uint64_t left = 0, right = WHOLE, message = 0;
    while (fread(&curr, 1, 1, in)) {
        if (curr >= 128 && flags.ascii) {
            continue;
        }

        uint64_t width = right - left;
        right = left + width * right_border_encode[curr] / total_encode;
        left = left + width * left_border_encode[curr] / total_encode;

        while (right < HALF || left > HALF) {
            if (right < HALF) {
                emit(out, message, 0);
                message = 0;
                left <<= 1;
                right <<= 1;
            } else if (left > HALF) {
                emit(out, message, 1);
                message = 0;
                left = 2 * (left - HALF);
                right = 2 * (right - HALF);
            }
        }

        while (left > QUARTER && right < 3 * QUARTER) {
            message++;
            left = 2 * (left - QUARTER);
            right = 2 * (right - QUARTER);
        }
    }

    message++;
    curr = left <= QUARTER ? emit(out, message, 0) : emit(out, message, 1);
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
            rate_encode[i] = 0;
            left_border_encode[i] = 0;
            right_border_encode[i] = 0;
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