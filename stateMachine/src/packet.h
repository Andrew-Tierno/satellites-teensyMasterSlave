#ifndef PACKET_H
#define PACKET_H
#include <t3spi.h>
#include <modules.h>

//Initialize T3SPI class as SPI_SLAVE
extern T3SPI SPI_SLAVE;
extern volatile uint16_t transmissionSize;
extern volatile bool transmitting;
extern volatile unsigned int packetsReceived;

#define SLAVE_CHIP_SELECT 31
// Always have first word in packet be constant for alignment checking
#define FIRST_WORD 0x1234
// Always have last word in packet be constant for alignment checking
#define LAST_WORD 0x4321
// 0xabcd is more useful than 0 because it indicates payload is alive
#define EMPTY_WORD 0xabcd

#define PACKET_SIZE 10 // Fixed size incoming packet
// Index of first word in packet body; word 0 is FIRST_WORD
#define PACKET_BODY_BEGIN 1 // First word in body is command number
// Exclusive; last two words are checksum and LAST_WORD
#define PACKET_BODY_END (PACKET_SIZE - 2)
#define BODY_LENGTH (PACKET_BODY_END - PACKET_BODY_BEGIN)
#define PACKET_OVERHEAD (PACKET_SIZE - BODY_LENGTH)

#define ABCD_BUFFER_SIZE 2
#define OUT_PACKET_BODY_BEGIN 15
#define OUT_PACKET_BODY_END_SIZE 2
#define OUT_PACKET_OVERHEAD (OUT_PACKET_BODY_END_SIZE + OUT_PACKET_BODY_BEGIN)

// Commands
#define MIN_COMMAND 0
#define COMMAND_ECHO 0
#define COMMAND_STATUS 1
#define COMMAND_IDLE 2
#define COMMAND_SHUTDOWN 3
#define COMMAND_IMU 4
#define COMMAND_IMU_DUMP 5
#define COMMAND_CALIBRATE 6
#define COMMAND_POINT_TRACK 7
#define COMMAND_REPORT_TRACKING 8
#define COMMAND_PROBE_MEMORY 9
#define MAX_COMMAND 9

// Response Headers
#define MIN_HEADER 0
#define RESPONSE_OK 0
#define RESPONSE_BAD_PACKET 1
#define RESPONSE_PID_DATA 3
#define RESPONSE_ADCS_REQUEST 4
#define RESPONSE_PROBE 5
#define MAX_HEADER 5

// Error numbers
#define INVALID_BORDER 0
#define INVALID_CHECKSUM 1
#define INTERNAL_ERROR 2
#define INVALID_COMMAND 3
#define DATA_NOT_READY 4
#define STATE_NOT_READY 5

void packet_setup(void);

#endif
