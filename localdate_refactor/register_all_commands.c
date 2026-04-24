
void register_localdate_command(void);

void register_all_builtin_commands(void)
{
    register_ls_command();
    register_localdate_command();
}
