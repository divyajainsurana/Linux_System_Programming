#include <stdio.h>
#include <string.h>
#include "cmd_spec.h"

void register_all_builtin_commands(void);

int main(int argc, char **argv)
{
    register_all_builtin_commands();

    if (argc < 2) {
        printf("Usage: <command> [args]\n");
        return 1;
    }

    const cmd_spec_t *cmd = find_command(argv[1]);

    if (!cmd) {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return cmd->run(argc - 1, argv + 1);
}
