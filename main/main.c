#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "DHT.h"
#include "i2c-lcd.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/i2c.h"
#include "driver/rtc_io.h"

#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

// Constants
#define I2C_MASTER_PORT I2C_NUM_0
#define SDA_PIN GPIO_NUM_21 // LCD 1602 PIN // LCD 1602 HAS TO USE 5V VCC
#define SCL_PIN GPIO_NUM_22 // LCD 1602 PIN // LCD 1602 HAS TO USE 5V VCC
#define DRY_ADC_VALUE 4095.0
#define WET_ADC_VALUE 1400.0
#define SOIL_MOISTURE_PERCENTAGE 5 // SOID MOISTURE TO TURN ON OR OFF THE PUMP
#define WET_SOIL_MOISTURE 100.0
#define LCD_UPDATE_DELAY_MS 1500
#define DHT_GPIO_PIN 13 // DHT11 DATA PIN // DHT11 SENSOR HAS TO USE 3.3V VCC 
#define PUMP_GPIO_PIN 12 // PUMP PIN
// GPIO 34 IS SOIL MOISTURE SENSOR DATA PIN
// SOIL MOISTURE SENSOR HAS TO USE 3.3V VCC

static esp_adc_cal_characteristics_t adc1_chars;
char buffer[100];
int level = 0;

int counter = 0; //set timer to tunn on PUMP
int pumpMode = 0;
// counter = 0 => pumpMode = 0 => The pump operates in automatic mode
// counter > 0 => pumpMode = 1 => The pump operates in user-controlled mode

char timerRun[] = ""; // timer to turn on pump

typedef struct {
    double Temp;
    double Humi;
} dataDHT;

dataDHT dht11; // data DHT11
float soil_moisture_percentage; // data soid moisture sensor

#define WIFI_SSID "Hoang" // access point name
#define WIFI_PASS "12345689" // access point password
#define BROKER_URL "mqtt://ahihi:aio_Zqmr890i9ayyOB9x4tPcygNAUnVX@io.adafruit.com" // url of Adafruit server
#define TEMPERATURE "ahihi/feeds/temperature" // temperature is sent to this FEED
#define HUMIDITY "ahihi/feeds/humidity" // humidity is sent to this FEED
#define SOIL_MOISTURE "ahihi/feeds/soilmoisture"
#define PUMP "ahihi/feeds/pump"

static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_handle_t mqtt_client;
char * data = "none"; // data from feed "ahihi/feeds/co"
char * cTime = ""; // time to start turning on the pump
char * realTime = ""; 
long lTime = 0; // time to start turning on the pump
long timeTurnOnPump = 0; //the amount of time the pump is on
int checkReceiveData = 0;
int counter_second; 
char Current_Date_Time[100];
int setSendPumpData = 0;

//handling events about mqtt protocol
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    int msg_id = 0;
    // esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(mqtt_client, "ahihi/feeds/control", 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            data = event -> data;
            checkReceiveData = 1;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    //ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

//handling events about wifi connection
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}
// connecting wifi
void wifi_connection()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
}

// connect ESP32 to Adafruit server via mqtt protocol
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = BROKER_URL,
        },
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

/** 
 * @brief Initialize I2C master 
 */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    i2c_param_config(I2C_MASTER_PORT, &conf);
    return i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
}

/** 
 * @brief Initialize ADC 
 */
static void adc_init(void)
{
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11)); // GPIO 34
}

/** 
 * @brief Read soil moisture percentage 
 */
static float read_soil_moisture_percentage(void)
{
    uint32_t voltage = adc1_get_raw(ADC1_CHANNEL_6); // GPIO 34
    float soil_moisture_percentage = (DRY_ADC_VALUE - voltage) / (DRY_ADC_VALUE - WET_ADC_VALUE) * WET_SOIL_MOISTURE;

    if (soil_moisture_percentage > 100)
    {
        soil_moisture_percentage = 100;
    }

    //vTaskDelay(3000 / portTICK_PERIOD_MS);
    return soil_moisture_percentage;
}

/** 
 * @brief Update LCD display 
 */
static void update_lcd_display(float soil_moisture_percentage, double temperature, double humidity)
{
    sprintf(buffer, "Soil: %.2f %%", soil_moisture_percentage);
    lcd_put_cur(0, 0); // Print soil moisture on the first line
    lcd_send_string(buffer);

    sprintf(buffer, "T:%.2fC H:%.2f%% ", temperature, humidity);
    lcd_put_cur(1, 0); // Print temperature and humidity on the second line
    lcd_send_string(buffer);
}

/** 
 * @brief Read data DHT11
 */

void readDataDHT11(void)
{
        dht_read_data(DHT_GPIO_PIN, &dht11.Humi, &dht11.Temp);
        //vTaskDelay(1500 / portTICK_PERIOD_MS);
}

void handleData(void){
    char* inputString = strdup(data);
    const char delimiter[] = " ";
    char *token;
    token = strtok(inputString, delimiter);
    char * time1 = token;
    char * time2 = "";
    while (token != NULL) {
        time2 = token;
        token = strtok(NULL, delimiter);
    }
    printf("%s %s\n", time1, time2);
    char*output;
    timeTurnOnPump = strtol(time2, &output, 10);
    cTime = time1;
}

// void handleData1(void){
//     char* inputString = strdup(data);
//     const char delimiter[] = " ";
//     char *token;
//     token = strtok(inputString, delimiter);
//     char * time1 = token;
//     char * time2 = "";
//     while (token != NULL) {
//         time2 = token;
//         token = strtok(NULL, delimiter);
//     }
//     printf("%s %s\n", time1, time2);
//     char*output;
//     lTime = strtol(time1, &output, 10);
//     timeTurnOnPump = strtol(time2, &output, 10);
// }

/** 
 * @brief Get real time
 */
void get_current_time() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(Current_Date_Time, sizeof(Current_Date_Time), "%d/%m/%Y-%H:%M:%S", &timeinfo);
    printf("Current time is: %s\n", Current_Date_Time);
}

/** 
 * @brief Control the pumper
 */
// void controlPump(float soil_moisture_percentage){
//     if(checkReceiveData == 1){
//         handleData1();
//         checkReceiveData = 0;
//         counter_second = 0;
//     }

//     if(lTime == counter_second && timeTurnOnPump !=0){
//         counter = timeTurnOnPump;
//     }
//     counter_second ++;

//     if(counter > 0){
//         pumpMode = 1;
//         counter --;
//     }
//     else{
//         pumpMode = 0;
//         counter = 0;
//     }

//     if(pumpMode == 1) gpio_set_level (PUMP_GPIO_PIN , 1);
//     else{
//         if(soil_moisture_percentage > SOIL_MOISTURE_PERCENTAGE){
//             gpio_set_level (PUMP_GPIO_PIN , 0);
//         }
//         else{
//             gpio_set_level (PUMP_GPIO_PIN , 1);
//         }
//     }
// }
/** 
 * @brief Control the pumper
 */
void controlPump1(float soil_moisture_percentage){
    int msg_id;
    if(checkReceiveData == 1){
        handleData();
        checkReceiveData = 0;
    }
    int compare = strcmp(Current_Date_Time, cTime);
    if(compare == 0 && timeTurnOnPump !=0){
        counter = timeTurnOnPump;
    }

    if(counter > 0){
        pumpMode = 1;
        counter --;
    }
    else{
        pumpMode = 0;
        counter = 0;
    }

    if(pumpMode == 1) {
        gpio_set_level (PUMP_GPIO_PIN , 1);
        if(setSendPumpData == 0) {
            msg_id = esp_mqtt_client_publish(mqtt_client, PUMP, "1", 0, 1, 0);
            setSendPumpData = 1;
        }
    }
    else{
        if(soil_moisture_percentage > SOIL_MOISTURE_PERCENTAGE){
            gpio_set_level (PUMP_GPIO_PIN , 0);
            if(setSendPumpData == 1) {
                msg_id = esp_mqtt_client_publish(mqtt_client, PUMP, "0", 0, 1, 0);
                setSendPumpData = 0;
            }
        }
        else{
            gpio_set_level (PUMP_GPIO_PIN , 1);
            if(setSendPumpData == 0) {
                msg_id = esp_mqtt_client_publish(mqtt_client, PUMP, "1", 0, 1, 0);
                setSendPumpData = 1;
            }
        }
    }
}


/** 
 * @brief Control the system
 */
void smartFarm(void *pvParameter){
    while (1)
    {
        soil_moisture_percentage = read_soil_moisture_percentage();
        readDataDHT11();
        get_current_time();
        printf("Soil Moisture Percentage is: %.2f %% \n", soil_moisture_percentage);
        printf("Temp=%f, Humi=%f\r\n", dht11.Temp, dht11.Humi);
        printf("timeToTurnOn = %ld ; cTime = %s\n",timeTurnOnPump, cTime);
        update_lcd_display(soil_moisture_percentage, dht11.Temp, dht11.Humi);
        controlPump1(soil_moisture_percentage);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void getRealTime(void *pvParameter){
    while (1)
    {
        get_current_time();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

//send data from DHT to Adafruit every 10 seconds
void mainTask(void *pvParameters)
{
    char str[32];
    int msg_id;

    while (1)
    {
        //dht_read_data(DHT_GPIO_PIN, &humidity, &temperature);
        printf("Temp: %f ; Humi: %f; Soil moisture: %.2f %% \n", dht11.Temp, dht11.Humi, soil_moisture_percentage);
        sprintf(str, "%0.2f", dht11.Temp);
        msg_id = esp_mqtt_client_publish(mqtt_client, TEMPERATURE, str, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        sprintf(str, "%0.2f", dht11.Humi);
        msg_id = esp_mqtt_client_publish(mqtt_client, HUMIDITY, str, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        sprintf(str, "%0.2f", soil_moisture_percentage);
        msg_id = esp_mqtt_client_publish(mqtt_client, SOIL_MOISTURE, str, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    lcd_init();
    lcd_clear();
    adc_init();
    esp_rom_gpio_pad_select_gpio( PUMP_GPIO_PIN ) ;
    gpio_set_direction ( PUMP_GPIO_PIN , GPIO_MODE_OUTPUT ) ;
    gpio_set_level (PUMP_GPIO_PIN , 0);
    wifi_connection();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    mqtt_app_start();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init(); 
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }  
    setenv("TZ", "UTC-7", 1);
    tzset();
    int msg_id = esp_mqtt_client_publish(mqtt_client, PUMP, "0", 0, 1, 0);
    //xTaskCreate(&getRealTime, "get real time", 2048*2, NULL, 5, NULL);
    xTaskCreate(&mainTask, "mainTask", 2048*2, NULL, 1, NULL);
    //xTaskCreate(&readData, "Read Data", 2048, NULL, 2, NULL);
    xTaskCreate(&smartFarm, "Smart Farm", 2048, NULL, 3, NULL);
}