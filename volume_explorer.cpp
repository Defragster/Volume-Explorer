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

#include "volume_explorer.h"
#include "xmodem.h"
#include "re.h"

void VolumeExplorer::exec_command(char const *buf) {
    char tokens[VOLUME_EXPLORER_TOKENS_BUF_SIZE];
    char *token_ptrs[VOLUME_EXPLORER_MAX_TOKENS];
    char *token_pos;
    uint8_t token_count;
    int pos = 0, prev_pos = 0, last_pos;

    bzero(tokens, VOLUME_EXPLORER_TOKENS_BUF_SIZE);
    bzero(token_ptrs, VOLUME_EXPLORER_MAX_TOKENS * sizeof(char *));
    for (last_pos = strlen(buf); buf[last_pos] == ' '; last_pos--)
        ;
    token_pos = &tokens[0];
    token_count = 0;
    while (pos <= last_pos) {
        if (buf[pos] == 32 || buf[pos] == 0) {
            int l = pos - prev_pos;
            strncpy(token_pos, &buf[prev_pos], l);
            token_ptrs[token_count] = token_pos;
            token_count++;
            if (token_count == VOLUME_EXPLORER_MAX_TOKENS) {
                term->println("Too many tokens");
                break;
            }
            token_pos += l;
            *(token_pos++) = '\0';
            if (buf[pos] == 0)
                break;
            while (buf[++pos] == 32)
                ;
            prev_pos = pos;
        } else
            pos++;
    }

    /*
        for (int i = 0; i < token_count; i++) {
            term->printf("[%s]\n", token_ptrs[i]);
        }
    */

    command_t *cmd = NULL;
    for (size_t i = 0; i < sizeof(cmds) / sizeof(command_t); i++) {
        if (stricmp(cmds[i].cmd, token_ptrs[0]) == 0) {
            cmd = &cmds[i];
            break;
        }
    }

    if (cmd) {
        if (token_count - 1 != cmd->prms) {
            error("wrong number of params");
        } else {
            switch (cmd->id) {
                case CMD_CD:
                    cmd_cd(token_ptrs[1]);
                    break;
                case CMD_LS:
                    cmd_ls();
                    break;
                case CMD_MKDIR:
                    cmd_mkdir(token_ptrs[1]);
                    break;
                case CMD_RM:
                    cmd_rm(token_ptrs[1]);
                    break;
                case CMD_MV:
                    cmd_mv(token_ptrs[1], token_ptrs[2]);
                case CMD_CP:
                    cmd_cp(token_ptrs[1], token_ptrs[2]);
                    break;
                case CMD_RMDIR:
                    cmd_rmdir(token_ptrs[1]);
                    break;
                case CMD_DUMP:
                    cmd_dump(token_ptrs[1]);
                    break;
                case CMD_CAT:
                    cmd_cat(token_ptrs[1]);
                    break;
                case CMD_TOUCH:
                    cmd_touch(token_ptrs[1]);
                    break;
#ifdef VOLUME_EXPLORER_XMODEM_ENABLE
                case CMD_RECV:
                    cmd_recv(token_ptrs[1]);
                    break;
                case CMD_SEND:
                    cmd_send(token_ptrs[1]);
                    break;
#endif
#ifdef VOLUME_EXPLORER_XMODEM_ENABLE
#ifdef VOLUME_EXPLORER_XMODEM_DEBUG
                case CMD_DBUG:
                    cmd_dbug();
                    break;
#endif
#endif
            }
        }
    } else {
        error("unknow command [%s]", token_ptrs[0]);
    }
    prompt();
}

void VolumeExplorer::update() {
    uint8_t input;

    if (stopped)
        return;
    if (term->available() > 0) {
        input = term->read();
        switch (input) {
            case VEX_KEY_ENTER:
                if (input_buf_index > 0) {
                    term->println();
                    input_buf[input_buf_index] = 0;
                    exec_command(input_buf);
                    input_buf_index = 0;
                    bzero(input_buf, VOLUME_EXPLORER_CMD_BUFSIZE);
                }
                break;
            case VEX_KEY_DEL:
                if (input_buf_index > 0) {
                    input_buf_index--;
                    input_buf[input_buf_index] = 0;
#ifdef VOLUME_EXPLORER_USE_ANSI_CODES
                    term->print("\u001b[1D \u001b[1D"); // send ansi cursor backward
#endif
                }
                break;
            case VEX_KEY_CTRL_Q:
                term->println("bye!");
                stopped = true;
                return;
                break;
            default:
                if (input >= 32 && input <= 127) {
                    term->write(input);
                    input_buf[input_buf_index++] = input;
                }
                break;
        }
    }
}

bool VolumeExplorer::file_match(char const *filename, char const *pattern) {
    char b[256];
    int b_pos = 0, p_pos = 0;
    char c;

    while (pattern[p_pos] != 0 && b_pos < 255) {
        c = pattern[p_pos];
        switch (c) {
            case '.':
                b[b_pos++] = '\\';
                break;
            case '*':
                b[b_pos++] = '.';
                break;
        }
        b[b_pos++] = c;
        p_pos++;
    }
    b[b_pos] = 0;
    return re_match(b, filename) == -1 ? false : true;
}

void VolumeExplorer::cmd_cd(char const *new_path) {
    char b[VOLUME_EXPLORER_PATH_LEN];

    expand_path(new_path, b);
    if (sd.exists(b)) {
        strcpy(path, b);
    } else
        error("%s not found", b);
}

void VolumeExplorer::cmd_ls() {
    VolumeExplorerDir dir;
    SdFile entry;

    if (dir.open(path)) {
        while (dir.has_entry()) {
            entry = dir.next_entry();
            if (entry.isDir()) {
                term->printf("   D %-16s", dir.entry_name());
            } else {
                term->printf("   F %-16s %i", dir.entry_name(), entry.fileSize()); //, entry.size());
            }
            term->println();
        }
    } else
        error("dir not found");
}

void VolumeExplorer::cmd_rm(char const *pathname) {
    char b1[VOLUME_EXPLORER_PATH_LEN];
    char b2[VOLUME_EXPLORER_PATH_LEN];
    VolumeExplorerDir dir;
    SdFile entry;

    expand_path(pathname, b1);
    if (has_wildcards(pathname) && !sure())
        return;
    if (has_wildcards(pathname) && dir.open(path)) {
        while (dir.has_entry()) {
            entry = dir.next_entry();
            if (entry.isDir())
                continue;
            expand_path(dir.entry_name(), b2);
            if (file_match(b2, b1)) {
                sd.remove(b2);
                if (noisy)
                    term->printf("deleted %s\n", b2);
            }
        }
    } else {
        if (!is_file(b1)) {
            error("%s is not a file", pathname);
            return;
        }
        if (sd.exists(b1)) {
            sd.remove(b1);
            if (noisy)
                term->printf("deleted %s\n", b1);
        } else
            error("file not found");
    }
}

void VolumeExplorer::cmd_mv(char const *pathname, char const *new_pathname) {
    char b1[VOLUME_EXPLORER_PATH_LEN];
    char b2[VOLUME_EXPLORER_PATH_LEN];

    expand_path(pathname, b1);
    expand_path(new_pathname, b2);
    if (!sd.rename(b1, b2))
        error("Unable to rename %s to %s", b1, b2);
}

void VolumeExplorer::cmd_cp(char const *src_filename, char const *dst_filename) {
    char src[VOLUME_EXPLORER_PATH_LEN];
    char dst[VOLUME_EXPLORER_PATH_LEN];
    char base[VOLUME_EXPLORER_PATH_LEN];
    char src_file[VOLUME_EXPLORER_PATH_LEN];
    char dst_file[VOLUME_EXPLORER_PATH_LEN];
    VolumeExplorerDir dir;
    SdFile entry;

    expand_path(src_filename, src);
    expand_path(dst_filename, dst);
    if (is_file(dst)) {
        if (file_copy(src, dst))
            term->printf("file %s copied to %s\n", src, dst);
    } else {
        base_path(src, base);
        if (dir.open(base)) {
            while (dir.has_entry()) {
                entry = dir.next_entry();
                if (entry.isDir())
                    continue;
                strcpy(src_file, base);
                strcat(src_file, "/");
                strcat(src_file, dir.entry_name());
                if (file_match(src_file, src)) {
                    strcpy(dst_file, dst);
                    strcat(dst_file, "/");
                    strcat(dst_file, dir.entry_name());
                    if (file_copy(src_file, dst_file))
                        term->printf("file %s copied to %s\n", src_file, dst_file);
                }
            }
        } else
            error("dir %s not found", src);
    }
}

void VolumeExplorer::cmd_mkdir(char const *pathname) {
    char b[VOLUME_EXPLORER_PATH_LEN];

    expand_path(pathname, b);
    if (!sd.mkdir(b))
        error("unable to create dir %s", b);
}

void VolumeExplorer::cmd_rmdir(char const *pathname) {
    char b[VOLUME_EXPLORER_PATH_LEN];

    expand_path(pathname, b);
    if (dir_size(b) > 0)
        error("directory is not empty");
    else {
        sd.rmdir(b);
        term->printf("dir %s deleted\n", b);
    }
}

void VolumeExplorer::cmd_dump(char const *filename) {
    int n, offset = 0;
    SdFile file;
    char b[VOLUME_EXPLORER_PATH_LEN];
    char fbuf[8];

    expand_path(filename, b);
    if (file.open(b, O_RDONLY)) {
        while ((n = file.read(fbuf, 8)) > 0) {
            term->printf("%08X : ", offset);
            for (int i = 0; i < n; i++) {
                term->printf("%02X ", fbuf[i]);
            }
            for (int i = n; i < 8; i++)
                term->printf("   ");
            term->printf("  ");
            for (int i = 0; i < n; i++) {
                if (fbuf[i] < 32 or fbuf[i] >= 127)
                    fbuf[i] = '.';
                term->printf("%c", fbuf[i]);
            }
            term->printf("\n");
            offset += n;
        }
        file.close();
    } else {
        error("%s not found", filename);
    }
}

void VolumeExplorer::cmd_cat(char const *filename) {
    int n;
    SdFile file;
    char b[VOLUME_EXPLORER_PATH_LEN];
    char fbuf[8];

    expand_path(filename, b);
    if (file.open(b, O_RDONLY)) {
        while ((n = file.fgets(fbuf, 8)) > 0) {
            term->write(fbuf, n);
        }
        file.close();
    } else {
        error("%s not found", filename);
    }
}

void VolumeExplorer::cmd_touch(char const *filename) {
    char b[VOLUME_EXPLORER_PATH_LEN];
    SdFile file;

    expand_path(filename, b);
    if (!file.open(b, O_WRITE | O_CREAT | O_EXCL))
        error("unable to touch file %s", filename);
    else
        file.close();
}

// sx -vv commands.cpp > /dev/cu.usbmodem3955991 < /dev/cu.usbmodem3955991
void VolumeExplorer::cmd_recv(char const *filename) {
    VexXModem xmodem(term);
    char b[VOLUME_EXPLORER_PATH_LEN];
    SdFile file;

    expand_path(filename, b);
    if (file.open(b, O_WRITE | O_CREAT | O_TRUNC)) {
        term->printf("ready to receive to file %s - disconnect from terminal and launch sx foo.txt > /dev/your_device < /dev/your_device from command line\n",
                     b);
        xmodem.enable_fast_write(true);
        xmodem.receive(file);
        file.close();
    } else
        error("unable to recv to %s", b);
}

void VolumeExplorer::cmd_send(char const *filename) {
    VexXModem xmodem(term);
    char b[VOLUME_EXPLORER_PATH_LEN];
    SdFile file;

    expand_path(filename, b);
    if (file.open(b, O_READ)) {
        term->printf("ready to send to file %s - disconnect from terminal and launch rx foo.txt > /dev/your_device < /dev/your_device from command line\n", b);
        xmodem.enable_fast_write(true);
        xmodem.send(file);
        file.close();
    } else
        error("unable to send to %s", b);
}

void VolumeExplorer::cmd_dbug() {
    uint32_t m;
    uint8_t c;
    uint8_t v;
    SdFile file;
    int bcount = 0;
    int icount = 0;
    char last_flag = 'O';

    if (file.open("xmodem", O_READ)) {
        while (file.available()) {
            file.read(&m, sizeof(int));
            c = file.read();
            v = file.read();
            term->printf("%-4u %-6u %-3u %c %02X\n", bcount, icount, m, c, v);
            bcount++;
            if (last_flag != c) {
                last_flag = c;
                icount = 0;
            } else
                icount++;
        }
    }
}
