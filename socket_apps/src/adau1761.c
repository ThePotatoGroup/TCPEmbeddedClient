#include "audioPlayer.h"
#include "adau1761.h"
#include "zedboard_freertos.h"


/* internal functions */

/* init I2C driver */
unsigned char adau1761_I2CMaster_init(tAdau1761 *pThis, unsigned int I2C_DeviceId, unsigned int I2C_CLK);
/* init ADAU1761 Codec to default settings */
void adau1761_codec_init(tAdau1761 *pThis);
/* init the AXI streaming FIFO */
void adau1761_FIFO_init();
/* init the axi_iis_adi component */
void adau1761_iis_init(tAdau1761 *pThis);
void adau1761_iis_tx_enable();

/* register write access to CODECs internal registers */
void adau1761_regWrite(tAdau1761 *pThis, unsigned char u8RegAddr, unsigned char u8Data);


/* Initializes ADI I2C IP and AXI Streaming FIFO */
unsigned char adau1761_init(tAdau1761 *pThis)
{
	/* init I2C driver */
	adau1761_I2CMaster_init(pThis, 0, I2C_CLOCK);

	/* init ADAU1761 Codec to default settings */
	adau1761_codec_init(pThis);

	/* init the axi_iis_adi component */
	adau1761_iis_init(pThis);

	/* init the AXI streaming FIFO */
	adau1761_FIFO_init(pThis);

	return 0;
}

/* init the axi_iis_adi component */
void adau1761_iis_init(tAdau1761 *pThis) {

	//Reset I2S TX
	Xil_Out32(AXI_I2S_REGISTER(AXI_I2S_REG_RESET), AXI_I2S_RESET_TX_FIFO);

	//configure I2S clock dividers
	unsigned char bclk_div, word_size;
	unsigned int bclk_rate;

	bclk_rate = AXI_I2S_RATE * AXI_I2S_BITS_PER_FRAME;
	word_size = AXI_I2S_BITS_PER_FRAME / 2 - 1;

	bclk_div = (AXI_I2S_REF_CLK / bclk_rate) / 2 -1;

	Xil_Out32(AXI_I2S_REGISTER(AXI_I2S_REG_CLK_CTRL), (word_size<<16)|bclk_div);

	//enable I2S TX
	adau1761_iis_tx_enable();
}

void adau1761_iis_tx_enable()
{
	Xil_Out32(AXI_I2S_REGISTER(AXI_I2S_REG_CTRL), AXI_I2S_CTRL_TX_EN);
}

/* ---------------------------------------------------------------------------- *
 * 								AudioCodec_Config()								*
 * ---------------------------------------------------------------------------- *
 * Configures audio codes's internal PLL. With MCLK = 10 MHz it configures the
 * PLL for a VCO frequency = 49.152 MHz, and an audio sample rate of 48 KHz.
 * ---------------------------------------------------------------------------- */
void adau1761_codec_init(tAdau1761 *pThis) {

	unsigned char u8TxData[8], u8RxData[6];

	// Disable Core Clock
	adau1761_regWrite(pThis, R0_CLOCK_CONTROL, 0x0E);

	/* 	MCLK = 12.288 MHz
		R = 0100 = 4

		PLL required output = 1024x48 KHz
		(PLLout)			= 49.152 MHz

		PLLout/MCLK			= 49.152 MHz/12.288 MHz
							= 4 */

	// Write 6 bytes to R1 @ register address 0x4002
	u8TxData[0] = 0x40; // Register write address [15:8]
	u8TxData[1] = 0x02; // Register write address [7:0]
	u8TxData[2] = 0x00; // byte 6 - M[15:8]
	u8TxData[3] = 0x00; // byte 5 - M[7:0]
	u8TxData[4] = 0x00; // byte 4 - N[15:8]
	u8TxData[5] = 0x00; // byte 3 - N[7:0]
	u8TxData[6] = 0x20; // byte 2 - 7 = reserved, bits 6:3 = R[3:0], 2:1 = X[1:0], 0 = PLL operation mode
	u8TxData[7] = 0x01; // byte 1 - 7:2 = reserved, 1 = PLL Lock, 0 = Core clock enable

	// Write bytes to PLL Control register R1 @ 0x4002
	XIicPs_MasterSendPolled(&(pThis->Iic), u8TxData, 8, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&pThis->Iic));

	// Register address set: 0x4002
	u8TxData[0] = 0x40;
	u8TxData[1] = 0x02;

	// Poll PLL Lock bit
	do {
		XIicPs_MasterSendPolled(&pThis->Iic, u8TxData, 2, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&pThis->Iic));
		XIicPs_MasterRecvPolled(&pThis->Iic, u8RxData, 6, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&pThis->Iic));
	}
	while((u8RxData[5] & 0x02) == 0); // while not locked

	adau1761_regWrite(pThis, R0_CLOCK_CONTROL, 0x0F);	// 1111
												// bit 3:		CLKSRC = PLL Clock input
												// bits 2:1:	INFREQ = 1024 x fs
												// bit 0:		COREN = Core Clock enabled

	//Initialize ADAU1761 control ports. (Refer to Page 51 of the ADAU1761 datasheet)

	adau1761_regWrite(pThis, R16_SERIAL_PORT_CONTROL_1, 0x00);
	adau1761_regWrite(pThis, R17_CONVERTER_CONTROL_0, 0x05);//48 KHz
	adau1761_regWrite(pThis, R64_SERIAL_PORT_SAMPLING_RATE, 0x05);//48 KHz
	adau1761_regWrite(pThis, R19_ADC_CONTROL, 0x13);
	adau1761_regWrite(pThis, R36_DAC_CONTROL_0, 0x03);
	adau1761_regWrite(pThis, R35_PLAYBACK_POWER_MANAGEMENT, 0x03);
	adau1761_regWrite(pThis, R58_SERIAL_INPUT_ROUTE_CONTROL, 0x01);
	adau1761_regWrite(pThis, R59_SERIAL_OUTPUT_ROUTE_CONTROL, 0x01);
	adau1761_regWrite(pThis, R65_CLOCK_ENABLE_0, 0x7F);
	adau1761_regWrite(pThis, R66_CLOCK_ENABLE_1, 0x03);

	adau1761_regWrite(pThis, R4_RECORD_MIXER_LEFT_CONTROL_0, 0x01);
	adau1761_regWrite(pThis, R5_RECORD_MIXER_LEFT_CONTROL_1, 0x05);
	adau1761_regWrite(pThis, R6_RECORD_MIXER_RIGHT_CONTROL_0, 0x01);
	adau1761_regWrite(pThis, R7_RECORD_MIXER_RIGHT_CONTROL_1, 0x05);

	adau1761_regWrite(pThis, R22_PLAYBACK_MIXER_LEFT_CONTROL_0, 0x21);
	adau1761_regWrite(pThis, R24_PLAYBACK_MIXER_RIGHT_CONTROL_0, 0x41);
	adau1761_regWrite(pThis, R26_PLAYBACK_LR_MIXER_LEFT_LINE_OUTPUT_CONTROL, 0x03);
	adau1761_regWrite(pThis, R27_PLAYBACK_LR_MIXER_RIGHT_LINE_OUTPUT_CONTROL, 0x09);
	adau1761_regWrite(pThis, R29_PLAYBACK_HEADPHONE_LEFT_VOLUME_CONTROL, 0xE7);
	adau1761_regWrite(pThis, R30_PLAYBACK_HEADPHONE_RIGHT_VOLUME_CONTROL, 0xE7);
	adau1761_regWrite(pThis, R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, 0xE7);
	adau1761_regWrite(pThis, R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, 0xE7);

}

/* ---------------------------------------------------------------------------- *
 * 									IicConfig()									*
 * ---------------------------------------------------------------------------- *
 * IIC initialization with clock configuration.
 * ---------------------------------------------------------------------------- */
unsigned char adau1761_I2CMaster_init(tAdau1761 *pThis, unsigned int I2C_DeviceId, unsigned int I2C_CLK)
{
	XIicPs_Config *Config;

	Config = XIicPs_LookupConfig(I2C_DeviceId);
	XIicPs_CfgInitialize(&(pThis->Iic), Config, Config->BaseAddress);
	XIicPs_SetSClk(&(pThis->Iic), I2C_CLK);

	return 0;
}


/* init the AXI streaming FIFO */
void adau1761_FIFO_init(tAdau1761 *pThis)
{
	//Reset AXI-Streaming FIFO Transmit side
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_TX_RESET) = FIFO_TX_RESET_VALUE;
	//Initialize the TX FIFO buffer with 0
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_TX_DES) = 0x00;

	//Reset AXI-Streaming FIFO Transmit side
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_RX_RESET) = FIFO_RX_RESET_VALUE;
	//Initialize the TX FIFO buffer with 0
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_RX_DES) = 0x00;

	/* Reset the core and generate the external reset by writing to the Local Link Reset Register. */
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_LLR_OFFSET) = FIFO_LLR_RESET_VALUE;

	/* clear all pending interrupts */
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_INT_STATUS) = 0xffffffff;

	/* Enable TFPE interrupt to propagate */
	*(volatile u32 *) (FIFO_BASE_ADDR + FIFO_INT_ENABLE) = FIFO_INT_TFPE;
}

/* ---------------------------------------------------------------------------- *
 * 								ADAU1761_RegWrite									*
 * ---------------------------------------------------------------------------- *
 * Function to write one byte (8-bits) to one of the registers from the audio
 * controller via I2C --
 * ---------------------------------------------------------------------------- */
void adau1761_regWrite(tAdau1761 *pThis, unsigned char u8RegAddr, unsigned char u8Data) {

	unsigned char u8TxData[3];

	u8TxData[0] = 0x40;
	u8TxData[1] = u8RegAddr;
	u8TxData[2] = u8Data;

	XIicPs_MasterSendPolled(&pThis->Iic, u8TxData, 3, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&pThis->Iic));
}

void audioPlayer_setvolume(audioPlayer_t *pThis){

	// Supports only for Mono as of now.
	adau1761_regWrite(&pThis->codec, R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, pThis->volume);
	adau1761_regWrite(&pThis->codec, R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, pThis->volume);

}
