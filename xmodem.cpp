#include "xmodem.h"

bool VexXModem::packet_valid(uint8_t *buf) {
    if (checksum_method == BIT8) {
        uint8_t checksum;
        int c = 0;
        checksum = buf[XMODEM_BLOCK_SIZE];
        for (int i = 0; i < XMODEM_BLOCK_SIZE; i++)
            c += buf[i];
        c &= 0xFF;
        return checksum == c;
    } else
        return true;
}

void VexXModem::send_packet(uint8_t *buf, uint16_t size) {
    out(XM_SOH);
    out(packet_num);
    out(uint8_t(0xFF) - packet_num);
    if (size < XMODEM_BLOCK_SIZE) {
        for (int i = size; i < XMODEM_BLOCK_SIZE; i++)
            buf[i] = 0x1A; // Padding
    }
    out(buf, XMODEM_BLOCK_SIZE);
    int c = 0;
    for (int i = 0; i < XMODEM_BLOCK_SIZE; i++)
        c += buf[i];
    c &= 0xFF;
    out(c);
}

// https://fieldeffect.info/wp/frontpage/xmodem/
void VexXModem::receive(SdFile &file) {
    uint8_t buf[XMODEM_BLOCK_SIZE + 1]; // for checksum
    uint16_t buf_index;
    uint8_t pck_number;
    uint8_t pck_comp;
    bool write_packet;

    char init_com_char = checksum_method == BIT8 ? XM_NAK : 'C';
    out(init_com_char);

    uint32_t start_time = millis();
    while (s->available() == 0) {
        if (millis() - start_time > 2000) {
            out(init_com_char);
            start_time = millis();
        }
    }

    buf_index = 0;
    packet_num = 1;
    write_packet = false;
    bool cont = true;
    state = WAIT_CMD;
    while (cont) {
        uint8_t input = in();
        if (state == WAIT_CMD) {
            switch (input) {
                case XM_SOH:
                    if (write_packet && buf_index > 0) {
                        file.write(buf, XMODEM_BLOCK_SIZE);
                        file.sync();
                        write_packet = false;
                    }
                    pck_number = in();
                    pck_comp = in();

                    if (pck_number != packet_num || pck_number + pck_comp != 0xFF) {
                        out(XM_CAN);
                        out(XM_CAN);
                        out(XM_CAN);
                        file.write(buf, buf_index);
                        file.sync();
                        cont = false;
                    }

                    buf_index = 0;
                    state = WAIT_DATA;
                    break;
                case XM_EOT:
                    if (buf_index > 0) {
                        int i = XMODEM_BLOCK_SIZE - 1; // skip checksum
                        while (buf[i] == 0x1A)         // Was 0x1C (28) on CP/M
                            i--;
                        file.write(buf, i + 1);
                        file.sync();
                        cont = false;
                    }
                    out(XM_ACK);
                    break;
            }
        } else {
            buf[buf_index] = input;
            if (buf_index == XMODEM_BLOCK_SIZE) { // Checksum included
                if (packet_valid(buf)) {
                    write_packet = true;
                    out(XM_ACK);
                    packet_num++;
                } else {
                    buf_index = 0;
                    out(XM_NAK);
                }
                state = WAIT_CMD;
            } else
                buf_index++;
        }
    };
}

void VexXModem::send(SdFile &file) {
    uint8_t buf[XMODEM_BLOCK_SIZE];
    int n;
    bool must_send = false;

    packet_num = 0;
    while ((n = file.read(buf, XMODEM_BLOCK_SIZE)) > 0) {
        bool cont = true;
        packet_num++;
        while (cont) {
            if (must_send) {
                send_packet(buf, n);
                must_send = false;
            }
            uint8_t input = in();
            switch (input) {
                case XM_NAK:
                    must_send = true;
                    break;
                case XM_ACK:
                    cont = false;
                    must_send = true;
                    break;
            }
        }
    }
    out(XM_EOT);
    in(); // ACK
}
