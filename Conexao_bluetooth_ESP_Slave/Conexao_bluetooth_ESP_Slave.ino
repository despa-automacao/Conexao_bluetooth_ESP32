#include<esp_now.h>
#include<esp_task_wdt.h>
#include <max6675.h>
#include <WiFi.h>
#include <Wire.h>
//Definindo o tempo limite do WatchDog para 3 segundos
#define WDT_TIMEOUT 3
int last = millis();

//Declarando delay
unsigned long tempo_entre_leituras;

//Incluindo pinos sensores

int thermoDO1 = 27;
int thermoCS1 = 14;
int thermoCLK1 = 13;

int thermoDO2 = 25;
int thermoCS2 = 26;
int thermoCLK2 = 4;

int thermoDO3 = 19;
int thermoCS3 = 18;
int thermoCLK3 = 5;

int thermoDO4 = 35;
int thermoCS4 = 32;
int thermoCLK4 = 33;

MAX6675 thermocouple1(thermoCLK1, thermoCS1, thermoDO1);
MAX6675 thermocouple2(thermoCLK2, thermoCS2, thermoDO2);
MAX6675 thermocouple3(thermoCLK3, thermoCS3, thermoDO3);
MAX6675 thermocouple4(thermoCLK4, thermoCS4, thermoDO4);

//Criando estrutura de dados 
typedef struct struct_message {
  float temp1, temp2, temp3, temp4;
} struct_message;
//Timer de reset ESP32
unsigned long communication_timer;

//criando objeto estrutura de dados
struct_message message;

//Endereço MAC do par EC:94:CB:6F:31:A8

uint8_t broadcastAddress[] = {0xEC, 0x94, 0xCB, 0x6F, 0x31, 0xA8};

//informações do par
esp_now_peer_info_t peerInfo;

//Funcção de callback para quando os dados são enviados
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if(status == ESP_NOW_SEND_SUCCESS) communication_timer = millis();
}

void setup() {
  //inicialização serial e I2C
  Wire.begin();
  Serial.begin(9600);

  //Wifi para station
  WiFi.mode(WIFI_STA);

  //inicializando ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //Registrando função de callback
  esp_now_register_send_cb(OnDataSent);

  //Registrando Par
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  //Adicionando par
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //Habilitando "Panic" para o ESP32 reiniciar
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  communication_timer = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (millis() - tempo_entre_leituras > 500) {
    Serial.print("C1 = ");
    Serial.println(thermocouple1.readCelsius());
    delay(500);
    Serial.print("C2 = ");
    Serial.println(thermocouple2.readCelsius());
    delay(500);
    Serial.print("C3 = ");
    Serial.println(thermocouple3.readCelsius());
    delay(500);
    Serial.print("C4 = ");
    Serial.println(thermocouple4.readCelsius());
   
      //Formatando estrutura de dados
      message.temp1 = thermocouple1.readCelsius();
      message.temp2 = thermocouple2.readCelsius();
      message.temp3 = thermocouple3.readCelsius();
      message.temp4 = thermocouple4.readCelsius();
    
    //Enviando mensagem via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*) &message, sizeof(message));

    if (result == ESP_OK) {
      Serial.println("Sending confirmed");
    }
    else {
      Serial.println("Sending error");
    }
    tempo_entre_leituras = millis();
  }
  //Reiniciando ESP-32 em caso de perda de comunicação bluetooth
  if(millis() - communication_timer > 28000){
    Serial.println("Falha na comunicação entre os ESP-32, reiniciando");
    while(1){
      communication_timer = millis();
      ESP.restart();
      }
    }
 if (millis() - last >= 2000) {
      Serial.println("Resetting WDT...");
      esp_task_wdt_reset();
      last = millis();
  }
}
