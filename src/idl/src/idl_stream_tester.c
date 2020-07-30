
#include <stdio.h>
#include <stdlib.h>
#include "idl/streamer_generator.h"
#include "idl/backend.h"
#include "idl/processor.h"

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fputs("a file name needs to be supplied when starting the converter", stderr);
    return -1;
  }
  printf("testing\n");

  char* source = NULL;
  FILE* fp = fopen(argv[1], "r");

  if (NULL == fp)
  {
    fprintf(stderr, "could not open the file at location %s", argv[1]);
    return -2;
  }

  if (fseek(fp, 0L, SEEK_END) == 0)
  {
    /* Get the size of the file. */
    long bufsize = ftell(fp);
    if (bufsize == -1)
    {
      fputs("could not find the end of the file", stderr);
      return -3;
    }

    /* Allocate our buffer to that size. */
    source = malloc(sizeof(char) * ((size_t)bufsize + 1));

    /* Go back to the start of the file. */
    if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

    /* Read the entire file into memory. */
    size_t newLen = fread(source, sizeof(char), (size_t)bufsize, fp);
    if (ferror(fp) != 0)
    {
      fputs("Error reading file", stderr);
      return -4;
    }
    else
    {
      source[newLen++] = '\0'; /* Just to be safe. */
    }
  }
  fclose(fp);

  idl_tree_t* tree = 0x0;
  idl_parse_string(source, 0x0, &tree);

  idl_streamers_generate(source, argv[1]);
  free(source);

  return 0;
}
