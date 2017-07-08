#include "packet.h"
#include "main.h"
#include "states.h"
#include "pidData.h"

T3SPI SPI_SLAVE;

// Very little effort is made to prevent these from overflowing
// They will only be used for basic telemetry
volatile unsigned int packetsReceived = 0;
volatile unsigned int wordsReceived = 0;

// Incoming
volatile uint16_t packet[PACKET_SIZE + 10] = {}; // buffer just in case we overflow or something
volatile int packetPointer = 0;

// Outgoing
#define outBufferLength  300
volatile uint16_t outData[outBufferLength] = {};
volatile uint16_t *outBody = outData + OUT_PACKET_BODY_BEGIN;
volatile uint16_t *currentlyTransmittingPacket = outData; // One doesn't always have to supply outData as the packet buffer
volatile unsigned int outPointer = 0;
volatile uint16_t transmissionSize = 0;
volatile bool transmitting = false;

bool shouldClearSendBuffer = false;

// Local functions
void response_echo();
void response_status();
void responseBadPacket(uint16_t flag);
void create_response();
void responseImuDump();
void setupTransmission(uint16_t header, unsigned int bodyLength);
void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint16_t *packetBuffer);

void packet_setup(void) {
    assert(outPointer == 0);
    assert(transmissionSize == 0);
    assert(transmitting == false);
    SPI_SLAVE.begin_SLAVE(SCK1, MOSI1, MISO1, T3_SPI1_CS0);
    SPI_SLAVE.setCTAR_SLAVE(16, T3_SPI_MODE0);
    NVIC_ENABLE_IRQ(IRQ_SPI1);
    NVIC_SET_PRIORITY(IRQ_SPI1, 16);

    pinMode(PACKET_RECEIVED_TRIGGER, OUTPUT);
    digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);

    // Not sure what priority this should be; this shouldn't fire at the same time as IRQ_SPI0
    attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
    //attachInterrupt(PACKET_RECEIVED_PIN, handlePacket, FALLING);

    // Low priority for pin 26 -- packet received interrupt
    NVIC_SET_PRIORITY(IRQ_PORTE, 144);
}

uint16_t getHeader() {
    return currentlyTransmittingPacket[3];
}

//Interrupt Service Routine to handle incoming data
void spi1_isr(void) {
  assert(outPointer <= transmissionSize);
  assert(packetPointer <= PACKET_SIZE);
  assert(transmitting || outPointer == 0);
  wordsReceived++;
  uint16_t to_send = EMPTY_WORD;
  if (transmitting && outPointer < transmissionSize) {
    to_send = currentlyTransmittingPacket[outPointer];
    outPointer++;
    if (outPointer == transmissionSize && ((state == TRACKING_STATE) || (state == CALIBRATION_STATE)) && getHeader() == RESPONSE_PID_DATA) {
        assert(imuPacketReady);
        imuPacketSent();
    }
  }
  uint16_t received = SPI_SLAVE.rxtx16(to_send);
  if (packetPointer < PACKET_SIZE) {
    //debugPrintf("Received %x\n", received);
    packet[packetPointer] = received;
    packetPointer++;
    if (packetPointer == PACKET_SIZE) {
      packetReceived();
    }
  }
}

void packetReceived() {
  assert(packetPointer == PACKET_SIZE);
  packetsReceived++;

  handlePacket();
}

void handlePacket() {
  unsigned int startTimePacketPrepare = micros();
  //debugPrintln("Received a packet!");
  assert(packetPointer == PACKET_SIZE);

  // Check for erroneous data
  if (transmitting || outPointer != 0) {
    debugPrintln("Error: I'm already transmitting!");
    // No response because we're presumably already transmitting
    // responseBadPacket(INTERNAL_ERROR);
    return;
  }
  if (packetPointer != PACKET_SIZE) {
    debugPrintf("Received %d bytes, expected %d\n", packetPointer, PACKET_SIZE);
    responseBadPacket(INTERNAL_ERROR);
    return;
  }
  if (packet[0] != FIRST_WORD || packet[PACKET_SIZE - 1] != LAST_WORD) {
    debugPrintf("Invalid packet endings: start %x, end %x\n", packet[0], packet[PACKET_SIZE - 1]);
    responseBadPacket(INVALID_BORDER);
    return;
  }
  uint16_t receivedChecksum = 0;
  for (int i = PACKET_BODY_BEGIN; i < PACKET_BODY_END; i++) {
      receivedChecksum += packet[i];
  }
  if (receivedChecksum != packet[PACKET_SIZE - 2]) {
      responseBadPacket(INVALID_CHECKSUM);
      return;
  }

  unsigned int midTimePacketPrepare = micros();
  (void) midTimePacketPrepare;

  create_response();

  unsigned int endTimePacketPrepare = micros();
  //assert (endTimePacketPrepare - startTimePacketPrepare <= 1);
  if (endTimePacketPrepare - startTimePacketPrepare > 1) {
      debugPrintf("Packet took %d, %d micros\n", endTimePacketPrepare - startTimePacketPrepare, midTimePacketPrepare - startTimePacketPrepare);
  }
}

void create_response() {
    assert(packetPointer == PACKET_SIZE);
    assert(transmitting == false);
    uint16_t command = packet[1];
    if (changingState) {
        responseBadPacket(STATE_NOT_READY);
        return;
    }
    if (command == COMMAND_ECHO) {
        response_echo();
    } else if (command == COMMAND_STATUS) {
        response_status();
    } else if (command == COMMAND_SHUTDOWN) {
        previousState = state;
        changingState = true;
        state = SHUTDOWN_STATE;
        response_status();
    } else if (command == COMMAND_IDLE) {
        previousState = state;
        changingState = true;
        state = IDLE_STATE;
        response_status();
    } else if (command == COMMAND_POINT_TRACK) {
        previousState = state;
        changingState = true;
        state = TRACKING_STATE;
        response_status();
    } else if (command == COMMAND_CALIBRATE) {
        previousState = state;
        changingState = true;
        state = CALIBRATION_STATE;
        response_status();
    } else if (command == COMMAND_REPORT_TRACKING) {
        if (!(state == TRACKING_STATE || state == CALIBRATION_STATE)) {
            responseBadPacket(INVALID_COMMAND);
        } else if (!imuPacketReady) {
            debugPrintf("Front of buf %d back %d samples sent %d\n", imuDataPointer, imuSentDataPointer, imuSamplesSent);
            responseBadPacket(DATA_NOT_READY);
        } else {
            responseImuDump();
        }
    } else {
        responseBadPacket(INVALID_COMMAND);
    }
}

void clearSendBuffer() { // Just for debugging purposes
    if (DEBUG && shouldClearSendBuffer) {
        for (int i = 0; i < outBufferLength; i++) {
            outData[i] = 0xbeef;
        }
    }
}

void response_echo() {
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    int bodySize = 7;
    for (int i = 0; i < bodySize; i++) {
      if (i < PACKET_SIZE) {
        outBody[i] = packet[i];
      } else {
        outBody[i] = 0;
      }
    }
    assert(outBody[bodySize] == 0xbeef);
    setupTransmission(RESPONSE_OK, bodySize);
}

void write32(volatile uint16_t* buffer, unsigned int index, uint32_t item) {
    *((volatile uint32_t *) &buffer[index]) = item;
}

void response_status() {
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    int bodySize = 13;
    outBody[0] = state;
    write32(outBody, 1, packetsReceived);
    write32(outBody, 3, wordsReceived);
    write32(outBody, 5, timeAlive);
    write32(outBody, 7, lastLoopTime);
    write32(outBody, 9, maxLoopTime);
    write32(outBody, 11, errors);
    if (DEBUG && shouldClearSendBuffer) {
        assert(outBody[bodySize-1] != 0xbeef);
        assert(outBody[bodySize] == 0xbeef);
    }
    setupTransmission(RESPONSE_OK, bodySize);
}

void responseImuDump() {
    setupTransmissionWithChecksum(RESPONSE_PID_DATA, IMU_DATA_DUMP_SIZE * sizeof(pidSample) * 8 / 16, imuPacketChecksum, imuDumpPacket);
}

void responseBadPacket(uint16_t flag) {
    errors++;
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    debugPrintf("Bad packet: flag %d\n", flag);
    unsigned int bodySize = 4;
    outBody[0] = flag;
    for (unsigned int i = 1; i < bodySize; i++) {
        outBody[i] = 0;
    }
    assert(outBody[bodySize] == 0xbeef);
    setupTransmission(RESPONSE_BAD_PACKET, bodySize);
}

void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint16_t *packetBuffer) {
    assert(!transmitting);
    assert(header <= MAX_HEADER);
    assert(bodyLength <= 500);
    assert(bodyLength > 0);
    currentlyTransmittingPacket = packetBuffer;
    outPointer = 0;
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    packetBuffer[0] = FIRST_WORD;
    packetBuffer[1] = transmissionSize;
    packetBuffer[2] = ~transmissionSize;
    packetBuffer[3] = header;
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN] != 0xbeef);
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN + bodyLength - 1] != 0xbeef);
    uint16_t checksum = bodyChecksum + transmissionSize + header;
    packetBuffer[transmissionSize - 2] = checksum;
    packetBuffer[transmissionSize - 1] = LAST_WORD;
    //debugPrintf("Sending checksum: %x\n", checksum);
    transmitting = true;
}

// Call this after body of transmission is filled
void setupTransmissionWithBuffer(uint16_t header, unsigned int bodyLength, volatile uint16_t *packetBuffer) {
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    uint16_t bodyChecksum = transmissionSize + header;
    for (unsigned int i = OUT_PACKET_BODY_BEGIN; i < (unsigned int) (transmissionSize - OUT_PACKET_BODY_END_SIZE); i++) {
      bodyChecksum += packetBuffer[i];
    }
    setupTransmissionWithChecksum(header, bodyLength, bodyChecksum, packetBuffer);
}

void setupTransmission(uint16_t header, unsigned int bodyLength){
    setupTransmissionWithBuffer(header, bodyLength, outData);
}

void clearBuffer(void) {
  if (packetPointer != 0 && packetPointer != PACKET_SIZE) {
    debugPrintf("Clearing %d bytes of data\n", packetPointer);
  }
  noInterrupts();
  packetPointer = 0;
  outPointer = 0;
  transmissionSize = 0;
  transmitting = false;
  interrupts();
}
