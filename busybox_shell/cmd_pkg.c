#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_pkg.h"

#define PKG_NAME "busybox_shell"
#define PKG_VERSION "1.0.0"
#define PKG_DESCRIPTION "Modular CLI utilities in C"

struct command_print_state {
    int first;
    int json;
};

struct package_info {
    char name[128];
    char version[64];
    char description[256];
    char files[32][PATH_MAX];
    int file_count;
};

static void print_command(const cmd_spec_t *spec, void *userdata)
{
    struct command_print_state *state = userdata;

    if (state->json) {
        if (!state->first) {
            printf(",\n");
        }

        printf("    {\"name\":\"%s\",\"summary\":\"%s\"}",
               spec->name,
               spec->summary);
    } else {
        printf("  %s - %s\n", spec->name, spec->summary);
    }

    state->first = 0;
}

static const char *home_dir(void)
{
    const char *home = getenv("HOME");

    return home != NULL ? home : ".";
}

static int make_path(char *out, size_t out_size, const char *a, const char *b)
{
    int written = snprintf(out, out_size, "%s/%s", a, b);

    return written < 0 || (size_t)written >= out_size;
}

static int make_mysh_path(char *out, size_t out_size, const char *suffix)
{
    int written = snprintf(out, out_size, "%s/.mysh/%s", home_dir(), suffix);

    return written < 0 || (size_t)written >= out_size;
}

static int mkdir_p(const char *path)
{
    char partial[PATH_MAX];
    size_t len;
    size_t index;

    len = strlen(path);
    if (len == 0 || len >= sizeof(partial)) {
        fprintf(stderr, "pkg: invalid path: %s\n", path);
        return 1;
    }

    strcpy(partial, path);
    for (index = 1; index < len; index++) {
        if (partial[index] != '/') {
            continue;
        }

        partial[index] = '\0';
        if (partial[0] != '\0' && mkdir(partial, 0777) != 0 && errno != EEXIST) {
            fprintf(stderr, "pkg: %s: %s\n", partial, strerror(errno));
            return 1;
        }
        partial[index] = '/';
    }

    if (mkdir(partial, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "pkg: %s: %s\n", partial, strerror(errno));
        return 1;
    }

    return 0;
}

static int run_program(char *const argv[])
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "pkg: fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "pkg: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "pkg: waitpid: %s\n", strerror(errno));
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 1;
    }

    return 0;
}

static int read_file(const char *path, char *buffer, size_t buffer_size)
{
    FILE *in;
    size_t nread;

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "pkg: %s: %s\n", path, strerror(errno));
        return 1;
    }

    nread = fread(buffer, 1, buffer_size - 1, in);
    if (ferror(in)) {
        fprintf(stderr, "pkg: %s: %s\n", path, strerror(errno));
        fclose(in);
        return 1;
    }

    buffer[nread] = '\0';
    fclose(in);
    return 0;
}

static int extract_json_string(
    const char *json,
    const char *key,
    char *out,
    size_t out_size
)
{
    char pattern[64];
    char *found;
    char *colon;
    char *start;
    char *end;
    size_t len;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    found = strstr(json, pattern);
    if (found == NULL) {
        return 1;
    }

    colon = strchr(found, ':');
    if (colon == NULL) {
        return 1;
    }

    start = strchr(colon, '"');
    if (start == NULL) {
        return 1;
    }
    start++;

    end = strchr(start, '"');
    if (end == NULL) {
        return 1;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

static int extract_json_files(const char *json, struct package_info *pkg)
{
    char *found = strstr(json, "\"files\"");
    char *start;
    char *end;
    char *cursor;

    pkg->file_count = 0;
    if (found == NULL) {
        return 1;
    }

    start = strchr(found, '[');
    end = strchr(found, ']');
    if (start == NULL || end == NULL || end < start) {
        return 1;
    }

    cursor = start + 1;
    while (cursor < end && pkg->file_count < 32) {
        char *item_start = strchr(cursor, '"');
        char *item_end;
        size_t len;

        if (item_start == NULL || item_start >= end) {
            break;
        }

        item_start++;
        item_end = strchr(item_start, '"');
        if (item_end == NULL || item_end > end) {
            break;
        }

        len = (size_t)(item_end - item_start);
        if (len >= PATH_MAX) {
            len = PATH_MAX - 1;
        }

        memcpy(pkg->files[pkg->file_count], item_start, len);
        pkg->files[pkg->file_count][len] = '\0';
        pkg->file_count++;
        cursor = item_end + 1;
    }

    return pkg->file_count == 0;
}

static int read_pkg_json(const char *package_dir, struct package_info *pkg)
{
    char path[PATH_MAX];
    char json[8192];

    memset(pkg, 0, sizeof(*pkg));
    if (make_path(path, sizeof(path), package_dir, "pkg.json") != 0) {
        fprintf(stderr, "pkg: package path too long\n");
        return 1;
    }

    if (read_file(path, json, sizeof(json)) != 0) {
        return 1;
    }

    if (extract_json_string(json, "name", pkg->name, sizeof(pkg->name)) != 0 ||
        extract_json_string(json, "version", pkg->version, sizeof(pkg->version)) != 0 ||
        extract_json_files(json, pkg) != 0) {
        fprintf(stderr, "pkg: %s: invalid or incomplete pkg.json\n", path);
        return 1;
    }

    if (extract_json_string(json, "description", pkg->description, sizeof(pkg->description)) != 0) {
        strcpy(pkg->description, "");
    }

    return 0;
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');

    return slash == NULL ? path : slash + 1;
}

static int append_db_entry(const struct package_info *pkg, const char *install_dir)
{
    char db_path[PATH_MAX];
    FILE *out;

    if (make_mysh_path(db_path, sizeof(db_path), "pkgdb.txt") != 0) {
        fprintf(stderr, "pkg: database path too long\n");
        return 1;
    }

    out = fopen(db_path, "a");
    if (out == NULL) {
        fprintf(stderr, "pkg: %s: %s\n", db_path, strerror(errno));
        return 1;
    }

    fprintf(out, "%s %s %s\n", pkg->name, pkg->version, install_dir);
    if (fclose(out) != 0) {
        fprintf(stderr, "pkg: %s: %s\n", db_path, strerror(errno));
        return 1;
    }

    return 0;
}

static int write_db_without(const char *name)
{
    char db_path[PATH_MAX];
    char temp_path[PATH_MAX];
    FILE *in;
    FILE *out;
    char line[PATH_MAX * 2];
    int status = 0;

    if (make_mysh_path(db_path, sizeof(db_path), "pkgdb.txt") != 0 ||
        make_mysh_path(temp_path, sizeof(temp_path), "pkgdb.txt.tmp") != 0) {
        fprintf(stderr, "pkg: database path too long\n");
        return 1;
    }

    in = fopen(db_path, "r");
    if (in == NULL) {
        if (errno == ENOENT) {
            return 0;
        }
        fprintf(stderr, "pkg: %s: %s\n", db_path, strerror(errno));
        return 1;
    }

    out = fopen(temp_path, "w");
    if (out == NULL) {
        fprintf(stderr, "pkg: %s: %s\n", temp_path, strerror(errno));
        fclose(in);
        return 1;
    }

    while (fgets(line, sizeof(line), in) != NULL) {
        char installed_name[128];

        if (sscanf(line, "%127s", installed_name) == 1 &&
            strcmp(installed_name, name) == 0) {
            continue;
        }
        fputs(line, out);
    }

    if (ferror(in) || fclose(in) != 0) {
        status = 1;
    }
    if (fclose(out) != 0) {
        status = 1;
    }
    if (status != 0) {
        fprintf(stderr, "pkg: failed to update package database\n");
        return 1;
    }

    if (rename(temp_path, db_path) != 0) {
        fprintf(stderr, "pkg: %s: %s\n", db_path, strerror(errno));
        return 1;
    }

    return 0;
}

static int find_installed_package(
    const char *name,
    char *version,
    size_t version_size,
    char *install_dir,
    size_t install_dir_size
)
{
    char db_path[PATH_MAX];
    FILE *in;
    char line[PATH_MAX * 2];

    if (make_mysh_path(db_path, sizeof(db_path), "pkgdb.txt") != 0) {
        fprintf(stderr, "pkg: database path too long\n");
        return 1;
    }

    in = fopen(db_path, "r");
    if (in == NULL) {
        return errno == ENOENT ? 1 : 1;
    }

    while (fgets(line, sizeof(line), in) != NULL) {
        char installed_name[128];
        char installed_version[64];
        char installed_dir[PATH_MAX];

        if (sscanf(line, "%127s %63s %1023s",
                   installed_name,
                   installed_version,
                   installed_dir) != 3) {
            continue;
        }

        if (strcmp(installed_name, name) == 0) {
            snprintf(version, version_size, "%s", installed_version);
            snprintf(install_dir, install_dir_size, "%s", installed_dir);
            fclose(in);
            return 0;
        }
    }

    fclose(in);
    return 1;
}

static int ensure_pkg_dirs(void)
{
    char path[PATH_MAX];

    if (make_mysh_path(path, sizeof(path), "pkgs") != 0 || mkdir_p(path) != 0) {
        return 1;
    }

    if (make_mysh_path(path, sizeof(path), "bin") != 0 || mkdir_p(path) != 0) {
        return 1;
    }

    return 0;
}

static int pkg_build(int argc, char **argv)
{
    char *tar_args[7];

    if (argc != 4) {
        fprintf(stderr, "pkg: usage: pkg build <src-dir> <output-tar>\n");
        return 1;
    }

    tar_args[0] = "tar";
    tar_args[1] = "-czf";
    tar_args[2] = argv[3];
    tar_args[3] = "-C";
    tar_args[4] = argv[2];
    tar_args[5] = ".";
    tar_args[6] = NULL;

    return run_program(tar_args);
}

static int remove_package_dir(const char *install_dir)
{
    char *rm_args[4];

    rm_args[0] = "rm";
    rm_args[1] = "-rf";
    rm_args[2] = (char *)install_dir;
    rm_args[3] = NULL;

    return run_program(rm_args);
}

static int create_executable_links(const struct package_info *pkg, const char *install_dir)
{
    char bin_dir[PATH_MAX];
    int index;

    if (make_mysh_path(bin_dir, sizeof(bin_dir), "bin") != 0) {
        fprintf(stderr, "pkg: bin path too long\n");
        return 1;
    }

    for (index = 0; index < pkg->file_count; index++) {
        char source[PATH_MAX];
        char link_path[PATH_MAX];
        const char *name = path_basename(pkg->files[index]);

        if (make_path(source, sizeof(source), install_dir, pkg->files[index]) != 0 ||
            make_path(link_path, sizeof(link_path), bin_dir, name) != 0) {
            fprintf(stderr, "pkg: executable path too long\n");
            return 1;
        }

        unlink(link_path);
        if (symlink(source, link_path) != 0) {
            fprintf(stderr, "pkg: symlink %s: %s\n", link_path, strerror(errno));
            return 1;
        }
    }

    return 0;
}

static int pkg_install(int argc, char **argv)
{
    char pkgs_dir[PATH_MAX];
    char temp_template[PATH_MAX];
    char *temp_dir;
    char install_dir[PATH_MAX];
    char package_dir_name[256];
    struct package_info pkg;

    if (argc != 3) {
        fprintf(stderr, "pkg: usage: pkg install <tar-file>\n");
        return 1;
    }

    if (ensure_pkg_dirs() != 0 ||
        make_mysh_path(pkgs_dir, sizeof(pkgs_dir), "pkgs") != 0) {
        return 1;
    }

    if (snprintf(temp_template, sizeof(temp_template), "%s/.install-XXXXXX", pkgs_dir) >=
        (int)sizeof(temp_template)) {
        fprintf(stderr, "pkg: temporary path too long\n");
        return 1;
    }

    temp_dir = mkdtemp(temp_template);
    if (temp_dir == NULL) {
        fprintf(stderr, "pkg: mkdtemp: %s\n", strerror(errno));
        return 1;
    }

    {
        char *extract_args[6] = { "tar", "-xzf", argv[2], "-C", temp_dir, NULL };
        if (run_program(extract_args) != 0) {
            remove_package_dir(temp_dir);
            return 1;
        }
    }

    if (read_pkg_json(temp_dir, &pkg) != 0) {
        remove_package_dir(temp_dir);
        return 1;
    }

    if (snprintf(package_dir_name, sizeof(package_dir_name), "%s-%s", pkg.name, pkg.version) >=
        (int)sizeof(package_dir_name) ||
        make_path(install_dir, sizeof(install_dir), pkgs_dir, package_dir_name) != 0) {
        fprintf(stderr, "pkg: install path too long\n");
        remove_package_dir(temp_dir);
        return 1;
    }

    write_db_without(pkg.name);
    remove_package_dir(install_dir);

    if (rename(temp_dir, install_dir) != 0) {
        fprintf(stderr, "pkg: install %s: %s\n", install_dir, strerror(errno));
        remove_package_dir(temp_dir);
        return 1;
    }

    if (create_executable_links(&pkg, install_dir) != 0 ||
        append_db_entry(&pkg, install_dir) != 0) {
        return 1;
    }

    printf("Installed %s %s\n", pkg.name, pkg.version);
    return 0;
}

static int pkg_list(void)
{
    char db_path[PATH_MAX];
    FILE *in;
    char line[PATH_MAX * 2];
    int any = 0;

    if (make_mysh_path(db_path, sizeof(db_path), "pkgdb.txt") != 0) {
        fprintf(stderr, "pkg: database path too long\n");
        return 1;
    }

    in = fopen(db_path, "r");
    if (in == NULL) {
        if (errno == ENOENT) {
            printf("No packages installed.\n");
            return 0;
        }
        fprintf(stderr, "pkg: %s: %s\n", db_path, strerror(errno));
        return 1;
    }

    while (fgets(line, sizeof(line), in) != NULL) {
        char name[128];
        char version[64];

        if (sscanf(line, "%127s %63s", name, version) == 2) {
            printf("%s %s\n", name, version);
            any = 1;
        }
    }

    fclose(in);
    if (!any) {
        printf("No packages installed.\n");
    }

    return 0;
}

static int remove_executable_links(const char *install_dir)
{
    struct package_info pkg;
    char bin_dir[PATH_MAX];
    int index;

    if (read_pkg_json(install_dir, &pkg) != 0) {
        return 1;
    }

    if (make_mysh_path(bin_dir, sizeof(bin_dir), "bin") != 0) {
        return 1;
    }

    for (index = 0; index < pkg.file_count; index++) {
        char link_path[PATH_MAX];

        if (make_path(link_path, sizeof(link_path), bin_dir, path_basename(pkg.files[index])) != 0) {
            fprintf(stderr, "pkg: executable link path too long\n");
            return 1;
        }

        if (unlink(link_path) != 0 && errno != ENOENT) {
            fprintf(stderr, "pkg: %s: %s\n", link_path, strerror(errno));
            return 1;
        }
    }

    return 0;
}

static int pkg_remove(int argc, char **argv)
{
    char version[64];
    char install_dir[PATH_MAX];

    if (argc != 3) {
        fprintf(stderr, "pkg: usage: pkg remove <name>\n");
        return 1;
    }

    if (find_installed_package(argv[2], version, sizeof(version),
                               install_dir, sizeof(install_dir)) != 0) {
        fprintf(stderr, "pkg: %s is not installed\n", argv[2]);
        return 1;
    }

    remove_executable_links(install_dir);
    if (remove_package_dir(install_dir) != 0 ||
        write_db_without(argv[2]) != 0) {
        return 1;
    }

    printf("Removed %s %s\n", argv[2], version);
    return 0;
}

static void print_metadata(int json)
{
    struct command_print_state state;

    state.first = 1;
    state.json = json;

    if (json) {
        printf("{\n");
        printf("  \"name\":\"%s\",\n", PKG_NAME);
        printf("  \"version\":\"%s\",\n", PKG_VERSION);
        printf("  \"description\":\"%s\",\n", PKG_DESCRIPTION);
        printf("  \"commands\":[\n");
        for_each_command(print_command, &state);
        printf("\n  ]\n");
        printf("}\n");
    } else {
        printf("Package: %s\n", PKG_NAME);
        printf("Version: %s\n", PKG_VERSION);
        printf("Description: %s\n", PKG_DESCRIPTION);
        printf("Commands:\n");
        for_each_command(print_command, &state);
    }
}

int pkg_run(int argc, char **argv)
{
    if (argc == 1) {
        print_metadata(0);
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        pkg_print_usage(stdout);
        return 0;
    }

    if (strcmp(argv[1], "--json") == 0) {
        print_metadata(1);
        return 0;
    }

    if (strcmp(argv[1], "build") == 0) {
        return pkg_build(argc, argv);
    }
    if (strcmp(argv[1], "install") == 0) {
        return pkg_install(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0) {
        return pkg_list();
    }
    if (strcmp(argv[1], "remove") == 0) {
        return pkg_remove(argc, argv);
    }

    fprintf(stderr, "pkg: unknown subcommand: %s\n", argv[1]);
    pkg_print_usage(stderr);
    return 1;
}

void pkg_print_usage(FILE *out)
{
    fprintf(out, "Usage: pkg [--json]\n");
    fprintf(out, "       pkg build <src-dir> <output-tar>\n");
    fprintf(out, "       pkg install <tar-file>\n");
    fprintf(out, "       pkg list\n");
    fprintf(out, "       pkg remove <name>\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Build, install, list, and remove packages for this shell.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "print built-in command metadata as JSON");

    fprintf(out, "\nSubcommands:\n");
    fprintf(out, "  %-20s %s\n", "build", "create a .tar.gz package archive");
    fprintf(out, "  %-20s %s\n", "install", "extract a package and link its executables");
    fprintf(out, "  %-20s %s\n", "list", "show installed packages");
    fprintf(out, "  %-20s %s\n", "remove", "uninstall a package by name");
}

cmd_spec_t cmd_pkg_spec = {
    .name = "pkg",
    .summary = "manage shell packages",
    .long_help = "Build, install, list, and remove packages for this shell.",
    .run = pkg_run,
    .print_usage = pkg_print_usage,
};

void register_pkg_command(void)
{
    register_command(&cmd_pkg_spec);
}
