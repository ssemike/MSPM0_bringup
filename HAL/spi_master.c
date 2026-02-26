
// #include "ti_msp_dl_config.h"

// #define SPI_PACKET_SIZE (16)

// // Update your data array with more values
// uint8_t gTxPacket[SPI_PACKET_SIZE] = {
//     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
//     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
// };

// /* Data for SPI to receive */
// uint8_t gRxPacket[SPI_PACKET_SIZE];

// /* Expected data from the Peripheral (Slave) */
// uint8_t expectedRxMessage[SPI_PACKET_SIZE] = {
//     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
//     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
// };

// volatile bool gSPIDataTransmitted;
// volatile bool gDMATXDataTransferred;
// volatile bool gDMARXDataTransferred;

// int main(void)
// {
//     SYSCFG_DL_init();

//     /* Reset flags */
//     gSPIDataTransmitted   = false;
//     gDMATXDataTransferred = false;
//     gDMARXDataTransferred = false;

//     /*
//      * Configure DMA TX channel (gTxPacket → SPI TX FIFO)
//      * We do NOT enable it yet — the button will trigger it.
//      */
//     DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&gTxPacket[0]);
//     DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)(&SPI_0_INST->TXDATA));
//     DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, SPI_PACKET_SIZE);

//     /*
//      * Configure DMA RX channel (SPI RX FIFO → gRxPacket)
//      * Enable it immediately so it is ready to catch incoming bytes.
//      */
//     DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)(&SPI_0_INST->RXDATA));
//     DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)&gRxPacket[0]);
//     DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, SPI_PACKET_SIZE);
//     DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);

//     DL_SYSCTL_disableSleepOnExit();

//     /* Enable SPI and Button interrupts */
//     NVIC_EnableIRQ(SPI_0_INST_INT_IRQN);
//     NVIC_EnableIRQ(GPIO_SWITCHES_INT_IRQN);

//     /* Main loop just sleeps — everything happens in ISRs */
//     while (1) {
//         __WFI();
//     }
// }

// void SPI_0_INST_IRQHandler(void)
// {
//     switch (DL_SPI_getPendingInterrupt(SPI_0_INST)) {
//         case DL_SPI_IIDX_DMA_DONE_TX:
//             /* DMA finished pushing gTxPacket into TX FIFO */
//             gDMATXDataTransferred = true;
//             break;

//         case DL_SPI_IIDX_TX_EMPTY:
//             /* All bytes have been clocked out on the wire */
//             gSPIDataTransmitted = true;
//             break;

//         case DL_SPI_IIDX_DMA_DONE_RX:
//             /* DMA finished moving received bytes into gRxPacket */
//             gDMARXDataTransferred = true;

//             /* === Full transaction complete → check result === */
//             bool comparisonSuccessful = true;
//             for (uint8_t i = 0; i < SPI_PACKET_SIZE; i++) {
//                 if (gRxPacket[i] != expectedRxMessage[i]) {
//                     comparisonSuccessful = false;
//                     break;
//                 }
//             }

//             if (comparisonSuccessful) {
//                 /* Turn on both LEDs to indicate success */
//                 DL_GPIO_clearPins(GPIO_LEDS_PORT,
//                     GPIO_LEDS_USER_LED_1_PIN | GPIO_LEDS_USER_TEST_PIN);
//             }
//             break;

//         default:
//             break;
//     }
// }

// /*
//  * Button handler — identical style to your echo example
//  */
// void GROUP1_IRQHandler(void)
// {
//     switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
//         case GPIO_SWITCHES_INT_IIDX:
//             switch (DL_GPIO_getPendingInterrupt(GPIO_SWITCHES_PORT)) {
//                 case GPIO_SWITCHES_USER_SWITCH_1_IIDX:
//                     /* Button pressed → start a new transfer */
//                     gDMATXDataTransferred = false;
//                     gSPIDataTransmitted   = false;
//                     gDMARXDataTransferred = false;

//                     /* Re-arm both DMA channels for the next packet */
//                     DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, SPI_PACKET_SIZE);
//                     DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, SPI_PACKET_SIZE);

//                     /* Enable TX DMA → this starts the whole SPI transaction */
//                     DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
//                     break;

//                 default:
//                     break;
//             }
//             break;

//         default:
//             break;
//     }
// }