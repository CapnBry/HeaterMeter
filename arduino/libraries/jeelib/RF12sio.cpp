// Streaming I/O layer on top of RF12 driver
// 2009-05-07 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>
#include <RF12sio.h>
#if ARDUINO>=100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

#define DEBUG 0

void RF12::initBuf(uint8_t base) {
    txbase = base;
    txfill = base + 8; // at most 15 fields
    txfields = 0;
    txbuf[txbase] = 0;
}

void RF12::addToBuf(uint8_t type, const void *ptr, uint8_t len) {
    if (txfields < 15) {
        uint8_t* p = txbuf + txbase + (++txfields >> 1);
        if (txfields & 1)
            *p |= type << 4;
        else
            *p = type;
    
        memcpy(txbuf + txfill, ptr, len);
        txfill += len;
    }
}

RF12& RF12::put(const void* ptr, char len) {
    char type = len;
    if (len < 0)
        if (len == F_STR)
            len = strlen((const char*) ptr) + 1;
        else
            len = 1 << ~len;
    else if (len > 7) {
        put(&len, F_1);
        type = F_EXT;
    }
    
    addToBuf(type & 0x0F, ptr, len);
    return *this;
}

uint8_t RF12::nextSize() {
    uint8_t type = rf12_buf[2 + (rxfield>>1)];
    if (rxfield & 1)
        type >>= 4;
    else
        type &= 0x0F;

    switch (type) {
        case F_1 & 0x0F:   return 1;
        case F_2 & 0x0F:   return 2;
        case F_4 & 0x0F:   return 4;
        case F_8 & 0x0F:   return 8;
        case F_STR & 0x0F: return strlen((char*) rf12_buf + rxpos) + 1;
        case F_EXT & 0x0F: return rf12_buf[rxpos];
    }
    return type;
}

RF12& RF12::get(void* ptr, char len) {
    uint8_t size = nextSize();
    if (len < 0)
        if (len > F_STR) {
            len = 1 << ~len;
            if (len > size) {
                // this sign-extend logic only works on little-endian machines
                memset(ptr, rf12_buf[rxpos+size-1] >> 7, len);
                len = size;
            }
        } else
            len = size;
    else if (size > 7)
        ++rxpos;
    memcpy(ptr, (char*) rf12_buf + rxpos, len);
    rxpos += size;
    ++rxfield;
    return *this;
}

void RF12::send(uint8_t v) {
    // rearrange buf so packet contents is:
    //  - field nibbles
    //  - command byte
    //  - argument data
    // the first nibble is adjusted to contain the field count
    
    uint8_t args = txbase + 8; // at most 15 fields
    txbuf[txbase] |= txfields;
    txbase += (txfields >> 1) + 1;
    txbuf[txbase++] = v;
    memcpy(txbuf + txbase, txbuf + args, txfill - args);
    initBuf(txbase + (txfill - args));
    
    if (sendTimer.idle())
        sendTimer.set(3); // start sending within 3 msecs
}

uint8_t RF12::poll() {
    // TODO - check for more packets still pending in rf12_buf
    
    if (ackHead && rf12_canSend()) {
        // Serial.print(" -> ack ");
        // Serial.println(ackData, DEC);
        rf12_sendStart(ackHead, &ackData, 1);
        ackHead = 0;
        return 0;
    }
    
    if (rf12_recvDone() && rf12_crc == 0) {
        uint8_t len = rf12_len;
        
        if (DEBUG) {
            Serial.print("OK");
            for (uint8_t i = 0; i < len + 2 && i < 25; ++i) {
                if (i <= 2)
                    Serial.print(" # "[i]);
                Serial.print(rf12_buf[i] >> 4, HEX);
                Serial.print(rf12_buf[i] & 0x0F, HEX);
            }
            Serial.println();
        }
        
        if ((rf12_hdr & RF12_HDR_CTL) && len == 1) {
            // last send acked, move any remaining in-progress tx data to head
            uint8_t num = rf12_buf[2];
            txfill -= num;
            memcpy(txbuf, txbuf + num, txfill);
            txbase -= num;
            // FIXME what if txbase > 0 at this point?
            ackTimer.set(0);
            return 0;
        }

        if ((rf12_hdr & ~RF12_HDR_MASK) == RF12_HDR_ACK) {
            // save details to send an ack on next poll
            ackHead = RF12_HDR_CTL | RF12_HDR_DST | (rf12_hdr & RF12_HDR_MASK);
            ackData = len;
        }
        
        rxfield = 0;
        uint8_t nf = rf12_buf[2] & 0x0F;
        rf12_buf[2] |= 0x0F; // command code is returned as first 1-byte value
        rxpos = (nf >> 1) + 3;
        return nf + 1;
    }
    
    if (ackTimer.poll() && sendTimer.idle())
        sendTimer.set(500); // got no ack, schedule resend in 500 ms
    if (sendTimer.poll() && txbase > 0)
        txPending = 1;
        
    if (txPending && rf12_canSend()) {
        if (DEBUG) {
            Serial.print(" -> #");
            Serial.print(txbase, HEX);
            Serial.print(' ');
            for (uint8_t i = 0; i < txbase; ++i) {
                Serial.print(txbuf[i] >> 4, HEX);
                Serial.print(txbuf[i] & 0x0F, HEX);
            }
            Serial.println();
        }
        
        txPending = 0;
        rf12_sendStart(RF12_HDR_ACK, txbuf, txbase); // send data, request ack
        ackTimer.set(50); // send again if no ack within 50 ms
    }
    
    return 0;
}
