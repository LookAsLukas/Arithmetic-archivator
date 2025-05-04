#include "io.h"

void bits(uint8_t n) {
    for (int i = 0; i < 8; i++) {
        printf("%d", (n >> (7 - i)) & 1);
    }
    printf("\n");
}

uint8_t emit(FILE *out, uint64_t s, int mode) {
    static uint8_t curr = 0;
    static int curr_cnt = 0;

    if (!out) {
        curr = 0;
        curr_cnt = 0;
        return 0;
    }

    curr |= mode << (7 - curr_cnt);
    if (++curr_cnt == 8) {
        fwrite(&curr, 1, 1, out);
        curr = 0;
        curr_cnt = 0;
    }
    mode = !mode;

    while (s--) {
        curr |= mode << (7 - curr_cnt);
        if (++curr_cnt == 8) {
            fwrite(&curr, 1, 1, out);
            curr = 0;
            curr_cnt = 0;
        }
    }

    return curr;
}

void print_options() {
    printf("Usage: ari [options] file file1 file2 ...\nOptions:\n");
    char *options[] = {
        "--help (-h)",
        "--ascii-only (-a)",
        "-output=<name>",
        "-e",
        "-d"
    };
    char *descriptions[] = {
        "Display this information",
        "Encode only ascii characters",
        "Set output file name for encoding or output directory for decoding (default: archive.ari and current directory)",
        "Encode mode (default)",
        "Decode mode"
    };
    for (int i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        printf("  %-20s  %s\n", options[i], descriptions[i]);
    }
    printf("\nEncode mode:\n  Encodes files <file>, <file1>, <file2> ...\n  Archive appears in the current directory\n  and has optionally specified name\n");
    printf("Decode mode:\n  Decodes archive <file> into files <file1> <file2> ...\n  If there are more files in the archive than names given,\n  all unspecified names get assigned a name of the form [file{n}],\n  where n is the index of current file\n  Files appear in the optionally specified directory\n");
}

char* print_directory(char *path) {
    char *save = path;
    for (; *path != '\0'; path++) {
        if (*path == '/') {
            for (; save != path; save++) {
                printf("%c", *save);
            }
        }
    }
    return save + 1;
}

void print_size(double size) {
    int ind = 0;
    while (ind < 4 && floor(log10(size)) + 1 > 3) {
        size /= 1000;
        ind++;
    }

    char *sizes[] = {
        "B",
        "KB",
        "MB",
        "GB"
    };
    printf("%.1lf %s", size, sizes[ind]);
}