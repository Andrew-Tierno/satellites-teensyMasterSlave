#include "packet.h"
#include "main.h"
#include "states.h"

IntervalTimer timer;
volatile uint16_t state = SETUP_STATE;
elapsedMicros micro = 0;
volatile unsigned int timeAlive = 0;
volatile int errors = 0;
volatile int bugs = 0;
volatile unsigned int lastLoopTime = 0;
volatile unsigned int lastLoopState = state;
volatile unsigned int lastAnalogRead = 0;
volatile unsigned int completedDmaTransfers = 0;

void setup() {
  Serial.begin(115200);
  timer.begin(heartbeat, 10000000); // Every 10 seconds
  packet_setup();
  analogReadResolution(16);
  SPI2.begin();
  dmaSetup();
  state = IDLE_STATE;
  unsigned int startTimeCheck = micro;
  for (int i = 0; !(SPI2_SR & SPI_SR_TCF); i++) {
      if (i > 100000) {
          debugPrintf("Error: loop uint8_t failed");
          break;
      }
  }
  unsigned int timeSpent = micro - startTimeCheck;
  debugPrintf("Time spent: %d\n", (unsigned int) timeSpent);
  assert(timeSpent > 500);
}

bool assertionError(const char* file, int line, const char* assertion) {
    errors++;
    bugs++;
    Serial.printf("%s, %d: assertion 'assertion' failed, total errors %d, bugs %d\n", file, line, errors, bugs);
    return false;
}

void heartbeat() {
    debugPrintf("State %d,", state);
    debugPrintf("transmitting %d, packetsReceived %d, ", transmitting, packetsReceived);
    debugPrintf("%d errors, bugs %d, last loop %d micros", errors, bugs, lastLoopTime);
    debugPrintf(", last state %d, last read %d", lastLoopState, lastAnalogRead);
    debugPrintf(", completed transfers %d\n", completedDmaTransfers);
}

void taskIdle(void) {
    // I can't put pins on SPI2 for now so just pray it works?
    SPI2.transfer16(0xbeef);
    lastAnalogRead = analogRead(14);

    //DmaSpi::Transfer trx(nullptr, 0, nullptr);

    // TODO: error recovery on dma error
    assert(trx.m_state != DmaSpi::Transfer::State::error);
    if (trx.done()) {
        compareBuffers(dmaSrc, dmaDest);
        trx = DmaSpi::Transfer((const unsigned uint8_t *) dmaSrc, DMASIZE, dmaDest);
        clrDest((uint8_t*)dmaDest);
        DMASPI0.registerTransfer(trx);
        completedDmaTransfers++;
    }
}

void checkTasks(void) {
    long startOfLoop = micro;
    assert(state <= MAX_STATE);
    if (state == IDLE_STATE) {
        taskIdle();
    }

    // Save this into a long because elapsedMillis is not guaranteed in interrupts
    timeAlive = micro / 1000000;
    lastLoopTime = micro - startOfLoop;
    lastLoopState = state;
    // For now just assert, there isn't really a way to recover from a long main loop
    assert(lastLoopTime <= 1000000.0);
}

void loop() {
    checkTasks();
}
