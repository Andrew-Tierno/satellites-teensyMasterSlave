#ifndef PID_H
#define PID_H
#include <main.h>
#include <packet.h>

typedef struct pidSample {
    adcSample sample;
    adcSample incoherentOutput;
    mirrorOutput out;

    pidSample(): sample(), incoherentOutput(), out() {
    }

    pidSample(const volatile pidSample& s) {
        copy(s);
    }

    pidSample(const volatile adcSample& sampleAdc, const volatile adcSample& incoherent, const volatile mirrorOutput& o) {
        sample.copy(sampleAdc);
        incoherentOutput.copy(incoherent);
        out.copy(o);
    }

    void copy(const volatile pidSample& s) {
        sample = s.sample;
        incoherentOutput = s.incoherentOutput;
        out = s.out;
    }

    pidSample& operator =(const volatile pidSample& s) {
        /*if (Serial) {
            Serial.printf("Equals operator\n");
        }*/
        copy(s);
        return *this;
    }

    volatile uint16_t getChecksum() {
        return sample.getChecksum() + out.getChecksum();
    }
} pidSample;

// Twice the size
typedef struct expandedPidSample {
    pidSample a;
    pidSample b;
} expandedPidSample;

void writeExpandedPidSampleWithChecksum(const pidSample* in, expandedPidSample* out, volatile uint16_t& pidBufferChecksum);

#define PID_NUM_CHANNELS 4
#define PID_SAMPLE_SIZE (sizeof(pidSample) / 2) // 16-bit
#define PID_DATA_DUMP_SIZE 15
#define PID_DATA_DUMP_SIZE_UINT16 (PID_DATA_DUMP_SIZE * sizeof(pidSample) * 8 / 16)
#define PID_BUFFER_SIZE 3000 // units are PID_SAMPLE_SIZE
#define PID_DATA_READY_PIN 42

/* *** The packet buffer ships out directly to audacy, it dequeues from the pidSamples buffer *** */
#pragma pack()
typedef struct pidDumpPacket_t {
    // This is uint32 instead of uint16 because DMA takes in 32-bit arguments but only uses the 16 least significant bits
    uint32_t header[OUT_PACKET_BODY_BEGIN];
    uint32_t pidHeader[6];
    expandedPidSample body[PID_DATA_DUMP_SIZE + OUT_PACKET_OVERHEAD + 10];
} pidDumpPacket_t;

// Only variable visible should be packet related
extern volatile bool pidPacketReady;
extern volatile uint32_t* pidDumpPacketUints;
extern volatile uint16_t pidPacketChecksum;

extern volatile unsigned int pidDataPointer, pidSentDataPointer, pidSamplesSent;

void pidDataSetup();
void taskPidData();
void enterPidData();
void leavePidData();
void pidPacketSent();
void recordPid(const volatile pidSample& s);
void pidDataHeartbeat();
#endif // PID
