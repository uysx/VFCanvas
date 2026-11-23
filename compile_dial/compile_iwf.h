/*
 * Name: compile_iwf.h
 * Author: OpenIDO
 * Description: Header for the Actions .iwf format packer
*/

#ifndef MAKE_IWF_FILE_H
#define MAKE_IWF_FILE_H

#include <stddef.h>
#include <stdint.h>

#define IWF_HEADER_SIZE 8
#define IWF_ENTRY_SIZE  40

#define IWF_IMG_EXT_PNG ".png"
#define IWF_IMG_EXT_BMP ".bmp"

#define IWF_SPECIAL_IWF_JSON  "iwf.json"
#define IWF_SPECIAL_IWF1_JSON "iwf1.json"
#define IWF_SPECIAL_FONT_JSON "font.json"

#define IWF_MAX_NAME_LEN 31

uint16_t iwf_pack_rgb565(uint8_t r, uint8_t g, uint8_t b);

uint8_t *iwf_convert_image_to_raw(const char *path,
                                  int force_opaque_preview,
                                  size_t *out_size);

uint8_t *iwf_load_file_data(const char *path, size_t *out_size);

typedef struct {
    char   *name;
    uint8_t *data;
    size_t  size;
} IwfFileEntry;

IwfFileEntry *iwf_collect_files(const char *input_dir, size_t *out_count);

int iwf_build(const IwfFileEntry *files, size_t count, const char *output_file);

int iwf_pack(const char *input_dir, const char *output_iwf);

#endif