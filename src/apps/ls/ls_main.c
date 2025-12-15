#include "jshell/builtins/cmd_ls.h"

int main(int argc, char **argv) {
  return cmd_ls_spec.run(argc, argv);
}
