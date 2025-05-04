#include "coders.h"

uint32_t rate_decode[256];
uint64_t left_border_decode[256], right_border_decode[256], total_decode;

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

uint32_t get_decode_sample(FILE *in) {
    uint32_t res = 0;
    for (int i = 0; i < 32; i++) {
        res |= get_next_bit(in) << (31 - i);
        if (errorflag) {
            return 0;
        }
    }
    return res;
}

void read_prerequisites(FILE *in, int ascii) {
    uint8_t f;
    if (!fread(&f, 1, 1, in)) {
        errorflag = 2;
        return;
    }
    int isshort = (f >> 1) & 1;
    f = f & 1;

    if (f) {
        for (int i = 0; i < 256 / (ascii + 1); i++) {
            if (!fread(rate_decode + i, 4 / (isshort + 1), 1, in)) {
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
        if (!fread(rate_decode + i, 4 / (isshort + 1), 1, in)) {
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
    read_prerequisites(in, ascii);
    if (errorflag) {
        return;
    }
    right_border_decode[0] = rate_decode[0];
    for (int i = 1; i < 256; i++) {
        left_border_decode[i] = left_border_decode[i - 1] + rate_decode[i - 1];
        right_border_decode[i] = left_border_decode[i] + rate_decode[i];
    }
    total_decode = right_border_decode[255];

    if (!total_decode) {
        emptyflag = 8;
        return;
    }

    uint64_t left = 0, right = WHOLE, outputted_cnt = 0;
    uint64_t decode_sample = get_decode_sample(in);
    if (errorflag) {
        return;
    }
    while (1) {
        for (int i = 0; i < 256; i++) {
            uint64_t width = right - left;
            uint64_t right_candidate = left + (width * right_border_decode[i]) / total_decode;
            uint64_t left_candidate = left + (width * left_border_decode[i]) / total_decode;
            
            if (left_candidate <= decode_sample && decode_sample < right_candidate) {
                uint8_t output_byte = i;
                fwrite(&output_byte, 1, 1, out);
                outputted_cnt++;
                left = left_candidate;
                right = right_candidate;
                if (outputted_cnt == total_decode) {
                    return;
                }
                break;
            }
        }

        while (right < HALF || left > HALF) {
            if (right < HALF) {
                left <<= 1;
                right <<= 1;
                decode_sample <<= 1;
            } else if (left > HALF) {
                left = 2 * (left - HALF);
                right = 2 * (right - HALF);
                decode_sample = 2 * (decode_sample - HALF);
            }

            decode_sample |= get_next_bit(in);
            if (errorflag) {
                return;
            }
        }

        while (left > QUARTER && right < 3 * QUARTER) {
            left = 2 * (left - QUARTER);
            right = 2 * (right - QUARTER);
            decode_sample = 2 * (decode_sample - QUARTER);

            decode_sample |= get_next_bit(in);
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
            rate_decode[i] = 0;
            left_border_decode[i] = 0;
            right_border_decode[i] = 0;
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