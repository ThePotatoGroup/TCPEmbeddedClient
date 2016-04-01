/**
 *@file audioTx.c
 *
 *@brief
 *  - receive audio samples from DMA
 *
 * Target:   TLL6527v1-0      
 * Compiler: VDSP++     Output format: VDSP++ "*.dxe"
 *
 * @author  Gunar Schirner
 *          Rohan Kangralkar
 * @date 03/15/2009
 *
 * LastChange:
 * $Id: audioTx.c 814 2013-03-12 03:59:36Z ovaskevi $
 *
 *******************************************************************************/
#include "audioTx.h"
#include "bufferPool_d.h"
#include "stdio.h"
/** Initialize audio tx
 *    - get pointer to buffer pool
 *    - register interrupt handler
 *    - initialize TX queue

 * Parameters:
 * @param pThis  pointer to own object
 * @param pBuffP  pointer to buffer pool to take and return chunks from
 * @param pIsrDisp   pointer to interrupt dispatcher to get ISR registered
 *
 * @return Zero on success.
 * Negative value on failure.
 */
int audioTx_init(audioTx_t *pThis, bufferPool_d_t *pBuffP)
{
    // paramter checking
    if ( NULL == pThis || NULL == pBuffP) {
        printf("[ATX]: Failed init\r\n");
        return -1;
    }
    
    // store pointer to buffer pool for later access     
    pThis->pBuffP       = pBuffP;

    pThis->pPending     = NULL; // nothing pending
    pThis->running      = 0;    // DMA turned off by default
    
    // init queue 
    pThis->queue = xQueueCreate(AUDIOTX_QUEUE_DEPTH, sizeof(chunk_d_t*));

    // note ISR registration done in _start
    
    printf("[ARX]: TX init complete\r\n");
    
    return 1;
}



/** start audio tx
 *   - empthy for now
 * Parameters:
 * @param pThis  pointer to own object
 *
 * @return Zero on success.
 * Negative value on failure.
 */
int audioTx_start(audioTx_t *pThis)
{
     
	/* initialize interrupt handler */

	XScuGic *pGic; // pointer to GIC interrupt driver
	pGic = prvGetInterruptControllerInstance(); // retrieve pointer to initialized instance

	// connect own interrupt handler
	XScuGic_Connect(pGic, XPS_FPGA15_INT_ID, (Xil_ExceptionHandler) audioTx_isr, (void*) pThis);

	// enable IRQ interrupt at GIC
	XScuGic_Enable(pGic, XPS_FPGA15_INT_ID);

	// define priority and trigger type for AXI Stream FIFO IRQ
	XScuGic_SetPriorityTriggerType(pGic, XPS_FPGA15_INT_ID, 0xA0, 0x3);

	/* Enable IRQ in processor core  */
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);

    return 1;
}



/** audio tx isr  (to be called from dispatcher) 
 *   - get chunk from tx queue
 *    - if valid, release old pending chunk to buffer pool 
 *    - configure DMA 
 *    - if not valide, configure DMA to replay same chunk again
 * Parameters:
 * @param pThis  pointer to own object
 *
 * @return None 
 */
void audioTx_isr(void *pThisArg)
{
    // create local casted pThis to avoid casting on every single access
	audioTx_t  *pThis = (audioTx_t*) pThisArg;


    /*TODO: place implementation here. See hints */
	//printf("we are inside the isr\n");

    /* check int type and clear interrupt */
	unsigned int isr_reg = *(volatile unsigned int*) (FIFO_BASE_ADDR + FIFO_INT_STATUS);
	*(volatile unsigned int*) (FIFO_BASE_ADDR + FIFO_INT_STATUS) = isr_reg;

	if (isr_reg & FIFO_INT_TFPE) {
		if (xQueueIsQueueEmptyFromISR(pThis->queue)) {
			pThis->running = 0;
		} else {
			chunk_d_t *pChunk = NULL;
			xQueueReceiveFromISR(pThis->queue, &pChunk, NULL);
			u32 samplesUsed = pChunk->bytesUsed / 4;
			u32 sample_idx = 0;
			while (sample_idx < samplesUsed) {
				*(volatile unsigned int*) (FIFO_TX_DATA + FIFO_BASE_ADDR) = pChunk->u32_buff[sample_idx] << 16;
				*(volatile unsigned int*) (FIFO_TX_LENGTH + FIFO_BASE_ADDR) = 1;
				sample_idx++;
			}
			bufferPool_d_release_from_ISR(pThis->pBuffP, pChunk);
		}
	}

    /* if queue is EMPTY
     *  - set signal that ISR is not running
     *  - return */

    /* receive pointer to chunk structure from Tx_queue,
     	Note: when ISR running, audioTx_put should send the chunck to Tx_queue */

    /* how many samples does the chunk contain ? */

    /* check if sufficient space in device FIFO*/

	/* copy samples in chuck into FIFO */

    /* chunk has been copied into the FIFO, release chunk now. Return chunk to buffer pool. */


    return;
}



/** audio tx put
 *   copyies filled pChunk into the TX queue for transmission
 *    if queue is full, then chunk is dropped 
 * Parameters:
 * @param pThis  pointer to own object
 *
 * @return Zero on success.
 * Negative value on failure.
 */
int audioTx_put(audioTx_t *pThis, chunk_d_t *pChunk)
{
    
    if ( NULL == pThis || NULL == pChunk ) {
        printf("[TX]: Failed to put\r\n");
        return -1;
    }
    //
    if (!pThis->running) {
    	//printf("writing first block\n");
    	u32 samplesUsed = pChunk->bytesUsed / 4;
    	u32 spaceAvailable = *(volatile unsigned int*) (FIFO_TX_VAC + FIFO_BASE_ADDR);
    	while (spaceAvailable < samplesUsed) {
    		vTaskDelay(1);
    		spaceAvailable = *(volatile unsigned int*) (FIFO_TX_VAC + FIFO_BASE_ADDR);
    	}
    	u32 sample_idx = 0;
    	while (sample_idx < samplesUsed) {
    		*(volatile unsigned int*) (FIFO_TX_DATA + FIFO_BASE_ADDR) = pChunk->u32_buff[sample_idx] << 16;
    		*(volatile unsigned int*) (FIFO_TX_LENGTH + FIFO_BASE_ADDR) = 1;
    		sample_idx++;
    	}
    	bufferPool_d_release(pThis->pBuffP, pChunk);
    	pThis->running = 1;
    } else {
    	//printf("putting buffer into queue\n");
    	xQueueSend(pThis->queue, &pChunk, 10);
    }
    return 0;
}
