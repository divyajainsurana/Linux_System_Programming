#ifndef CMD_RPC_H
#define CMD_RPC_H

#include <stdio.h>

int rpc_run(int argc, char **argv);
void rpc_print_usage(FILE *out);
void register_rpc_command(void);

#endif
