#ifndef VOLUME_EXPLORER_XMODEM_H
#define VOLUME_EXPLORER_XMODEM_H

#include <Arduino.h>
#include <SdFat.h>

#define XMODEM_BLOCK_SIZE 128

#define XM_SOH 0x01
#define XM_STX 0x02 // Marker for 1K Blocks
#define XM_EOT 0x04
#define XM_ACK 0x06
#define XM_NAK 0x15
#define XM_CAN 0x18

class VexXModem {
    Stream *s;
    uint8_t packet_num;
    SdFile log;

    enum State { WAIT_CMD, WAIT_DATA };
    State state;

    enum ChecksumMethod { BIT8, CRC16 };
    ChecksumMethod checksum_method;

    bool fast_write;
    bool log_enabled;

    uint8_t in(int time_out = 1000) {
        while (true)
            if (s->available() > 0) {
                uint8_t v = s->read();
                write_log(v, 'I');
                return v;
            }
    }
    void out(uint8_t v) {
        s->write(v);
        Serial.send_now();
        s->flush();
        write_log(v, 'O');
    }
    void out(uint8_t *b, int size) {
        if (fast_write) {
            s->write(b, size);
            s->flush();
        } else {
            for (int i = 0; i < size; i++)
                out(b[i]);
        }
    }

    void write_log(uint8_t v, char t) {
        if (!log_enabled)
            return;
        uint32_t m = millis();
        log.write(&m, 4);
        log.write(t);
        log.write(v);
    }

  public:
    VexXModem(Stream *_s) : s(_s) {
        if (log_enabled)
            log.open("xmodem.log", O_WRITE | O_CREAT | O_TRUNC | O_SYNC);
        fast_write = false;
        log_enabled = false;
        checksum_method = BIT8;
    }
    ~VexXModem() {
        if (log_enabled)
            log.close();
    }
    void receive(SdFile &file);
    void send(SdFile &file);
    bool packet_valid(uint8_t *buf);
    void send_packet(uint8_t *buf, uint16_t size);
    void enable_fast_write(bool v) {
        fast_write = v;
    }
    void enable_log(bool v) {
        log_enabled = v;
    }
};

#endif