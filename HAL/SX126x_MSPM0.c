#include "ics/SX126X/sx126x-board.h"
#include "HAL/SX126x_MSPM0.h"

// Global DioIrq callback
static DioIrqHandler *BoardDioIrq = NULL;

// Global SX126x instance
SX126x_t SX126x;

void SX126xIoInit(void) {
    // Pin configuration is handled via SYSCFG_DL_init().
    // Set default CS and Reset pin states.
    SX126xSetNss(1);
    DL_GPIO_setPins(DIGITAL_OUTPUT_PORTA_PORT, DIGITAL_OUTPUT_PORTA_LORA_2_RST_PIN);
}

void SX126xIoIrqInit(DioIrqHandler dioIrq) {
    BoardDioIrq = dioIrq;
}

void SX126xIoDeInit(void) {
}

void SX126xIoDbgInit(void) {
}

void SX126xReset(void) {
    // Drive reset pin LOW to trigger hardware reset
    DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTA_PORT, DIGITAL_OUTPUT_PORTA_LORA_2_RST_PIN);
    SX126xDelayMs(20);
    // Drive reset pin HIGH to release hardware reset
    DL_GPIO_setPins(DIGITAL_OUTPUT_PORTA_PORT, DIGITAL_OUTPUT_PORTA_LORA_2_RST_PIN);
    SX126xDelayMs(20);
}

void SX126xWaitOnBusy(void) {
    // Wait while BUSY pin is HIGH
    while (DL_GPIO_readPins(EXTERNAL_INTERRUPT_LORA_2_BUSY_PORT, EXTERNAL_INTERRUPT_LORA_2_BUSY_PIN)) {
        delay_cycles(100);
    }
}

bool SX126xCheckRfFrequency(uint32_t frequency) {
    return true;
}

void SX126xDelayMs(uint32_t ms) {
    delay_cycles(ms * 32000U); // 32MHz clock -> 32,000 cycles per ms
}

// Software/Hardware Timer stubs (handled internally by SX126x ticks)
void SX126xTimerInit(void) {}
void SX126xSetTxTimerValue(uint32_t nMs) {}
void SX126xTxTimerStart(void) {}
void SX126xTxTimerStop(void) {}
void SX126xSetRxTimerValue(uint32_t nMs) {}
void SX126xRxTimerStart(void) {}
void SX126xRxTimerStop(void) {}

void SX126xSetNss(uint8_t lev) {
    if (lev == 0) {
        DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_CHIP_S_LORA_PIN);
    } else {
        DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_CHIP_S_LORA_PIN);
    }
}

uint8_t SX126xSpiInOut(uint8_t data) {
    // Polling transfer over SPI0
    while (DL_SPI_isTXFIFOFull(SPI_0_INST));
    DL_SPI_transmitData8(SPI_0_INST, data);
    
    while (DL_SPI_isRXFIFOEmpty(SPI_0_INST));
    return DL_SPI_receiveData8(SPI_0_INST);
}
