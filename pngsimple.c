/*
 * Copyright 2002-2010 Guillaume Cottenceau.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#define PNG_DEBUG 3
#include <png.h>

static void abort_(const char * s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

struct png_file
{
    int width, height;
    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;
};

static int read_png_file(const char* file_name, struct png_file *p)
{
    unsigned char header[8];    // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        return 0;

    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8))
        abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);


    /* initialize stuff */
    p->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!p->png_ptr)
        abort_("[read_png_file] png_create_read_struct failed");

    p->info_ptr = png_create_info_struct(p->png_ptr);
    if (!p->info_ptr)
        abort_("[read_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[read_png_file] Error during init_io");

    png_init_io(p->png_ptr, fp);
    png_set_sig_bytes(p->png_ptr, 8);

    png_read_info(p->png_ptr, p->info_ptr);
    png_set_palette_to_rgb(p->png_ptr);
    png_set_add_alpha(p->png_ptr, 255, PNG_FILLER_AFTER);
    png_read_update_info(p->png_ptr, p->info_ptr);
        
    p->width = png_get_image_width(p->png_ptr, p->info_ptr);
    p->height = png_get_image_height(p->png_ptr, p->info_ptr);
    p->color_type = png_get_color_type(p->png_ptr, p->info_ptr);
    p->bit_depth = png_get_bit_depth(p->png_ptr, p->info_ptr);

    p->number_of_passes = png_set_interlace_handling(p->png_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[read_png_file] Error during read_image");

    p->row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * p->height);
    for (int y=0; y<p->height; y++)
        p->row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(p->png_ptr,p->info_ptr));

    png_read_image(p->png_ptr, p->row_pointers);

    png_destroy_read_struct(&p->png_ptr, &p->info_ptr, NULL);

    fclose(fp);    
    return 1;
}

static void write_png_file(const char* file_name, struct png_file *p)
{
    /* create file */
    FILE *fp = fopen(file_name, "wb");
    if (!fp)
        abort_("[write_png_file] File %s could not be opened for writing", file_name);


    /* initialize stuff */
    p->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!p->png_ptr)
        abort_("[write_png_file] png_create_write_struct failed");

    p->info_ptr = png_create_info_struct(p->png_ptr);
    if (!p->info_ptr)
        abort_("[write_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[write_png_file] Error during init_io");

    png_init_io(p->png_ptr, fp);


    /* write header */
    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[write_png_file] Error during writing header");

    png_set_IHDR(p->png_ptr, p->info_ptr, p->width, p->height,
                 p->bit_depth, p->color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(p->png_ptr, p->info_ptr);


    /* write bytes */
    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[write_png_file] Error during writing bytes");

    png_write_image(p->png_ptr, p->row_pointers);

    /* end write */
    if (setjmp(png_jmpbuf(p->png_ptr)))
        abort_("[write_png_file] Error during end of write");

    png_write_end(p->png_ptr, NULL);

    png_destroy_write_struct(&p->png_ptr, &p->info_ptr);

    /* cleanup heap allocation */
    for (int y=0; y<p->height; y++)
        free(p->row_pointers[y]);
    free(p->row_pointers);

    fclose(fp);
}
