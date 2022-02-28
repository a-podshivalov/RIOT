#include <stdio.h>
#include <string.h>

#include "lptimer.h"
#include "thread.h"

#include "bmx280.h"

#include "cayenne_lpp.h"

#include "net/loramac.h"     /* core loramac definitions */
#include "semtech_loramac.h" /* package API */

typedef enum {
    APP_MSG_SEND,
} app_message_types_t;

typedef struct {
    uint8_t *buffer;
    uint32_t length;
} lora_data_t;

static sx127x_params_t sx127x_params = {
    .nss_pin = SX127X_SPI_NSS,
    .spi = SX127X_SPI,

    .dio0_pin = SX127X_DIO0,
    .dio1_pin = SX127X_DIO1,
    .dio2_pin = SX127X_DIO2,
    .dio3_pin = SX127X_DIO3,
    .dio4_pin = SX127X_DIO4,
    .dio5_pin = SX127X_DIO5,
    .reset_pin = SX127X_RESET,
   
    .rfswitch_pin = SX127X_RFSWITCH,
};

static semtech_loramac_t loramac;  /* The loramac stack device descriptor */
static kernel_pid_t sender_pid;  /* Pid of a thread which controls data sending */
static kernel_pid_t loramac_pid;  /* Pid of a thread which controls data sending */

/* define the required keys for activation */
static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t appeui[LORAMAC_APPEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* we need AppKey for OTAA */
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static bmx280_t dev_bmx280;
static bmx280_params_t bme280_params = {
    .i2c_dev = I2C_DEV(1),
    .i2c_addr = 0x76,
    .t_sb = BMX280_SB_0_5,
    .filter = BMX280_FILTER_OFF,
    .run_mode = BMX280_MODE_FORCED,
    .temp_oversample = BMX280_OSRS_X1,
    .press_oversample = BMX280_OSRS_X1,
    .humid_oversample = BMX280_OSRS_X1,
};

static char bmp280_stack[THREAD_STACKSIZE_DEFAULT];
static cayenne_lpp_t bmp_lpp;

void *bmp280_thread(void *arg) {
    (void) arg;
    static msg_t data_msg = { .type = APP_MSG_SEND };
    static lora_data_t data;
    
    if (bmx280_init(&dev_bmx280, &bme280_params) == BMX280_OK) {
        puts("BME280 succesfully initialised!\r");
        
        lptimer_ticks32_t last_wakeup = lptimer_now();
        
        while(1){
            int t = bmx280_read_temperature(&dev_bmx280); /* degrees C * 100 */
            int h = bme280_read_humidity(&dev_bmx280); /* percents * 100 */
            int p = bmx280_read_pressure(&dev_bmx280)/10; /* Pa -> 0.1 mbar */

            //printf("T = %i.%i C, H = %i.%i %%, P = %i mbar\r\n", t/100, t%100, h/100, h%100, p/10);
            
            /* Put data into Cayenne LPP buffer */
            cayenne_lpp_reset(&bmp_lpp);
            cayenne_lpp_add_temperature(&bmp_lpp, 1, (int16_t) (t/10)); /* degrees C * 10 */
            cayenne_lpp_add_barometric_pressure(&bmp_lpp, 2, p);
            cayenne_lpp_add_relative_humidity(&bmp_lpp, 3, (int16_t) (h/50)); /* 1/2% */

            /* Send a message to a LoRaWAN thread */
            data.buffer = bmp_lpp.buffer;
            data.length = bmp_lpp.cursor;
            data_msg.content.ptr = &data;
            msg_send(&data_msg, sender_pid);
            
            lptimer_periodic_wakeup(&last_wakeup, 10000);
        }
    }
    
    return NULL;
}

#define MAX_RETR 10

int main(void){
    int res;
    
    /* Adjust message queue size */
    msg_t msg;
    msg_t msg_queue[8];
    msg_init_queue(msg_queue, 8);
    
    sx127x_params.rfswitch_active_level = SX127X_GET_RFSWITCH_ACTIVE_LEVEL();
    
    loramac_pid = semtech_loramac_init(&loramac, &sx127x_params);
    sender_pid = thread_getpid();
    
    /* Set the keys identifying the device */
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);
        
    semtech_loramac_set_tx_power(&loramac, 0);
    semtech_loramac_set_dr(&loramac, LORAMAC_DR_5);
    semtech_loramac_set_retries(&loramac, 0); /* Process failed TX attemts in the app */
    semtech_loramac_set_tx_mode(&loramac, LORAMAC_TX_CNF);   /* confirmed packets */
    semtech_loramac_set_tx_port(&loramac, LORAMAC_DEFAULT_TX_PORT); /* port 2 */
    semtech_loramac_set_class(&loramac, LORAMAC_CLASS_C); /* Always listen */
    
    puts("[LoRa] LoRaMAC initialised");
    
    do{
        lptimer_ticks32_t last_join_wakeup = lptimer_now();

        res = semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA);
                    
        switch (res) {
        case SEMTECH_LORAMAC_JOIN_SUCCEEDED: 
            puts("[LoRa] successfully joined to the network");
            break;
        
        case SEMTECH_LORAMAC_ALREADY_JOINED:
            /* ignore, can't be */
            break;
        
        case SEMTECH_LORAMAC_RESTRICTED:
        case SEMTECH_LORAMAC_BUSY:
        case SEMTECH_LORAMAC_NOT_JOINED:
        case SEMTECH_LORAMAC_JOIN_FAILED:
        case SEMTECH_LORAMAC_DUTYCYCLE_RESTRICTED:
            printf("[LoRa] LoRaMAC join failed: code %d\n", res);
            lptimer_periodic_wakeup(&last_join_wakeup, 10000);
            break;
        
        default:
            printf("[LoRa] join request: unknown response %d\n", res);
            break;
        }
    } while (res != SEMTECH_LORAMAC_JOIN_SUCCEEDED);
    
    /* Start another thread for sensor measurements */
    thread_create(bmp280_stack, sizeof(bmp280_stack),
              THREAD_PRIORITY_MAIN-1,
              THREAD_CREATE_STACKTEST,
              bmp280_thread,
              NULL,
              "Luminoсity");
    
    while (1) {
        msg_receive(&msg);

        if (msg.sender_pid != loramac_pid) {
            /* Application message */
            if (msg.type == APP_MSG_SEND) {
                lora_data_t *data = msg.content.ptr;
                (void) data;
                res = semtech_loramac_send(&loramac, data->buffer, data->length);

                switch (res) {
                case SEMTECH_LORAMAC_BUSY:
                    puts("[LoRa] MAC already busy");
                    break;
                case SEMTECH_LORAMAC_NOT_JOINED: 
                    puts("[LoRa] Not joined to the network");
                    break;
                case SEMTECH_LORAMAC_TX_OK:
                    puts("[LoRa] TX is in progress");
                    break;
                case SEMTECH_LORAMAC_DUTYCYCLE_RESTRICTED:
                    puts("[LoRa] TX duty cycle restricted");
                    break;
                default:
                    printf("[LoRa] Unknown send() response %d\n", res);
                    break;
                }
            }
        } else {
            switch (msg.type) {
                case MSG_TYPE_LORAMAC_TX_STATUS: 
                    if (msg.content.value == SEMTECH_LORAMAC_TX_DONE) {
                        puts("[LoRa] TX done");
                        break;
                    }
                    
                    if (msg.content.value == SEMTECH_LORAMAC_TX_CNF_FAILED) {
                        puts("[LoRa] Uplink confirmation failed");
                        /* TODO: rejoin if there are too many failures */
                        break;
                    }
                    
                    printf("[LoRa] Unknown TX status %lu\n", msg.content.value);
                    break;
                
                case MSG_TYPE_LORAMAC_RX: 
                    if ((loramac.rx_data.payload_len == 0) && loramac.rx_data.ack) {
                        printf("[LoRa] Ack received: RSSI %d, DR %d\n",
                                loramac.rx_data.rssi,
                                loramac.rx_data.datarate);
                    } else {
                        printf("[LoRa] Data received: %d bytes, port %d, RSSI %d, DR %d\n",
                                loramac.rx_data.payload_len,
                                loramac.rx_data.port,
                                loramac.rx_data.rssi,
                                loramac.rx_data.datarate);

                        printf("[LoRa] Hex data: ");
                        for (int l = 0; l < loramac.rx_data.payload_len; l++) {
                            printf("%02X ", loramac.rx_data.payload[l]);
                        }
                        printf("\n");

                        /* TODO: process data here */
                    }
                    break;
                
                case MSG_TYPE_LORAMAC_JOIN:
                    puts("[LoRa] LoRaMAC join notification\n");
                    break;
                    
                default:
                    printf("[LoRa] Unidentified LoRaMAC msg type %d\n", msg.type);
                    break;
            }
        }
    }
}
