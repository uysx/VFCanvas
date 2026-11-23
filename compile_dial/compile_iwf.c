/*
 * Name: compile_iwf.c
 * Author: OpenIDO
 * Description: Watch face packer for the Actions .iwf format
*/

#include "compile_iwf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WIN32

#include <windows.h>

typedef struct {
    char *name;
    char *path;
} NamePath;

static int is_directory(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int is_regular_file(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static NamePath *list_dir(const char *dir, size_t *out_count)
{
    size_t cap = 16;
    size_t count = 0;
    NamePath *list = (NamePath *)calloc(cap, sizeof(NamePath));
    if (!list) {
        *out_count = 0;
        return NULL;
    }

    size_t dlen = strlen(dir);
    int need_sep = (dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');

    char *pattern = (char *)malloc(dlen + 4);
    if (!pattern) {
        free(list);
        *out_count = 0;
        return NULL;
    }
    memcpy(pattern, dir, dlen);
    size_t poff = dlen;
    if (need_sep)
        pattern[poff++] = '\\';
    pattern[poff++] = '*';
    pattern[poff++] = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);
    if (h == INVALID_HANDLE_VALUE) {
        free(list);
        *out_count = 0;
        return NULL;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        if (count == cap) {
            cap *= 2;
            NamePath *tmp = (NamePath *)realloc(list, cap * sizeof(NamePath));
            if (!tmp)
                break;
            list = tmp;
        }

        size_t nlen = strlen(fd.cFileName);
        size_t total = dlen + (need_sep ? 1 : 0) + nlen + 1;
        char *full = (char *)malloc(total);
        if (!full)
            break;
        memcpy(full, dir, dlen);
        size_t off = dlen;
        if (need_sep)
            full[off++] = '/';
        memcpy(full + off, fd.cFileName, nlen);
        full[off + nlen] = '\0';

        list[count].name = _strdup(fd.cFileName);
        list[count].path = full;
        count++;
    } while (FindNextFileA(h, &fd));

    FindClose(h);

    *out_count = count;
    return list;
}

#else

#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char *name;
    char *path;
} NamePath;

static int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode);
}

static int is_regular_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISREG(st.st_mode);
}

static NamePath *list_dir(const char *dir, size_t *out_count)
{
    size_t cap = 16;
    size_t count = 0;
    NamePath *list = (NamePath *)calloc(cap, sizeof(NamePath));
    if (!list) {
        *out_count = 0;
        return NULL;
    }

    DIR *d = opendir(dir);
    if (!d) {
        free(list);
        *out_count = 0;
        return NULL;
    }

    size_t dlen = strlen(dir);
    int need_sep = (dlen > 0 && dir[dlen - 1] != '/');

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        if (count == cap) {
            cap *= 2;
            NamePath *tmp = (NamePath *)realloc(list, cap * sizeof(NamePath));
            if (!tmp)
                break;
            list = tmp;
        }

        size_t nlen = strlen(de->d_name);
        size_t total = dlen + (need_sep ? 1 : 0) + nlen + 1;
        char *full = (char *)malloc(total);
        if (!full)
            break;
        memcpy(full, dir, dlen);
        size_t off = dlen;
        if (need_sep)
            full[off++] = '/';
        memcpy(full + off, de->d_name, nlen);
        full[off + nlen] = '\0';

        list[count].name = strdup(de->d_name);
        list[count].path = full;
        count++;
    }

    closedir(d);
    *out_count = count;
    return list;
}

#endif

static int ci_strcmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
            return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int cmp_namepath(const void *a, const void *b)
{
    const NamePath *na = (const NamePath *)a;
    const NamePath *nb = (const NamePath *)b;
    return ci_strcmp(na->name, nb->name);
}

static char *path_join(const char *dir, const char *name)
{
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int need_sep = (dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');
    size_t total = dlen + (need_sep ? 1 : 0) + nlen + 1;
    char *out = (char *)malloc(total);
    if (!out)
        return NULL;
    memcpy(out, dir, dlen);
    size_t off = dlen;
    if (need_sep)
        out[off++] = '/';
    memcpy(out + off, name, nlen);
    out[off + nlen] = '\0';
    return out;
}

static char *stem_of(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    size_t len = dot ? (size_t)(dot - filename) : strlen(filename);
    char *out = (char *)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, filename, len);
    out[len] = '\0';
    return out;
}

uint16_t iwf_pack_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t r5 = (r >> 3) & 0x1F;
    uint16_t g6 = (g >> 2) & 0x3F;
    uint16_t b5 = (b >> 3) & 0x1F;
    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

uint8_t *iwf_convert_image_to_raw(const char *path,
                                  int force_opaque_preview,
                                  size_t *out_size)
{
    int w = 0, h = 0, channels = 0;
    unsigned char *img = stbi_load(path, &w, &h, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to load image '%s': %s\n",
                path, stbi_failure_reason());
        return NULL;
    }

    size_t npixels = (size_t)w * (size_t)h;

    int has_alpha = 0;
    if (!force_opaque_preview) {
        for (size_t i = 0; i < npixels; i++) {
            if (img[i * 4 + 3] != 255) {
                has_alpha = 1;
                break;
            }
        }
    }

    size_t rgb565_size = npixels * 2;
    uint8_t *rgb565 = (uint8_t *)malloc(rgb565_size);
    if (!rgb565) {
        stbi_image_free(img);
        return NULL;
    }

    for (size_t i = 0; i < npixels; i++) {
        uint8_t r = img[i * 4 + 0];
        uint8_t g = img[i * 4 + 1];
        uint8_t b = img[i * 4 + 2];
        uint16_t px = iwf_pack_rgb565(r, g, b);
        rgb565[i * 2 + 0] = (uint8_t)((px >> 8) & 0xFF);
        rgb565[i * 2 + 1] = (uint8_t)(px & 0xFF);
    }

    uint8_t *alpha_a4 = NULL;
    size_t alpha_size = 0;
    if (has_alpha) {
        alpha_size = (npixels + 1) / 2;
        alpha_a4 = (uint8_t *)malloc(alpha_size);
        if (!alpha_a4) {
            free(rgb565);
            stbi_image_free(img);
            return NULL;
        }

        for (size_t i = 0; i < npixels; i += 2) {
            uint8_t a0 = img[i * 4 + 3];
            uint8_t a0_4 = (uint8_t)((a0 * 15) / 255);

            uint8_t a1_4 = 0;
            if (i + 1 < npixels) {
                uint8_t a1 = img[(i + 1) * 4 + 3];
                a1_4 = (uint8_t)((a1 * 15) / 255);
            }

            alpha_a4[i / 2] = (uint8_t)((a0_4 << 4) | a1_4);
        }
    }

    uint8_t header[16];
    memset(header, 0, sizeof(header));
    header[0] = 'R';
    header[1] = 'A';
    header[2] = 'W';
    header[3] = 0x00;

    header[4] = (uint8_t)(w & 0xFF);
    header[5] = (uint8_t)((w >> 8) & 0xFF);
    header[6] = (uint8_t)(h & 0xFF);
    header[7] = (uint8_t)((h >> 8) & 0xFF);

    if (has_alpha) {
        header[8] = 0x85;
        header[9] = 0x66;
        uint32_t rgb_size = (uint32_t)rgb565_size;
        header[12] = (uint8_t)(rgb_size & 0xFF);
        header[13] = (uint8_t)((rgb_size >> 8) & 0xFF);
        header[14] = (uint8_t)((rgb_size >> 16) & 0xFF);
        header[15] = (uint8_t)((rgb_size >> 24) & 0xFF);
    } else {
        header[8] = 0x85;
        header[9] = 0x00;
    }

    size_t total = sizeof(header) + rgb565_size + alpha_size;
    uint8_t *out = (uint8_t *)malloc(total);
    if (!out) {
        free(rgb565);
        free(alpha_a4);
        stbi_image_free(img);
        return NULL;
    }

    size_t off = 0;
    memcpy(out + off, header, sizeof(header));
    off += sizeof(header);
    memcpy(out + off, rgb565, rgb565_size);
    off += rgb565_size;
    if (alpha_a4) {
        memcpy(out + off, alpha_a4, alpha_size);
        off += alpha_size;
    }

    free(rgb565);
    free(alpha_a4);
    stbi_image_free(img);

    *out_size = total;
    return out;
}

static int has_image_ext(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return 0;
    if (ci_strcmp(dot, IWF_IMG_EXT_PNG) == 0)
        return 1;
    if (ci_strcmp(dot, IWF_IMG_EXT_BMP) == 0)
        return 1;
    return 0;
}

static int is_preview_stem(const char *path)
{
    const char *base = strrchr(path, '/');
#ifdef _WIN32
    const char *bs = strrchr(path, '\\');
    if (bs && (!base || bs > base))
        base = bs;
#endif
    base = base ? base + 1 : path;

    const char *dot = strrchr(base, '.');
    size_t stem_len = dot ? (size_t)(dot - base) : strlen(base);

    const char *preview = "preview";
    size_t i;
    for (i = 0; i < stem_len && preview[i]; i++) {
        if (tolower((unsigned char)base[i]) != preview[i])
            return 0;
    }
    return (i == stem_len && preview[i] == '\0');
}

uint8_t *iwf_load_file_data(const char *path, size_t *out_size)
{
    if (!has_image_ext(path)) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            fprintf(stderr, "Failed to open '%s': %s\n", path, strerror(errno));
            return NULL;
        }
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            return NULL;
        }
        long sz = ftell(f);
        if (sz < 0) {
            fclose(f);
            return NULL;
        }
        rewind(f);

        uint8_t *buf = (uint8_t *)malloc((size_t)sz);
        if (!buf) {
            fclose(f);
            return NULL;
        }
        if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
            free(buf);
            fclose(f);
            return NULL;
        }
        fclose(f);
        *out_size = (size_t)sz;
        return buf;
    }

    int force_opaque = is_preview_stem(path);
    return iwf_convert_image_to_raw(path, force_opaque, out_size);
}

IwfFileEntry *iwf_collect_files(const char *input_dir, size_t *out_count)
{
    size_t cap = 16;
    size_t count = 0;
    IwfFileEntry *entries = (IwfFileEntry *)calloc(cap, sizeof(IwfFileEntry));
    if (!entries)
        return NULL;

    const char *specials[] = {
        IWF_SPECIAL_IWF_JSON,
        IWF_SPECIAL_IWF1_JSON,
        IWF_SPECIAL_FONT_JSON
    };
    for (int i = 0; i < 3; i++) {
        char *p = path_join(input_dir, specials[i]);
        if (!p)
            goto fail;
        if (is_regular_file(p)) {
            size_t sz = 0;
            uint8_t *data = iwf_load_file_data(p, &sz);
            if (!data) {
                free(p);
                goto fail;
            }
            if (count == cap) {
                cap *= 2;
                IwfFileEntry *tmp = (IwfFileEntry *)realloc(entries, cap * sizeof(IwfFileEntry));
                if (!tmp) {
                    free(data);
                    free(p);
                    goto fail;
                }
                entries = tmp;
            }
#ifdef _WIN32
            entries[count].name = _strdup(specials[i]);
#else
            entries[count].name = strdup(specials[i]);
#endif
            entries[count].data = data;
            entries[count].size = sz;
            count++;
        }
        free(p);
    }

    {
        size_t n = 0;
        NamePath *items = list_dir(input_dir, &n);
        if (!items)
            goto fail;

        size_t rf = 0;
        for (size_t i = 0; i < n; i++) {
            int is_special = 0;
            for (int k = 0; k < 3; k++) {
                if (ci_strcmp(items[i].name, specials[k]) == 0) {
                    is_special = 1;
                    break;
                }
            }
            if (is_special || !is_regular_file(items[i].path)) {
                free(items[i].name);
                free(items[i].path);
                continue;
            }
            items[rf++] = items[i];
        }

        qsort(items, rf, sizeof(NamePath), cmp_namepath);

        for (size_t i = 0; i < rf; i++) {
            size_t sz = 0;
            uint8_t *data = iwf_load_file_data(items[i].path, &sz);
            if (!data) {
                for (size_t j = i; j < rf; j++) {
                    free(items[j].name);
                    free(items[j].path);
                }
                free(items);
                goto fail;
            }
            if (count == cap) {
                cap *= 2;
                IwfFileEntry *tmp = (IwfFileEntry *)realloc(entries, cap * sizeof(IwfFileEntry));
                if (!tmp) {
                    free(data);
                    for (size_t j = i; j < rf; j++) {
                        free(items[j].name);
                        free(items[j].path);
                    }
                    free(items);
                    goto fail;
                }
                entries = tmp;
            }
#ifdef _WIN32
            entries[count].name = _strdup(items[i].name);
#else
            entries[count].name = strdup(items[i].name);
#endif
            entries[count].data = data;
            entries[count].size = sz;
            count++;

            free(items[i].name);
            free(items[i].path);
        }
        free(items);
    }

    {
        size_t n = 0;
        NamePath *items = list_dir(input_dir, &n);
        if (!items)
            goto fail;

        size_t sd = 0;
        for (size_t i = 0; i < n; i++) {
            if (!is_directory(items[i].path)) {
                free(items[i].name);
                free(items[i].path);
                continue;
            }
            items[sd++] = items[i];
        }

        qsort(items, sd, sizeof(NamePath), cmp_namepath);

        for (size_t si = 0; si < sd; si++) {
            size_t m = 0;
            NamePath *sub = list_dir(items[si].path, &m);
            if (!sub)
                continue;

            size_t sf = 0;
            for (size_t i = 0; i < m; i++) {
                if (!is_regular_file(sub[i].path)) {
                    free(sub[i].name);
                    free(sub[i].path);
                    continue;
                }
                sub[sf++] = sub[i];
            }

            qsort(sub, sf, sizeof(NamePath), cmp_namepath);

            for (size_t fi = 0; fi < sf; fi++) {
                size_t sz = 0;
                uint8_t *data = iwf_load_file_data(sub[fi].path, &sz);
                if (!data) {
                    for (size_t j = fi; j < sf; j++) {
                        free(sub[j].name);
                        free(sub[j].path);
                    }
                    free(sub);
                    for (size_t j = si; j < sd; j++) {
                        free(items[j].name);
                        free(items[j].path);
                    }
                    free(items);
                    goto fail;
                }

                char *stem = stem_of(sub[fi].name);
                if (!stem) {
                    free(data);
                    for (size_t j = fi; j < sf; j++) {
                        free(sub[j].name);
                        free(sub[j].path);
                    }
                    free(sub);
                    for (size_t j = si; j < sd; j++) {
                        free(items[j].name);
                        free(items[j].path);
                    }
                    free(items);
                    goto fail;
                }

                size_t dir_len = strlen(items[si].name);
                size_t stem_len = strlen(stem);
                char *vname = (char *)malloc(dir_len + 1 + stem_len + 1);
                if (!vname) {
                    free(stem);
                    free(data);
                    for (size_t j = fi; j < sf; j++) {
                        free(sub[j].name);
                        free(sub[j].path);
                    }
                    free(sub);
                    for (size_t j = si; j < sd; j++) {
                        free(items[j].name);
                        free(items[j].path);
                    }
                    free(items);
                    goto fail;
                }
                memcpy(vname, items[si].name, dir_len);
                vname[dir_len] = '_';
                memcpy(vname + dir_len + 1, stem, stem_len);
                vname[dir_len + 1 + stem_len] = '\0';
                free(stem);

                if (count == cap) {
                    cap *= 2;
                    IwfFileEntry *tmp = (IwfFileEntry *)realloc(entries, cap * sizeof(IwfFileEntry));
                    if (!tmp) {
                        free(vname);
                        free(data);
                        for (size_t j = fi; j < sf; j++) {
                            free(sub[j].name);
                            free(sub[j].path);
                        }
                        free(sub);
                        for (size_t j = si; j < sd; j++) {
                            free(items[j].name);
                            free(items[j].path);
                        }
                        free(items);
                        goto fail;
                    }
                    entries = tmp;
                }

                entries[count].name = vname;
                entries[count].data = data;
                entries[count].size = sz;
                count++;

                free(sub[fi].name);
                free(sub[fi].path);
            }
            free(sub);

            free(items[si].name);
            free(items[si].path);
        }
        free(items);
    }

    *out_count = count;
    return entries;

fail:
    for (size_t i = 0; i < count; i++) {
        free(entries[i].name);
        free(entries[i].data);
    }
    free(entries);
    return NULL;
}

static void write_u16_le(FILE *f, uint16_t v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
}

static void write_u32_le(FILE *f, uint32_t v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 24) & 0xFF, f);
}

int iwf_build(const IwfFileEntry *files, size_t count, const char *output_file)
{
    if (count > 0xFFFF) {
        fprintf(stderr, "Too many entries (%zu), max is 65535\n", count);
        return 1;
    }

    uint32_t data_start = (uint32_t)(IWF_HEADER_SIZE + count * IWF_ENTRY_SIZE);
    uint32_t *offsets = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!offsets)
        return 1;

    uint32_t offset = data_start;
    for (size_t i = 0; i < count; i++) {
        offsets[i] = offset;
        offset += (uint32_t)files[i].size;
    }

    FILE *f = fopen(output_file, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open output '%s': %s\n",
                output_file, strerror(errno));
        free(offsets);
        return 1;
    }

    fwrite("iwf\0", 1, 4, f);
    write_u16_le(f, 1);
    write_u16_le(f, (uint16_t)count);

    for (size_t i = 0; i < count; i++) {
        const char *name = files[i].name;
        size_t nlen = strlen(name);
        if (nlen > IWF_MAX_NAME_LEN) {
            fprintf(stderr, "Entry '%s' exceeds %d ASCII characters\n",
                    name, IWF_MAX_NAME_LEN);
            fclose(f);
            free(offsets);
            return 1;
        }
        char name_buf[32];
        memset(name_buf, 0, sizeof(name_buf));
        memcpy(name_buf, name, nlen);
        fwrite(name_buf, 1, 32, f);

        write_u32_le(f, offsets[i]);
        write_u32_le(f, (uint32_t)files[i].size);
    }

    for (size_t i = 0; i < count; i++) {
        if (files[i].size > 0) {
            if (fwrite(files[i].data, 1, files[i].size, f) != files[i].size) {
                fprintf(stderr, "Failed to write data for '%s'\n", files[i].name);
                fclose(f);
                free(offsets);
                return 1;
            }
        }
    }

    fclose(f);
    free(offsets);
    return 0;
}

int iwf_pack(const char *input_dir, const char *output_iwf)
{
    size_t count = 0;
    IwfFileEntry *files = iwf_collect_files(input_dir, &count);
    if (!files) {
        fprintf(stderr, "Failed to collect files from '%s'\n", input_dir);
        return 1;
    }

    int rc = iwf_build(files, count, output_iwf);

    for (size_t i = 0; i < count; i++) {
        free(files[i].name);
        free(files[i].data);
    }
    free(files);

    if (rc == 0)
        printf("Packed %zu files -> %s\n", count, output_iwf);

    return rc;
}

#ifndef IWF_NO_MAIN
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_dir> <output_iwf>\n", argv[0]);
        return 1;
    }
    return iwf_pack(argv[1], argv[2]);
}
#endif