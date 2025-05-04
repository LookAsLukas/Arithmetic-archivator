#include "coders.h"
#include <math.h>

uint32_t r[256];
uint64_t c[256], d[256], R;

int errorflag = 0, emptyflag = 0, lastfile = 0;

int get_next_bit(FILE *in) {
    static uint8_t curr = 0;
    static int curr_cnt = 0;

    if (!in) {
        curr = 0;
        curr_cnt = 0;
        return 0;
    }
    
    if (!curr_cnt) {
        if (!fread(&curr, 1, 1, in) && !lastfile) {
            errorflag = 1;
            return (curr >> 7) & 1;
        }
        curr_cnt = 8;
    }

    return (curr >> (--curr_cnt)) & 1;
}

uint32_t read_z(FILE *in) {
    uint32_t res = 0;
    for (int i = 0; i < 32; i++) {
        res |= get_next_bit(in) << (31 - i);
        if (errorflag) {
            return 0;
        }
    }
    return res;
}

void get_r(FILE *in, int ascii) {
    uint8_t f;
    if (!fread(&f, 1, 1, in)) {
        errorflag = 2;
        return;
    }
    int isshort = (f >> 1) & 1;
    f = f & 1;

    if (f) {
        for (int i = 0; i < 256 / (ascii + 1); i++) {
            if (!fread(r + i, 4 / (isshort + 1), 1, in)) {
                errorflag = 3;
                return;
            }
        }
        return;
    }

    int rcnt;
    if (!fread(&rcnt, 4, 1, in)) {
        errorflag = 4;
        return;
    }
    
    while (rcnt--) {
        uint8_t i;
        if (!fread(&i, 1, 1, in)) {
            errorflag = 5;
            return;
        }
        if (!fread(r + i, 4 / (isshort + 1), 1, in)) {
            errorflag = 6;
            return;
        }
    }
}

void decode_(FILE *in, FILE *out) {
    char ascii;
    if (!fread(&ascii, 1, 1, in)) {
        errorflag = 7;
        return;
    }
    get_r(in, ascii);
    if (errorflag) {
        return;
    }
    d[0] = r[0];
    for (int i = 1; i < 256; i++) {
        c[i] = c[i - 1] + r[i - 1];
        d[i] = c[i] + r[i];
    }
    R = d[255];

    if (!R) {
        emptyflag = 8;
        return;
    }

    // for (int i = 0; i < 80; i++) {
    //     printf("%d", get_next_bit(in));
    //     if (i % 8 == 7) {
    //         printf("");
    //     }
    // }
    // printf("\n");
    // return;

    uint64_t a = 0, b = WHOLE, cnt = 0;
    uint64_t z = read_z(in);
    if (errorflag) {
        return;
    }
    while (1) {
        // printf("Z: %lu\n", z);
        for (int i = 0; i < 256; i++) {
            uint64_t w = b - a, b0 = a + (w * d[i]) / R, a0 = a + (w * c[i]) / R;
            if (a0 <= z && z < b0) {
                // printf("a0 b0: %lu %lu ||| %d\n", a0, b0, i);
                uint8_t lol = i;
                fwrite(&lol, 1, 1, out);
                cnt++;
                a = a0;
                b = b0;
                if (cnt == R) {
                    return;
                }
                break;
            }
        }

        while (b < HALF || a > HALF) {
            if (b < HALF) {
                a <<= 1;
                b <<= 1;
                z <<= 1;
            } else if (a > HALF) {
                a = 2 * (a - HALF);
                b = 2 * (b - HALF);
                z = 2 * (z - HALF);
            }

            z |= get_next_bit(in);
            if (errorflag) {
                return;
            }
        }

        while (a > QUARTER && b < 3 * QUARTER) {
            a = 2 * (a - QUARTER);
            b = 2 * (b - QUARTER);
            z = 2 * (z - QUARTER);

            z |= get_next_bit(in);
            if (errorflag) {
                return;
            }
        }
    }
}

void decode(char *archivename, char **filenames, int namecnt, char *directory) {
    FILE *in = fopen(archivename, "rb");
    if (!in) {
        printf("ERROR: No such file: %s\n", archivename);
        return;
    }

    int filecnt;
    fread(&filecnt, 4, 1, in);
    for (int i = 0; i < filecnt; i++) {
        FILE *out;
        char *final_name = filenames[i];
        int freefinal = 0;
        if (i < namecnt) {
            out = fopen(filenames[i], "wb");
            if (!out) {
                printf("WARNING: No such directory: ");
                char *name = print_directory(filenames[i]);
                printf(". Output file name is %s\n", name);
                out = fopen(name, "wb");
                final_name = name;
            }
        } else {
            char name[30] = {0};
            sprintf(name, "file%d", i - namecnt);
            if (directory[0] == '.' && directory[1] == '\0') {
                final_name = name;
            } else {
                char *buf = malloc(strlen(name) + strlen(directory) + 5);
                freefinal = 1;
                buf[0] = 0;
                strcat(buf, directory);
                strcat(buf, "/");
                strcat(buf, name);
                final_name = buf;
            }
            out = fopen(final_name, "wb");
        }
        printf("Decoding into %s...\n", final_name);
        
        get_next_bit(NULL);
        for (int i = 0; i < 256; i++) {
            r[i] = 0;
            c[i] = 0;
            d[i] = 0;
        }
        emptyflag = 0;
        lastfile = i == filecnt - 1;
        decode_(in, out);
        if (errorflag) {
            printf("Aborting: incorrect file format\n");
            if (freefinal) {
                free(final_name);
            }
            fclose(out);
            fclose(in);
            return;
        }
        fclose(out);
        if (!emptyflag) {
            fseek(in, -3, SEEK_CUR);
        }
        printf("Successfully decoded into %s...\n", final_name);
        if (freefinal) {
            free(final_name);
        }
    }
    printf("Successfully decoded from %s!\n", archivename);
    fclose(in);
}