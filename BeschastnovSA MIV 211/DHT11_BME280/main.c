#include <stdio.h>
#include "stdint.h"

#include "periph/i2c.h"
#include "bmx280.h"
#include "lptimer.h"
#include "dht.h"
#include "dht_params.h"
#include <board.h>
#include "fmt.h"

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

static dht_params_t params[] = {{.pin = UNWD_GPIO_7}};

int main(void) {
    dht_t dev;
    int16_t temp, hum;
    char temp_s[10];
    char hum_s[10];


    if (bmx280_init(&dev_bmx280, &bme280_params) != BMX280_OK) {
        puts("BME280 was not succesfully initialised!\n\r");
        return 1;
    }

    if (dht_init(&dev, &params[0]) != DHT_OK) {
        puts("[Failed]\n\r");
        return 1;
    }
        
    while(true){

        if (dht_read(&dev, &temp, &hum) != DHT_OK ) {
            puts("Error reading DHT values\n\r");
            continue;
        }

        int t = bmx280_read_temperature(&dev_bmx280); /* degrees C * 100 */
        int h = bme280_read_humidity(&dev_bmx280); /* percents * 100 */

        size_t n = fmt_s16_dfp(temp_s, temp, -1);
        temp_s[n] = '\0';
        n = fmt_s16_dfp(hum_s, hum, -1);
        hum_s[n] = '\0';

        printf("DHT\n\rhumidity: %s%%, Temperature: %s C\n\rBME280\n\rHumidity: H = %i.%i %%, Temperature: T = %i.%i C\n\n\n\r", hum_s, temp_s, h/100, h%100, t/100, t%100);

        lptimer_sleep(4000);
    }

    return 0;
}
