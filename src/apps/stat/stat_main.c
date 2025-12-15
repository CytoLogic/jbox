#include "jshell/builtins/cmd_stat.h"

int main(int argc, char **argv) {
  return cmd_stat_spec.run(argc, argv);
}
