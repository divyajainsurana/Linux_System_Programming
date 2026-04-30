
void register_ls_command(void);
void register_localdate_command(void);
void register_cat_command(void);
void register_pkg_command(void);

void register_all_builtin_commands(void)
{
    register_ls_command();
    register_localdate_command();
    register_cat_command();
    register_pkg_command();
}
