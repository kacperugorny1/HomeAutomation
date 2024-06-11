#include <stdio.h> //for basic printf commands
#include <string.h> //for handling strings
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_log.h" //for showing logs
#include "esp_timer.h" // for esp_timer_get_time
#include "esp_event.h" //for wifi event
#include "nvs_flash.h" //non volatile storage
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps
#include "mqtt_client.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "math.h"

/*
TODO:
mutex on expectdata and send set.
*/
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define UART UART_NUM_2

#define DATALEN 2
static const int RX_BUF_SIZE = 1024;

static const char *TAG = "MQTT_TCP";
// const char *ssid = "internet_dom1";
// const char *pass = "1qaz2wsx";
const char *ssid = "UPC94DE76D";
const char *pass = "Vv5re2masfmk";
// const char *mqtt_addres = "mqtt://192.168.0.242:1883"; //wro
const char *mqtt_addres = "mqtt://185.201.114.232:1883"; //goszcz


volatile bool WifiConnected = false;
volatile bool mqttConnected = false;
int retry_num = 0;
volatile char* volatile data;
int expect_address = -1;
volatile bool send = false;
volatile bool expectdata = false;
bool first_ignore = true;
volatile int64_t tm = 0;

esp_mqtt_client_handle_t client;
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void wifi_connection();
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
static void mqtt_app_start();
static void uart_rx_task(void *arg);
static void uart_tx_task(void *arg);
static void uart_configure();

void app_main() {
    nvs_flash_init();
    wifi_connection();
    esp_timer_early_init();
    data = (char *)malloc(sizeof(char)*2);
    while(!WifiConnected) vTaskDelay(100/portTICK_PERIOD_MS);
    mqtt_app_start();
    while(!mqttConnected) vTaskDelay(100/portTICK_PERIOD_MS);
    uart_configure();
    xTaskCreate(uart_tx_task, "uart_tx_task", 4096, NULL, 10, NULL);
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 10, NULL);

}
static void uart_rx_task(void *arg){
    char* Rxdata = (char *) malloc(sizeof(char) * 512);
    char str[1024] = "";
    char help[100] = "";
    while(1){
        int l = uart_read_bytes(UART_NUM_2, (void *)Rxdata, 512,200/portTICK_PERIOD_MS);
        if(esp_timer_get_time() - tm > 300000) expectdata = false;
        if(l != 0 && expectdata){
            for(int i = 0; i < l; ++i){
                if(expect_address != -1 && l == 1)
                    sprintf(help,"u%d: "BYTE_TO_BINARY_PATTERN" ",expect_address,BYTE_TO_BINARY(Rxdata[i]));
                else{
                    sprintf(help,"u%d: "BYTE_TO_BINARY_PATTERN" ",Rxdata[i],BYTE_TO_BINARY(Rxdata[i + 1]));
                    i++;
                }
                strcat(str,help);
            }
            printf(str);
            esp_mqtt_client_publish(client,"test",str,0,0,0);
            strcpy(str,"");
            expect_address = -1;
            expectdata = false;
        }
    }
}

static void uart_tx_task(void *arg){
    while (1) {
        //change data to queue or smth like this
        vTaskDelay(200 / portTICK_PERIOD_MS);
        if(send == true && expectdata == false){
            printf("Sending %d %d\n",data[0], data[1]);
            if(data[0] == 0xFF) {
                expectdata = true;
                if(data[1] != 0xFF && data[1] != 0xFE) expect_address = data[1];
            }
            send = false;
            uart_write_bytes(UART, data, DATALEN);
        }

        //periodic read leds
        if(esp_timer_get_time() - tm > 1000000){
            // wait for all data to come
            while(expectdata == true) vTaskDelay(100);
            expect_address = -1;
            expectdata = true;
            data[0] = 0xFF;
            data[1] = 0xFF;
            uart_write_bytes(UART, data, DATALEN);
            tm = esp_timer_get_time();
        }

    }
}

static void uart_configure()
{
    uart_config_t uart2_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART, &uart2_config);
    uart_set_pin(UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);


    // ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart2_config));
    // ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 23, 22, 0, 0));
    // ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0));
    
    // char ReceviedData[10];
    
	
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_addres
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    mqtt_event_handler_cb(event_data);
}
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event){ //here esp_mqtt_event_handle_t is a struct which receieves struct event from mqtt app start funtion
  esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, "test", 0);
        esp_mqtt_client_publish(client,"test","esp32 connected",0,0,0);
        mqttConnected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("\nTOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        bool mistake_in_data = false;
        if(event->data_len == 5 && event->data[0] == 'h' && event->data[1] == 'e') esp_mqtt_client_publish(client,"test","esp32 connected",0,0,0);
        if(event->data_len == 17 && !first_ignore){
            data[0] = 0;
            data[1] = 0;
            for(int i = 0; i < event->data_len; ++i)
            {
                if(i == 8) continue;
                if((event->data[i] - 48 > 1 || event->data[i] - 48 < 0)){
                    mistake_in_data = true;
                }
                else {
                    data[i/9] |= (event->data[i]-48)<<(8 - (i%9) - 1);
                }
            }            
            if(!mistake_in_data)
                send = true;
        }
        else if(first_ignore) first_ignore = false;
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




static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
    if(event_id == WIFI_EVENT_STA_START)
    {
    printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
    printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
    printf("WiFi lost connection\n");
    if(retry_num<5){esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");}
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
    printf("Wifi got IP...\n\n");
    WifiConnected = true;
    }
}


void wifi_connection(){
    esp_netif_init(); //network interdace initialization
    esp_event_loop_create_default(); //responsible for handling and dispatching events
    esp_netif_create_default_wifi_sta(); //sets up necessary data structs for wifi station interface
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();//sets up wifi wifi_init_config struct with default values
    esp_wifi_init(&wifi_initiation); //wifi initialised with dafault wifi_initiation
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);//creating event handler register for wifi
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);//creating event handler register for ip event
    wifi_config_t wifi_configuration ={ //struct wifi_config_t var wifi_configuration
    .sta= {
        .ssid = "",
        .password= "", /*we are sending a const char of ssid and password which we will strcpy in following line so leaving it blank*/ 
    }//also this part is used if you donot want to use Kconfig.projbuild
    };
    strcpy((char*)wifi_configuration.sta.ssid,ssid); // copy chars from hardcoded configs to struct
    strcpy((char*)wifi_configuration.sta.password,pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);//setting up configs when event ESP_IF_WIFI_STA
    esp_wifi_start();//start connection with configurations provided in funtion
    esp_wifi_set_mode(WIFI_MODE_STA);//station mode selected
    esp_wifi_connect(); //connect with saved ssid and pass
    printf( "wifi_init_softap finished. SSID:%s  password:%s",ssid,pass);
}