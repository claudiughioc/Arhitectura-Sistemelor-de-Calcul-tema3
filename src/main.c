#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2)
        perror("Insert the name of the input file");

    char *input_file = argv[1];
    printf("Input file name %s\n", input_file);

    FILE *fin = fopen(input_file, "r");
    if (!fin)
        perror("Error while opening input file for reading");


    return 0;
}
