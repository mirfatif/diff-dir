#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>

#define VERSION "v0.1"

static int test_args(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "\n%s %s\n", basename(argv[0]), VERSION);
        fprintf(stderr, "\nUsage:\n\t%s <dir1> <dir2>\n", basename(argv[0]));
        fprintf(stderr, "\nFind difference of two directories recursively based on file size and modification time.\n\n");
        return 1;
    }

    struct stat st;
    char dir1[PATH_MAX];
    char dir2[PATH_MAX];

    for (int i = 1; i <= 2; i++)
    {
        if (lstat(argv[i], &st))
        {
            fprintf(stderr, "%s: %s\n", argv[i], strerror(errno));
            return 1;
        }

        if (!S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "Not a directory: %s\n", argv[i]);
            return 1;
        }

        if (!realpath(argv[i], i == 1 ? dir1 : dir2))
        {
            fprintf(stderr, "%s: %s\n", argv[i], strerror(errno));
            return 1;
        }
    }

    if (!strcmp(dir1, dir2))
    {
        fprintf(stderr, "Same directories\n");
        return 1;
    }

    return 0;
}

static int file_lstat(char *path, struct stat *st, bool must_exist)
{
    if (lstat(path, st))
    {
        if (must_exist || errno != ENOENT)
        {
            fprintf(stderr, "%s: %s\n", path, strerror(errno));
            return 1;
        }

        return -1;
    }

    if (!S_ISREG(st->st_mode))
    {
        if (must_exist)
        {
            fprintf(stderr, "File changed: %s\n", path);
            return 1;
        }

        return -2;
    }

    return 0;
}

static int check_same(char *path1, char *path2, bool first_pass, bool check_size)
{
    if (!first_pass || !check_size)
    {
        if (access(path2, F_OK))
        {
            if (errno != ENOENT)
            {
                fprintf(stderr, "Failed to access %s: %s\n", path2, strerror(errno));
                return 1;
            }

            printf("MISSING: %s\n", path2);
        }
    }
    else
    {
        struct stat st;

        if (file_lstat(path1, &st, true))
            return 1;

        long size = st.st_size;
        unsigned long mtime = st.st_mtime;

        int err = file_lstat(path2, &st, false);

        if (err == 1)
            return 1;

        if (err == -1)
        {
            printf("MISSING: %s\n", path2);
            return 0;
        }

        if (err == -2 || size != st.st_size || mtime != st.st_mtime)
            printf("DIFFERS: %s\n", path1);
    }

    return 0;
}

static int compare(char *dir1, char *dir2, bool first_pass)
{
    DIR *dir = opendir(dir1);

    if (!dir)
    {
        fprintf(stderr, "Failed to read %s: %s\n", dir1, strerror(errno));
        return 1;
    }

    const struct dirent *entry;
    char path1[PATH_MAX];
    char path2[PATH_MAX];

    while ((entry = readdir(dir)))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        sprintf(path1, "%s/%s", dir1, entry->d_name);
        sprintf(path2, "%s/%s", dir2, entry->d_name);

        if (entry->d_type == DT_DIR)
        {
            if (compare(path1, path2, first_pass))
            {
                closedir(dir);
                return 1;
            }
        }
        else if (check_same(path1, path2, first_pass, entry->d_type == DT_REG))
        {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char **argv)
{
    if (test_args(argc, argv))
        return 1;

    while (argv[1][strlen(argv[1]) - 1] == '/')
        argv[1][strlen(argv[1]) - 1] = '\0';

    while (argv[2][strlen(argv[2]) - 1] == '/')
        argv[2][strlen(argv[2]) - 1] = '\0';

    if (compare(argv[1], argv[2], true))
        return 1;

    if (compare(argv[2], argv[1], false))
        return 1;

    return 0;
}
