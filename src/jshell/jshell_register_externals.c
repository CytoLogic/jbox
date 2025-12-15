#include "jshell_register_externals.h"

#include "apps/ls/cmd_ls.h"
#include "apps/stat/cmd_stat.h"
#include "apps/cat/cmd_cat.h"
#include "apps/head/cmd_head.h"
#include "apps/tail/cmd_tail.h"
#include "apps/cp/cmd_cp.h"
#include "apps/mv/cmd_mv.h"
#include "apps/rm/cmd_rm.h"
#include "apps/mkdir/cmd_mkdir.h"
#include "apps/rmdir/cmd_rmdir.h"
#include "apps/touch/cmd_touch.h"
#include "apps/rg/cmd_rg.h"
#include "apps/echo/cmd_echo.h"
#include "apps/sleep/cmd_sleep.h"
#include "apps/date/cmd_date.h"
#include "apps/less/cmd_less.h"


void jshell_register_all_external_commands(void) {
  jshell_register_ls_command();
  jshell_register_stat_command();
  jshell_register_cat_command();
  jshell_register_head_command();
  jshell_register_tail_command();
  jshell_register_cp_command();
  jshell_register_mv_command();
  jshell_register_rm_command();
  jshell_register_mkdir_command();
  jshell_register_rmdir_command();
  jshell_register_touch_command();
  jshell_register_rg_command();
  jshell_register_echo_command();
  jshell_register_sleep_command();
  jshell_register_date_command();
  jshell_register_less_command();
}
