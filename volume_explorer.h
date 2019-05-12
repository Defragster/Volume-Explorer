/*

    SdFat Volume Explorer

    Copyright (C) 2019 TACTIF CIE <www.tactif.com> / Bordeaux - France
    Author Christophe Gimenez <christophe.gimenez@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>

*/

#ifndef VOLUME_EXPLORER_H
#define VOLUME_EXPLORER_H

#include <Arduino.h>
#include <SdFat.h>

#define VOLUME_EXPLORER_USE_ANSI_CODES
#define VOLUME_EXPLORER_XMODEM_ENABLE
#define VOLUME_EXPLORER_XMODEM_DEBUG

#define VOLUME_EXPLORER_PATH_LEN 256
#define VOLUME_EXPLORER_FILENAME_LEN 32
#define VOLUME_EXPLORER_CMD_BUFSIZE 256
#define VOLUME_EXPLORER_TOKENS_BUF_SIZE 256
#define VOLUME_EXPLORER_MAX_TOKENS 8

class VolumeExplorerDir {
    FatFile dir;
    SdFile entry;
    char buf[VOLUME_EXPLORER_PATH_LEN];

  public:
    bool open(char const *pathname) {
        bool b = dir.open(pathname, O_READ);
        if (!b)
            Serial.printf("Can't open dir %s", pathname);
        return b;
    }
    bool has_entry() {
        if (entry.isOpen())
            entry.close();
        return entry.openNext(&dir, O_READ);
    }
    SdFile &next_entry() {
        entry.getName(buf, VOLUME_EXPLORER_FILENAME_LEN);
        return entry;
    }
    char *entry_name() {
        return buf;
    }
    ~VolumeExplorerDir() {
        dir.close();
        entry.close();
    }
};

class VolumeExplorer {
    Sd2Card card;
    SdFatSdio sd;

    char path[VOLUME_EXPLORER_PATH_LEN] = {0};
    Stream *term;

    char input_buf[VOLUME_EXPLORER_CMD_BUFSIZE] = {0};
    int input_buf_index;

    bool noisy = true;
    bool stopped = false;

    enum { VEX_KEY_ENTER = 13, VEX_KEY_DEL = 127, VEX_KEY_CTRL_Q = 17 };

    typedef struct {
        int id;
        char cmd[6];
        uint8_t prms;
    } command_t;

    enum cmd_id { CMD_LS, CMD_CD, CMD_MKDIR, CMD_RM, CMD_MV, CMD_CP, CMD_RMDIR, CMD_DUMP, CMD_CAT, CMD_TOUCH, CMD_RECV, CMD_SEND, CMD_DBUG };

    command_t cmds[13] = {
        {.id = CMD_LS, .cmd = "ls", .prms = 0},
        {.id = CMD_CD, .cmd = "cd", .prms = 1},
        {.id = CMD_RM, .cmd = "rm", .prms = 1},
        {.id = CMD_MV, .cmd = "mv", .prms = 2},
        {.id = CMD_CP, .cmd = "cp", .prms = 2},
        {.id = CMD_MKDIR, .cmd = "mkdir", .prms = 1},
        {.id = CMD_RMDIR, .cmd = "rmdir", .prms = 1},
        {.id = CMD_DUMP, .cmd = "dump", .prms = 1},
        {.id = CMD_CAT, .cmd = "cat", .prms = 1},
        {.id = CMD_TOUCH, .cmd = "touch", .prms = 1},
        {.id = CMD_RECV, .cmd = "recv", .prms = 1},
        {.id = CMD_SEND, .cmd = "send", .prms = 1},
        {.id = CMD_DBUG, .cmd = "dbug", .prms = 0}, // Internal use for xmodem debugging
    };

  private:
    void prompt() {
        term->printf("#%s:", path);
    }

    void error(const char *format, ...) {
        char sbuf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(sbuf, 256, format, args);
        va_end(args);
        term->printf("error : %s\n", sbuf);
    }

    void base_path(char const *pathname, char *base) {
        int i;
        for (i = strlen(pathname); pathname[i] != '/'; i--)
            ;
        memcpy(base, pathname, i);
        base[i] = 0;
    }

    void expand_path(char const *pathname, char *base) {
        char tmp[VOLUME_EXPLORER_PATH_LEN];
        char *b_pos, *t_pos;

        if (is_absolute(pathname)) {
            strcpy(tmp, pathname);
        } else {
            strcpy(tmp, path);
            if (strlen(path) > 1)
                strcat(tmp, "/");
            strcat(tmp, pathname);
        }
        t_pos = tmp;
        b_pos = base;
        while (*(t_pos) != '\0') {
            uint16_t ddot = *(uint16_t *)(t_pos);
            if (ddot == 0x2E2E) { // ..
                if (b_pos - base > 1) {
                    b_pos -= 2;
                    while (*(--b_pos - 1) != '/')
                        ;
                } else
                    b_pos = base + 1;
                t_pos += 2;
            } else
                *(b_pos++) = *(t_pos);
            t_pos++;
        }
        *(b_pos) = '\0';
    }

    bool file_match(char const *filename, char const *pattern);

    bool has_wildcards(char const *filename) {
        for (unsigned int i = 0; i < strlen(filename); i++)
            if (filename[i] == '*')
                return true;
        return false;
    }

    bool file_copy(char const *src_path, char const *dst_path) {
        File src, dst;
        char fbuf[4096];
        int n;

        if (!src.open(src_path, O_READ)) {
            error("can't open %s for reading", src_path);
            return false;
        }
        if (!dst.open(dst_path, O_WRITE | O_CREAT | O_EXCL)) {
            error("can't open %s for writing", dst_path);
            return false;
        }
        while ((n = src.read(fbuf, 4096)) > 0) {
            dst.write(fbuf, n);
        }
        src.close();
        dst.close();
        return true;
    }

    bool is_valid(char const *pathname) {
        File f;
        bool r;
        f.open(pathname, O_READ);
        r = f;
        f.close();
        return r;
    }

    bool is_dir(char const *pathname) {
        File f;
        bool r;
        f.open(pathname, O_READ);
        r = f.isDir();
        f.close();
        return r;
    }

    bool is_file(char const *pathname) {
        return !is_dir(pathname);
    }

    bool is_root() {
        return strcmp(path, "/") == 0;
    }

    bool is_relative(char const *pathname) {
        return pathname[0] != '/';
    }

    bool is_absolute(char const *pathname) {
        return !is_relative(pathname);
    }

    int dir_size(char const *pathname) {
        SdFile entry;
        FatFile dir;
        int count = 0;
        if (dir.open(pathname, O_READ)) {
            while (entry.openNext(&dir, O_READ)) {
                count++;
                entry.close();
            }
            dir.close();
        } else
            return -1;
        return count;
    }

    bool sure() {
        term->print("are you sure ? [Yy/Nn] :");
        while (term->available() == 0)
            ;
        int r = term->read();
        term->println();
        return r == 'Y' || r == 'y';
    }

  public:
    VolumeExplorer(Stream *_t) : term(_t) {
    }
    void init() {
        if (!sd.begin()) {
            term->println("sd begin error");
        }
        input_buf_index = 0;
        strcat(path, "/");
        cmd_ls();
        prompt();
    }

    void update();
    void exec_command(char const *buf);

    void cmd_cd(char const *new_path);
    void cmd_mkdir(char const *pathname);
    void cmd_rmdir(char const *pathname);
    void cmd_ls();
    void cmd_rm(char const *filename);
    void cmd_mv(char const *filename, char const *new_pathname);
    void cmd_cp(char const *src_filename, char const *dst_filename);
    void cmd_dump(char const *filename);
    void cmd_cat(char const *filename);
    void cmd_touch(char const *filename);
    void cmd_recv(char const *filename);
    void cmd_send(char const *filename);
    void cmd_dbug();
};

#endif
