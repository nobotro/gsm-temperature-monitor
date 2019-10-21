#include "api_hal_gpio.h"
#include "stdint.h"
#include "stdbool.h"
#include "api_debug.h"
#include "api_os.h"
#include "api_hal_pm.h"
#include "api_os.h"
#include "api_event.h"
#include "time.h"
#include "api_sys.h"
#include "hal_iomux.h"
#include "api_event.h"
#include "api_sms.h"
#include "api_hal_uart.h"
#include "arst7789.h"
#include "lvgl.h"
#include "api_key.h"
 #include "stdio.h"
 #include "stdlib.h"
#define ST7789_PIN_RST GPIO_PIN6
#define ST7789_PIN_DC  GPIO_PIN7
#define ST7789_SPI          SPI2


#define ST7789_WIDTH   240  // x
#define ST7789_HEIGHT  240  // y


#define MAIN_TASK_STACK_SIZE    (1024 * 2)
#define MAIN_TASK_PRIORITY      0 
#define MAIN_TASK_NAME         "MAIN Task"

#define MAX_CYCLES 3373
static HANDLE mainTaskHandle = NULL;
static HANDLE secondTaskHandle = NULL;
static HANDLE lvglTaskHandle = NULL;

#define DHT_PIN GPIO_PIN25
#define BCKL_PIN GPIO_PIN5

static void ex_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t* color_p);
static void ex_disp_map(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t* color_p);
static void ex_disp_fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2,  lv_color_t color);

static bool ex_tp_read(lv_indev_data_t* data);


float humidity;
float temperature;
uint8_t flag = 0;

bool firstboot=true;
uint8_t* oled_buffer=NULL;
bool screen_is_on=false;
bool sms_is_sending=true;
bool init_flag=false;

 lv_obj_t * label1;
    lv_obj_t * label2;
    lv_obj_t * label3;
    lv_obj_t * label4;


bool clicklock=false;

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
       
        case API_EVENT_ID_SYSTEM_READY:
            Trace(1,"system initialize complete");
            flag |= 1;
            break;
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(2,"network register success");
            TIME_SetIsAutoUpdateRtcTime(true);
            OS_Sleep(2000);
            OS_StartTask(secondTaskHandle,NULL);
            flag |= 2;
            break;
        case API_EVENT_ID_SMS_SENT:
            Trace(2,"Send Message Success");
            break;
        case API_EVENT_ID_SMS_ERROR:
            Trace(10,"SMS error occured! cause:%d",pEvent->param1);

        case API_EVENT_ID_KEY_DOWN:
            // Trace(1,"key down, key:0x%02x",pEvent->param1);
            if(pEvent->param1 == KEY_POWER && !clicklock && !sms_is_sending)
            {clicklock=true;
                screen_is_on=true;
                GPIO_Set(BCKL_PIN, GPIO_LEVEL_HIGH);
                
                OS_StartTask(lvglTaskHandle,NULL);
                                    
                OS_Sleep(10000);
                OS_StopTask(lvglTaskHandle);
                 GPIO_Set(BCKL_PIN, GPIO_LEVEL_LOW);
                 

                Trace(1,"power key press down now");
                clicklock=false;
                screen_is_on=false;

            }
            break;

        default:
            break;
    }

    
}


void SMSInit()
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT,SIM0))
    {
        Trace(1,"sms set format error");
        return;
    }
    SMS_Parameter_t smsParam = {
        .fo = 17 ,
        .vp = 167,
        .pid= 0  ,
        .dcs= 0  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
    };
    if(!SMS_SetParameter(&smsParam,SIM0))
    {
        Trace(1,"sms set parameter error");
        return;
    }
   
}
void UartInit()
{
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
    };
    UART_Init(UART1,config);
}
void Init()
{
    UartInit();
    SMSInit();
    
}

void Init_Interface()
{
    
     oled_buffer= OS_Malloc(115200);
     memset(oled_buffer, 0, 115200);
         ST7789_Init(ST7789_PIN_DC,ST7789_PIN_RST,ST7789_SPI,oled_buffer);
         init(240,240);
         fillScreen(WHITE);
          
         scan();

         
     
        
         
}




void Init_LVGL()
{

    srand(123);
    lv_init();

    lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/
    disp_drv.disp_flush = ex_disp_flush;            /*Used in buffered mode (LV_VDB_SIZE != 0  in lv_conf.h)*/

    disp_drv.disp_fill = ex_disp_fill;              /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/
    disp_drv.disp_map = ex_disp_map;                /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/


   /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);

    // OS_StartCallbackTimer(mainTaskHandle,1,lvgl_task_tick,NULL);

    /*************************
     * Input device interface
     *************************/
    /*Add a touchpad in the example*/
    /*touchpad_init();*/                            /*Initialize your touchpad*/
    lv_indev_drv_t indev_drv;                       /*Descriptor of an input device driver*/
    lv_indev_drv_init(&indev_drv);                  /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;         /*The touchpad is pointer type device*/
    indev_drv.read = ex_tp_read;                 /*Library ready your touchpad via this function*/
    lv_indev_drv_register(&indev_drv);              /*Finally register the driver*/

    
}





void showData(int hum,int temp)
{
    
        
       

    /*Modify the Label's text*/
    char tempb[5];
    char humb[5];
    sprintf(tempb,"%dC",temp);
 sprintf(humb,"%d%%",hum);

    lv_label_set_text(label1, "temperature");
lv_label_set_text(label2, tempb);
lv_label_set_text(label3, "humidity");
lv_label_set_text(label4, humb);
 lv_obj_align(label1, NULL, LV_ALIGN_IN_TOP_MID, 0, 34);
lv_obj_align(label2, NULL,LV_ALIGN_IN_TOP_MID, 0, 80);
lv_obj_align(label3, NULL,LV_ALIGN_IN_TOP_MID, 0, 140);
lv_obj_align(label4, NULL, LV_ALIGN_IN_TOP_MID, 0, 186);


    /* Align the Label to the center
     * NULL means align on parent (which is the screen now)
     * 0, 0 at the end means an x, y offset after alignment*/
  

}






uint32_t expectPulse(GPIO_LEVEL level){
    volatile uint32_t count = 0;
    GPIO_LEVEL pinState;

    GPIO_Get(DHT_PIN, &pinState);
    while (pinState == level){
        if (count++ > MAX_CYCLES){
            return 0;
        }
        GPIO_Get(DHT_PIN, &pinState);
    }

    return count;
}


bool readDht(){
    uint8_t data[5];
    uint32_t cycles[80];
    humidity=0;
    temperature=0;
    GPIO_config_t ioPin = {
    .mode = GPIO_MODE_OUTPUT,
    .pin = DHT_PIN,
    .defaultLevel = GPIO_LEVEL_HIGH
    };
    uint32_t status;

    // Start reading process
    GPIO_Init(ioPin);
    GPIO_Set(DHT_PIN, GPIO_LEVEL_HIGH);
    OS_Sleep(250);

    // Set data line low and wait for 20 msec
    GPIO_Set(DHT_PIN, GPIO_LEVEL_LOW);
    OS_Sleep(20);
    // End start signal
    GPIO_Set(DHT_PIN, GPIO_LEVEL_HIGH);
    OS_SleepUs(40);

    // Change pin
    ioPin.mode = GPIO_MODE_INPUT;
    GPIO_Init(ioPin);

   
    // status = SYS_EnterCriticalSection();

    // First expect a low signal f0r 80 us
    if (expectPulse(GPIO_LEVEL_LOW) == 0){
        // SYS_ExitCriticalSection(status);    
        Trace(2, "temppTimeout waiting for start signal low pulse");
        return false;
    }
    if (expectPulse(GPIO_LEVEL_HIGH) == 0){
        // SYS_ExitCriticalSection(status);    
        Trace(2, "temppTimeout waiting for start signal high pulse");
        return false;
    }
   
    for (int i = 0; i < 80; i += 2) {
    cycles[i] = expectPulse(GPIO_LEVEL_LOW);
    cycles[i + 1] = expectPulse(GPIO_LEVEL_HIGH);
    }

    // End time critical
    // SYS_ExitCriticalSection(status);    

    // Inspect pulses and determine which ones are 0 (high state cycle count < low
    // state cycle count), or 1 (high state cycle count > low state cycle count).
    for (int i = 0; i < 40; ++i) {
        uint32_t lowCycles = cycles[2 * i];
        uint32_t highCycles = cycles[2 * i + 1];
        if ((lowCycles == 0) || (highCycles == 0)) {
            Trace(2, "temppTimeout waiting for pulse (%d).", i);
            return false;
        }
        data[i/8] <<= 1;
       
        if (highCycles > lowCycles) {
            data[i/8] |= 1;
        }
        
    }

    temperature=(((data[2]&127)<<8)|data[3])/10;
    if(data[2]>>7)temperature=temperature*-1;
    humidity=((data[0]<<8)|data[1])/10;
    
    bool passed=((data[0] +data[1] +data[2] +data[3]) & 0xff)==data[4];
     Trace(1,"tempppp status: %d humm: %.2f%% temp: %.2f  \n",passed,humidity,temperature);
            
    return true;
}

uint32_t calc_sleep_range(TIME_System_t time)
{

                
     uint8_t temp=-1;
    uint8_t ranges[]={ 0,3, 6, 9, 12, 15, 18, 21};
    temp=ranges[((time.hour/3)+1)%8];

    if(temp==0)
        temp=24;

         


       return (((temp-time.hour)*60)- time.minute)*60000;

   


}


void DhtTask()
{


    //turn off screen
  
    
    while(1){   


                if(screen_is_on)
                {
                   OS_Sleep(10000) ;
                   continue;

                }

             
    
                PM_SleepMode(false);
                OS_Sleep(1000);
                
                sms_is_sending=true;
                

                float avgtemp=0;
                float avghum=0;
                int passed=0;
                //read temp 5 times
                for(int i=0;i<5;i++)
                {   
                    readDht();
                    OS_Sleep(3000);
                    if(i==0)continue;//skip first measured data
                   
                    if(humidity >0 && temperature>0)
                    {
                        passed++;

                    avghum+=humidity;
                    avgtemp+=temperature;
                    }
                   

                }
                avghum=avghum/passed;
                avgtemp=avgtemp/passed;
                char st[70];

		       TIME_System_t localTime;
                TIME_GetLocalTime(&localTime);

                uint16_t sleep_count=(calc_sleep_range(localTime)/240000)+1;

                Trace(1,"sleep count %d",sleep_count);

                sprintf(st,"boot: %d, time : %02d:%02d , hum: %d%% temp: %d , next sms in: %d minute",firstboot,localTime.hour,localTime.minute,(int)avghum,(int)avgtemp,sleep_count*4);
                firstboot=false;
                //use 00 insted of +
                SMS_SendMessage("00995592297676",st,sizeof(st),SIM0);
             

                PM_SleepMode(true);
                sms_is_sending=false;
               
 		
                
             
                for(int i=0;i<sleep_count;i++)
                {
                    OS_Sleep(240000);
                   
                }

                  
               
}

}

void LVGL_Task(void* param)
{
   
    
    Trace(3,"lvgl start");
    
    if(!init_flag)
    {
Init_Interface();
     Init_LVGL();


init_flag=true;

 static lv_style_t style_txt;
    lv_style_copy(&style_txt, &lv_style_plain);
    
style_txt.text.color = LV_COLOR_HEX(0x25aae1);
style_txt.body.main_color=LV_COLOR_HEX(0x0);
style_txt.body.grad_color=LV_COLOR_HEX(0x0);

 lv_obj_set_style(lv_scr_act(), &style_txt);   

   
    /*Create a Label on the currently active screen*/
    label1 =  lv_label_create(lv_scr_act(), NULL);
     label2 =  lv_label_create(lv_scr_act(), NULL);
      label3 =  lv_label_create(lv_scr_act(), NULL);
       label4 =  lv_label_create(lv_scr_act(), NULL);
    
       

    }
  
    //waiting to finish temp read and sms jobs
    while(sms_is_sending){};

     PM_SleepMode(false);
     OS_Sleep(1000);
        readDht();
         PM_SleepMode(true);
         OS_Sleep(1000);
        
    
    showData((int)humidity,(int)temperature);
  
   //stop second rask if screen on
     

    // //show hello world and height 16 bits

    /*************************************
     * Run the task handler of LittlevGL
     *************************************/
    while(1) {
         
        lv_tick_inc(3);
        lv_task_handler();
        OS_Sleep(5);
    }
}






void MainTask(void *pData)
{
    API_Event_t* event=NULL;
     Init();
    


      GPIO_config_t backlit = {
    .mode = GPIO_MODE_OUTPUT,
    .pin = BCKL_PIN,
    .defaultLevel = GPIO_LEVEL_LOW
    };
    GPIO_Init(backlit);
    
    secondTaskHandle = OS_CreateTask(DhtTask,
        NULL, NULL, 2048, 1, 1, 0, "dht task");


    lvglTaskHandle = OS_CreateTask(LVGL_Task,
        NULL, NULL, 8048, 2, 1, 0, "lvgl task");




    while(1)
    {
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}


void gpio_Main()
{
    mainTaskHandle = OS_CreateTask(MainTask ,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}



static void ex_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t* color_p)
{
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

         Trace(3,"ex_disp_flush %d,%d %d,%d",x1,y1,x2,y2);

   
   
   
    for(uint16_t y = y1; y <= y2; y++) {
        for(uint16_t x = x1; x <= x2; x++) {
     
        
        oled_buffer[((y*240)+x)*2]=(color_p->full)>>8;
        oled_buffer[(((y*240)+x)*2)+1]=color_p->full;
             color_p++;
             
        }
    }
    
    
    
 

  scan();
  
    
  lv_flush_ready();
   
    
}


/* Write a pixel array (called 'map') to the a specific area on the display
 * This function is required only when LV_VDB_SIZE == 0 in lv_conf.h*/
static void ex_disp_fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2,  lv_color_t color)
{
     Trace(3,"ex_disp_fill %d,%d %d,%d",x1,y1,x2,y2);
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

      for(uint16_t y = y1; y <= y2; y++) {
        for(uint16_t x = x1; x <= x2; x++) {
        oled_buffer[((y*240)+x)]=(color.full)>>8;
        oled_buffer[(((y*240)+x))+1]=color.full;
           
             
        }
    }
    
    
    
    
     
  scan();
}

/* Write a pixel array (called 'map') to the a specific area on the display
 * This function is required only when LV_VDB_SIZE == 0 in lv_conf.h*/
static void ex_disp_map(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
    
    Trace(3,"ex_disp_map %d,%d %d,%d",x1,y1,x2,y2);
      for(uint16_t y = y1; y <= y2; y++) {
        for( uint16_t x = x1; x <= x2; x++) {
        oled_buffer[((y*240)+x)*2]=(color_p->full)>>8;
        oled_buffer[(((y*240)+x)*2)+1]=color_p->full;
             color_p++;
             
        }
    }
    
    
    
    
     
  scan();
}
 
 
/* Read the touchpad and store it in 'data'
 * Return false if no more data read; true for ready again */
static bool ex_tp_read(lv_indev_data_t *data)
{
    /* Read your touchpad */
    /* data->state = LV_INDEV_STATE_REL or LV_INDEV_STATE_PR */
    /* data->point.x = tp_x; */
    /* data->point.y = tp_y; */

    return false;   /*false: no more data to read because we are no buffering*/
}
