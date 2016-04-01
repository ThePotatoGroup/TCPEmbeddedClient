/**
 *@file audioRx.c
 *
 *@brief
 *  - receive audio samples from DMA
 *
 * Target:   TLL6527v1-0      
 * Compiler: VDSP++     Output format: VDSP++ "*.dxe"
 *
 * @author  Gunar Schirner
 *           Rohan Kangralkar
 * @date 03/15/2009
 *
 * LastChange:
 * $Id: audioRx.c 846 2014-02-27 15:35:54Z fengshen $
 *
 *******************************************************************************/
#include "audioRx.h"
#include "bufferPool_d.h"
#include "audioSample.h"

int audioRx_init(audioRx_t *pThis, bufferPool_d_t *pBuffP)
{
    if ( NULL == pThis || NULL == pBuffP) {
        printf("[ARX]: Failed init\r\n");
        return -1;
    }

    pThis->pPending     = NULL;
    pThis->pBuffP       = pBuffP;

    // init queue with
    pThis->queue = xQueueCreate(AUDIORX_QUEUE_DEPTH, sizeof(chunk_d_t*));


    printf("[ARX]: RX init complete\r\r\n");

    return 1;
}


int audioRx_start(audioRx_t *pThis)
{
	audioSample_init(&pThis->audioSample);

    return 1;
}

int audioRx_get(audioRx_t *pThis, chunk_d_t **pChunk)
{
	int size = 0;

    int i = 0;
    while (bufferPool_d_acquire(pThis->pBuffP, pChunk, 512) != 1) {
    	printf("waiting %d \n", i++);
    	vTaskDelay( 1 );
        //printf("No free buffer pool samples available\n");
        //return -1;
    }



    size = audioSample_get(&pThis->audioSample, *pChunk);

    return size > 0 ? 1 : -1;
}

