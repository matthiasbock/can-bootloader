#include <stdlib.h>
#include <string.h>
#include "can_datagram.h"
#include <crc/crc32.h>

enum {
    STATE_PROTOCOL_VERSION,
    STATE_CRC,
    STATE_DST_LEN,
    STATE_DST,
    STATE_DATA_LEN,
    STATE_DATA,
    STATE_TRAILING,
};

void can_datagram_init(can_datagram_t *dt)
{
    memset(dt, 0, sizeof *dt);
    dt->protocol_version = CAN_DATAGRAM_VERSION;
}

void can_datagram_set_address_buffer(can_datagram_t *dt, uint8_t *buf)
{
    dt->destination_nodes = buf;
}

void can_datagram_set_data_buffer(can_datagram_t *dt, uint8_t *buf, size_t buf_size)
{
    dt->data = buf;
    dt->_data_buffer_size = buf_size;
}

void can_datagram_input_byte(can_datagram_t *dt, uint8_t val)
{
    switch (dt->_reader_state) {
        case STATE_PROTOCOL_VERSION:
            dt->protocol_version = val;
            dt->_reader_state = STATE_CRC;
            break;

        case STATE_CRC:
            dt->crc = (dt->crc << 8) | val;
            dt->_crc_bytes_read ++;

            if (dt->_crc_bytes_read >= 4) {
                dt->_reader_state = STATE_DST_LEN;
            }
            break;

        case STATE_DST_LEN: /* Destination nodes list length */
            dt->destination_nodes_len = val;
            dt->_reader_state = STATE_DST;
            break;

        case STATE_DST: /* Destination nodes */
            dt->destination_nodes[dt->_destination_nodes_read] = val;
            dt->_destination_nodes_read ++;

            if (dt->_destination_nodes_read >= dt->destination_nodes_len) {
                dt->_reader_state = STATE_DATA_LEN;
            }
            break;

        case STATE_DATA_LEN: /* Data length, MSB */
            dt->data_len = (dt->data_len << 8) | val;
            dt->_data_length_bytes_read ++;

            if (dt->_data_length_bytes_read >= 4) {
                dt->_reader_state = STATE_DATA;
            }

            break;


        case STATE_DATA: /* Data */
            dt->data[dt->_data_bytes_read] = val;
            dt->_data_bytes_read ++;

            if (dt->_data_bytes_read >= dt->data_len) {
                // Received the announced number of bytes
                dt->_reader_state = STATE_TRAILING;
            }

            if (dt->_data_bytes_read >= dt->_data_buffer_size) {
                // Input buffer overflow
                dt->_reader_state = STATE_TRAILING;
            }

            break;

        default:
            // Don't change the reader state and drop trailing data
            break;
    }
}

bool can_datagram_is_complete(can_datagram_t *dt)
{
    return (dt->_reader_state == STATE_TRAILING);
}

bool can_datagram_is_valid(can_datagram_t *dt)
{
    return (can_datagram_is_complete(dt))
        && (can_datagram_compute_crc(dt) == dt->crc)
        && (dt->protocol_version == CAN_DATAGRAM_VERSION);
}

void can_datagram_start(can_datagram_t *dt)
{
    dt->_reader_state = STATE_PROTOCOL_VERSION;
    dt->crc = 0x11223344;
    dt->_crc_bytes_read = 0;
    dt->_destination_nodes_read = 0;
    dt->_data_length_bytes_read = 0;
    dt->_data_bytes_read = 0;
}

int can_datagram_output_bytes(can_datagram_t *dt, char *buffer, size_t buffer_len)
{
    size_t i;
    for (i = 0; i < buffer_len; i++ ) {
        switch (dt->_writer_state) {
            case STATE_PROTOCOL_VERSION:
                buffer[i] = dt->protocol_version;
                dt->_writer_state = STATE_CRC;
                dt->_crc_bytes_written = 0;
                break;

            case STATE_CRC:
                buffer[i] = dt->crc >> (24 - 8 * dt->_crc_bytes_written);
                dt->_crc_bytes_written ++;

                if (dt->_crc_bytes_written >= 4) {
                    dt->_writer_state = STATE_DST_LEN;
                }
                break;

            case STATE_DST_LEN: /* Destination node length */
                buffer[i] = dt->destination_nodes_len;
                dt->_writer_state = STATE_DST;
                dt->_destination_nodes_written = 0;
                break;

            case STATE_DST: /* Destination nodes */
                buffer[i] = dt->destination_nodes[dt->_destination_nodes_written];
                dt->_destination_nodes_written ++;

                if (dt->_destination_nodes_written >= dt->destination_nodes_len) {
                    dt->_writer_state = STATE_DATA_LEN;
                    dt->_data_length_bytes_written = 0;
                }
                break;

            case STATE_DATA_LEN: /* Data length MSB first */
                buffer[i] = dt->data_len >> (24 - 8 * dt->_data_length_bytes_written);
                dt->_data_length_bytes_written ++;

                if (dt->_data_length_bytes_written >= 4) {
                    dt->_writer_state = STATE_DATA;
                    dt->_data_bytes_written = 0;
                }
                break;

            case STATE_DATA: /* Data */
                /* If already finished, proceed to trailing state and return. */
                if (dt->_data_bytes_written >= dt->data_len) {
                    dt->_writer_state = STATE_TRAILING;
                    return i;
                }

                buffer[i] = dt->data[dt->_data_bytes_written];
                dt->_data_bytes_written ++;
                break;

            case STATE_TRAILING:
                /* Datagram has been written completely. */
                return i;
                break;

            default:
                /* This point should never be reached. */
                return i;
                break;
        }
    }

    return buffer_len;
}

uint32_t can_datagram_compute_crc(can_datagram_t *dt)
{
    uint32_t crc;
    uint8_t tmp[4];
    crc = crc32(0, &dt->destination_nodes_len, 1);
    crc = crc32(crc, &dt->destination_nodes[0], dt->destination_nodes_len);

    /* data_len is not in network endianess, correct that before CRC update. */
    tmp[0] = (dt->data_len >> 24) & 0xff;
    tmp[1] = (dt->data_len >> 16) & 0xff;
    tmp[2] = (dt->data_len >> 8) & 0xff;
    tmp[3] = (dt->data_len >> 0) & 0xff;

    crc = crc32(crc, tmp, 4);
    crc = crc32(crc, &dt->data[0], dt->data_len);
    return crc;
}

bool can_datagram_id_start_is_set(unsigned int id)
{
    return id & ID_START_MASK;
}
