#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


struct pixel {
    int red;
    int green;
    int blue;
};


void write_petic(int row, int column, long x, long y, int petic_h, int petic_w,
        int img_w, int zoom, struct pixel *a, struct pixel *result)
{
        long i, j, pos_source, pos_result;
        for (i = 0; i < petic_h; i++) {
            for (j = 0; j < petic_w; j++) {
                pos_source = (x + i) * img_w + y + j;
                pos_result = row * img_w * zoom + column * petic_w + j;
                printf("\tWriting from %ld to %ld, petic_h = %d, petic_w = %d\n",
                        pos_source, pos_result, petic_h, petic_w);
                result[pos_result] = a[pos_source];
            }
        }
}

int main(int argc, char **argv)
{
    int zoom, rows, columns, overlap_vert, overlap_oriz;
    long height, width, i, j, k, petic_x, petic_y;
    int max_color, line_no = 0;
    char line[256];
    char *numbers, *tok;
    struct pixel *a, *result;

    if (argc < 8)
        perror("Insert the name of the input file");
    


    /* Store the arguments */
    char *input_file = argv[1];
    char *output_file = argv[2];
    zoom = atoi(argv[3]);
    rows = atoi(argv[4]);
    columns = atoi(argv[5]);
    overlap_vert = atoi(argv[6]);
    overlap_oriz = atoi(argv[7]);
    printf("Input file name %s\n", input_file);



    /* Read the matrix */
    FILE *fin = fopen(input_file, "r");
    if (!fin)
        perror("Error while opening input file for reading");
    while(fgets(line, sizeof(line), fin) != NULL) {
        /* Exclude comments */
        if (line[0] == '#')
            continue;
        /* Check if the file is ppm */
        if (!line_no && strncmp(line, "P3", 2)) {
            perror("The input file is not ppm");
            return 1;
        }
        line_no++;
        if (line_no == 2) {
            numbers = strtok(line, " ");
            width = atol(numbers);
            while (1) {
                tok = strtok(NULL, " ");
                if (tok == NULL)
                    break;
                height = atol(tok);
            }
            a = malloc(width * height * sizeof(struct pixel));
            if (!a) {
                perror("Error on alocating matrix");
                return 1;
            }
        }
        if (line_no == 3) {
            max_color = atoi(line);
            printf("Width = %ld, height = %ld, ", width, height);
            printf("Max color = %d\n", max_color);
        }
        /* Read the RGB values */
        if (line_no > 3) {
            if (i % 3 == 0)
                a[i / 3].red = atoi(line);
            if (i % 3 == 1)
                a[i / 3].green = atoi(line);
            if (i % 3 == 2)
                a[i / 3].blue = atoi(line);
            i++;
        }
    }


    int petic_h = height * zoom / rows;
    int petic_w = width * zoom / columns;
    printf("Dimensiuni petic %d pe %d\n", petic_h, petic_w);
    srand(time(NULL));
    result = malloc(height * zoom * zoom * width * sizeof(struct pixel));
    
    printf("Successful reach after allocation\n");
    for (j = 0; j < rows; j++) {
        for (k = 0; k < columns; k++) {
            petic_x = rand() % (height - petic_h);
            petic_y = rand() % (width - petic_w);
            printf("Choosing %ld and %ld\n", petic_x, petic_y);
            write_petic(j, k, petic_x, petic_y,
                    petic_h, petic_w, width, zoom, a, result);
        }
    }



    /* Write the result file */
    long pos;
    FILE *fout = fopen(output_file, "w");
    fprintf(fout, "P3\n");
    fprintf(fout,"%ld %ld\n", width * zoom, height * zoom);
    fprintf(fout, "%d\n", max_color);
    for (i = 0; i < height * zoom; i++) {
        for (j = 0; j < width * zoom; j++) {
            pos = i * width * zoom + j;
            fprintf(fout, "%d\n%d\n%d\n", result[pos].red, result[pos].green,
                    result[pos].blue);
        }
    }

    fclose(fin);
    fclose(fout);
    return 0;
}
