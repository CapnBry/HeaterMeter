// Ports library remote interface to the RF12 wireless radio
// 2009-02-14 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <JeeLib.h>

void RemoteHandler::setup(uint8_t id, uint8_t band, uint8_t group) {
    rf12_config();
}

uint8_t RemoteHandler::poll(RemoteNode& node, uint8_t send) {
    if (rf12_recvDone() && rf12_crc == 0 && rf12_len == sizeof node.data)
        memcpy(&node.data, (void*) rf12_data, sizeof node.data);

    if (send && rf12_canSend()) {
        rf12_sendStart(RF12_HDR_ACK, &node.data, sizeof node.data);
        return 1;
    }

    return 0;
}
