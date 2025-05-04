#include "coders.h"

int err = 0;

char* read_output_flag(char *arg) {
    char *temp = "-output=", *p;
    for (p = arg; *p != '\0' && *temp != '\0'; p++, temp++) {
        if (*p != *temp) {
            return NULL;
        }
    }
    if (*temp != '\0') {
        return NULL;
    }
    if (*p == '\0') {
        printf("ERROR: empty output name\n");
        err = 1;
    }

    return p;
}

void encode_handle(int argc, char **argv) {
    flags_t flags = {0};
    char *name = "archive.ari";
    char **filenames = malloc(sizeof(char*) * argc);
    int ind = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "-e")) {
            continue;
        }
        if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--ascii-only")) {
            flags.ascii = 1;
            continue;
        }
        char *buf;
        if (buf = read_output_flag(argv[i])) {
            if (err) {
                free(filenames);
                exit(1);
            }
            name = buf;
            continue;
        }
        filenames[ind++] = argv[i];
    }

    encode(filenames, ind, name, flags);

    free(filenames);
}

void decode_handle(int argc, char **argv) {
    flags_t flags = {0};
    char *directory = ".";
    char **filenames = malloc(sizeof(char*) * argc);
    int ind = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "-e")) {
            continue;
        }
        if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--ascii-only")) {
            continue;
        }
        char *buf;
        if (buf = read_output_flag(argv[i])) {
            directory = buf;
            continue;
        }
        filenames[ind++] = argv[i];
    }

    for (int i = 1; i < ind; i++) {
        char *buf = malloc(strlen(filenames[i]) + strlen(directory) + 5);
        buf[0] = 0;
        strcat(buf, directory);
        strcat(buf, "/");
        strcat(buf, filenames[i]);
        filenames[i] = buf;
    }

    decode(filenames[0], filenames + 1, ind - 1, directory);

    
    for (int i = 1; i < ind; i++) {
        free(filenames[i]);
    }
    free(filenames);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_options();
            return 0;
        }
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-e")) {
            encode_handle(argc, argv);
            break;
        }
        if (!strcmp(argv[i], "-d")) {
            decode_handle(argc, argv);
            break;
        }
    }
    return 0;
}
