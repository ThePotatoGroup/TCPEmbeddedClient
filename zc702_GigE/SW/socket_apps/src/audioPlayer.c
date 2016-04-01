/**
 *@file audioPlayer.c
 *
 *@brief
 *  - core module for audio player
 *
 * Target:   TLL6527v1-0      
 * Compiler: VDSP++     Output format: VDSP++ "*.dxe"
 *
 * @author:  Gunar Schirner
 *           Rohan Kangralkar
 * @date	03/08/2010
 *
 * LastChange:
 * $Id: audioPlayer.c 846 2014-02-27 15:35:54Z fengshen $
 *
 *******************************************************************************/

#include "audioPlayer.h"
#include "adau1761.h"
#include "zedboard_freertos.h"
#include "stdio.h"

/* number of chunks to allocate */
//TODO: up to 30 once heap size is increased
#define CHUNK_NUM 30

/* size of each chunk in bytes */
#define CHUNK_SIZE 512

/**
 * @def VOLUME_CHANGE_STEP
 * @brief Magnitude of change in the volume when increasing or decreasing
 */
#define VOLUME_CHANGE_STEP (4)
/**
 * @def VOLUME_MAX
 * @brief MAX volume possible is +6db refer to ssm2603 manual
 */
#define VOLUME_MAX (0x7F)
/**
 * @def VOLUME_MIN
 * @brief MIN volume possible is -73db refer to ssm2603 manual
 */
#define VOLUME_MIN (0x2F)


/** initialize audio player 
 *@param pThis  pointer to own object 
 *
 *@return 0 success, non-zero otherwise
 **/
int audioPlayer_init(audioPlayer_t *pThis) {
    int                         status                  = 0;
    
    printf("[AP]: Init start\r\n");
    
    pThis->volume 		= VOLUME_MIN; /*default volume */
    pThis->frequency 	= 48000; /* default frequency */
    

    /* init the codec */
	adau1761_init(&(pThis->codec));

	/* allocate buffer pool */
	status = bufferPool_d_init(&(pThis->bp), CHUNK_NUM, CHUNK_SIZE);
    if ( 1 != status ) {
        return FAIL;
    }
    
    /* Initialize the gpio ... (TBD) */
    /* TODO insert code for GPIO init here */

    /* Initialize the audio RX module*/
    status = audioRx_init(&pThis->rx, &pThis->bp) ;
    if ( 1 != status) {
        return FAIL;
    }

    /* Initialize the audio TX module */
    status = audioTx_init(&pThis->tx, &pThis->bp);
    if ( 1 != status ) {
        return FAIL;
    }   
    
    printf("[AP]: Init complete\r\n");

    return PASS;
}




/** startup phase after initialization 
 *@param pThis  pointer to own object 
 *
 *@return 0 success, non-zero otherwise
 **/
int audioPlayer_start(audioPlayer_t *pThis)
{
    printf("[AP]: startup \r\n");
    
	/* create the audio task */
	xTaskCreate( audioPlayer_task, ( signed char * ) "HW", configMINIMAL_STACK_SIZE, pThis, tskIDLE_PRIORITY + 1 , pThis );

    
    return PASS;
}



/** main loop of audio player does not terminate
 *@param pThis  pointer to own object 
 *
 *@return 0 success, non-zero otherwise
 **/
void audioPlayer_task (void *pArg) {
    
	int                      status = FAIL;
	audioPlayer_t *pThis = (audioPlayer_t *)  pArg;
    

	/* startup sub components */
    /* Start the audio RX module */
    status = audioRx_start(&pThis->rx);
    if ( 1 != status) {
    	/* how to indicate startup failure ?*/
        return;
    }

    /* Start the audio TX module */
    status = audioTx_start(&pThis->tx);
	if ( 1 != status) {
    	/* how to indicate startup failure ?*/
        return;
    }
	//pThis->rx.queue = pThis->tx.queue;
	/* main loop */
	int chunkCount = 0;
	while(1) {
    	/** get audio chunk */
		status = audioRx_get(&pThis->rx, &pThis->chunk);

        /** If we have chunks that can be played then we provide them
         * to the audio TX */
		// 4.3 4) time to play 15 chunks is 512 / 44.2kHz * 15?
        if (chunkCount == 864) {
        	vTaskDelay(1000);
        	chunkCount = 0;
        } else {
        	chunkCount++;
        }
		if ( 1 == status ) {
          /** play audio chunk through speakers */
          audioTx_put(&pThis->tx, pThis->chunk);
        }
  }
}


