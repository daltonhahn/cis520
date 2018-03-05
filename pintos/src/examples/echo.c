#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;

  printf("arg <%d>\n", argc);
  printf("argv[0] <%s>\n", argv[0]);
  printf("Hello World\n");

  /*
  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");
  */

  return EXIT_SUCCESS;
}
