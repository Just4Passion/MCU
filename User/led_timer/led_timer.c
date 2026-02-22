
#include "FreeRTOS.h"
#include "timers.h"

#define partstMAX_OUTPUT_LED 1
#define ledFLASH_RATE_BASE    ((( TickType_t)333)/portTICK_PERIOD_MS)
#define ledDONT_BLOCK         (( TickType_t )0)


void vParTestToggleLED(unsigned portBASE_TYPE uxLED)
{
    static unsigned short usBit[partstMAX_OUTPUT_LED] = {0};

	vTaskSuspendAll();
	{
		if( uxLED < partstMAX_OUTPUT_LED )
		{
            if (usBit[uxLED] == 0)
            {
                usBit[uxLED] = 1;
                led_off();
            }
            else
            {
                usBit[uxLED] = 0;
                led_on();
            }
		}
	}
	xTaskResumeAll();
}

static void vLedTimerCallback(TimerHandle_t xTimer)
{
    BaseType_t xTimerID;
    xTimerID = (BaseType_t)pvTimerGetTimerID(xTimer);
    vParTestToggleLED( xTimerID );
}

void vStartLedTimers(UBaseType_t uxNumberOfLed)
{
    UBaseType_t uxLEDTimer;
    TimerHandle_t xLedTimer;

    for (uxLEDTimer = 0; uxLEDTimer < uxNumberOfLed; uxLEDTimer++)
    {
        xLedTimer = xTimerCreate("LED Timer", 
                                ledFLASH_RATE_BASE * ( uxLEDTimer + 1 ),
                                pdTRUE,
                                (void *)uxLEDTimer, 
                                vLedTimerCallback);
        if (xLedTimer != NULL)
        {
            xTimerStart(xLedTimer, 0);
        }
    }
}




