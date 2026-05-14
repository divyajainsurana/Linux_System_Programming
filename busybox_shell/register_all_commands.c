
void register_ls_command(void);
void register_localdate_command(void);
void register_cat_command(void);
void register_pkg_command(void);
void register_pwd_command(void);
void register_wc_command(void);
void register_touch_command(void);
void register_mkdir_command(void);
void register_rmdir_command(void);
void register_echo_command(void);
void register_whoami_command(void);
void register_clear_command(void);
void register_id_command(void);
void register_uname_command(void);
void register_head_command(void);
void register_tail_command(void);
void register_cp_command(void);
void register_mv_command(void);
void register_rm_command(void);
void register_dirname_command(void);
void register_du_command(void);

void register_all_builtin_commands(void)
{
    register_ls_command();
    register_localdate_command();
    register_cat_command();
    register_pkg_command();
    register_pwd_command();
    register_wc_command();
    register_touch_command();
    register_mkdir_command();
    register_rmdir_command();
    register_echo_command();
    register_whoami_command();
    register_clear_command();
    register_id_command();
    register_uname_command();
    register_head_command();
    register_tail_command();
    register_cp_command();
    register_mv_command();
    register_rm_command();
    register_dirname_command();
    register_du_command();
}
