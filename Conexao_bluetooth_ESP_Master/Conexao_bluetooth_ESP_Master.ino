#include <RTClib.h>
#include <esp_now.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>

//Definições SD VSPI
#define vspi
#ifdef vspi
SPIClass sdSPI(VSPI);
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCLK 18
#define SD_CS 5

#endif

//WatchDog
#define WDT_TIMEOUT 3
int lastWD = millis();

//NANO RESET
const int resetPin = 13;
unsigned long verificationTime = millis();
int contaNano = 0;

//Dados Uart
#define RX0  16
#define TX0  17
String v1;
HardwareSerial Reciever(1);
const unsigned int MAX_MESSAGE_LENGTH = 12;
unsigned long tempo_entre_leituras = 0;
unsigned long tempo_entre_impressoes = 0;
bool uartConectado = false;

//criação objeto LCD
LiquidCrystal_I2C lcd(0x27, 24, 4);

//Criação objeto RTC
RTC_DS1307 rtc;
DateTime tstamp;

//Definindo estrutura de dados
typedef struct struct_message {
  float temp1, temp2, temp3, temp4;
} struct_message;
//timer de reset ESP
unsigned long communication_timer;

//Variáveis de temperatura atual
float t1Atual = 0; float t2Atual = 0; float t3Atual = 0; float t4Atual = 0;
double v1Atual;


//Criando objeto da estrutura
struct_message mensagem;

//Inicio Funções SD VSPI -----------------------------------------------
String dataMessage;
unsigned long tempo_entre_gravacoes = 0;
//Função para inicio do SD - Chamada no setup
void SD_init() {
  //Montando sistema de arquivos
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Memory card mount failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMORY CARD MOUNT FAILED");

  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("SD ausente");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD AUSENTE");

  }
  else if (cardType == CARD_MMC || cardType == CARD_SD || cardType == CARD_SDHC) {
    Serial.println("SD montado");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD MONTADO");
  }
}
//Função emendar texto - Chamada no loop
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");

  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
//Função para escrever texto no SD
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}
//Função para Imprimir os dados dos sensores no arquivo datalog.txt
void logSDCard() {
  dataMessage = "TS = " + String(t1Atual) + "ºC; TI = " + String(t2Atual) + "ºC; TE = " + String(t3Atual) + "ºC; TS = " +
                String(t4Atual) + "ºC; VAZAO = " + String(v1Atual) + "L/min; Horario = " + String(tstamp.hour()) + ":" +
                String(tstamp.minute()) + ":" + String(tstamp.second()) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/datalog.txt", dataMessage.c_str());
}
//final das funções SD -----------------------------------------------------

//Callback para recebimento dos dados ESP NOW
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&mensagem, incomingData, sizeof(mensagem));
  Serial.print("Data recieved ");
  Serial.println(len);
  Serial.print("Temp 1: ");
  Serial.println(mensagem.temp1);
  t1Atual = mensagem.temp1;
  Serial.print("Temp 2: ");
  Serial.println(mensagem.temp2);
  t2Atual = mensagem.temp2;
  Serial.print("Temp 3: ");
  Serial.println(mensagem.temp3);
  t3Atual = mensagem.temp3;
  Serial.print("Temp 4: ");
  Serial.println(mensagem.temp4);
  t4Atual = mensagem.temp4;
  Serial.println();
  communication_timer = millis();
  delay(1000);
}



void setup() {
  //Iniciando RTC
  Wire.begin();
  rtc.begin();
  //Iniciando Serial Monitor ESP
  Serial.begin(9600);
  Reciever.begin(9600, SERIAL_8N1, RX0, TX0);
  //Verificação RTC
  if (!rtc.isrunning()) {
    Serial.println("RTC parado, vou ajustar com a hora da compilacao...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //Iniciando LCD
  lcd.begin();
  lcd.backlight();

  //Trocando WIFI para modo station
  WiFi.mode(WIFI_STA);
  Serial.println("Wifi Iniciado");

  //Iniciando ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
    //Register callback function
    esp_now_register_recv_cb(OnDataRecv);
  }

  //Iniciando SD VSPI
#ifdef vspi
  Serial.println(" Please insert the memory card ");
  delay(12000);
#endif
  SD_init(); //initialization SD function
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  lcd.clear();
  //Iniciando WatchDog
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  communication_timer = millis();

  //Verificando se o arquivo datalog.txt existe
  File file = SD.open("/datalog.txt");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/datalog.txt", "Leituras de Temperatura, Vazao e Horario \r\n");
  }
  else {
    Serial.println("File already exists");



  }
}
void printnn(int n) {
  // imprime um numero com 2 digitos
  // acrescenta zero `a esquerda se necessario
  String digitos = String(n);
  if (digitos.length() == 1) {
    digitos = "0" + digitos;
  }
  lcd.print(digitos);
}




void loop() {

  if (Reciever.available() > 0) uartConectado = true;
  //Variavel passada por Uart
  while (Reciever.available() > 0)
  {
    //Create a place to hold the incoming message
    static char messageV[MAX_MESSAGE_LENGTH];
    static unsigned int message_pos = 0;

    //Read the next available byte in the serial receive buffer
    char inByte = Reciever.read();

    //Message coming in (check not terminating character) and guard for over message size
    if ( inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1) )
    {
      //Add the incoming byte to our message
      messageV[message_pos] = inByte;
      message_pos++;
    }
    //Full message received...
    else
    {
      //Add null character to string
      messageV[message_pos] = '\0';

      //Print the message (or do other things)
      v1 = messageV;
      v1Atual = v1.toDouble();
      Serial.println(v1);
      Serial.println(v1Atual);
      verificationTime = millis();
      //Reset for the next message
      message_pos = 0;
    }
  }
  while (Reciever.available() == 0 && millis() - verificationTime > 5000) {
    pinMode(resetPin, OUTPUT);        //This "Activates" pin 12,
    digitalWrite(resetPin, HIGH);      //Puts the Reset pin LOW (as it needs),
    delay(500);                       //To allow each mode to change
    digitalWrite(resetPin, LOW);      //Puts the Reset pin LOW (as it needs),
    Serial.println("Falha de comunicação UART, reiniciando");
    verificationTime = millis();

  }
  if (millis() - tempo_entre_impressoes > 300) {
    //print LCD
    lcd.clear();
    lcd.print("Reator");
    lcd.setCursor(10, 0);
    lcd.print("AR");
    //Impressão T1
    lcd.setCursor(0, 1);
    lcd.print("TS="); lcd.print(t1Atual, 0);  lcd.print("C");
    //Impressão T2
    lcd.setCursor(0, 2);
    lcd.print("TI="); lcd.print(t2Atual, 0);  lcd.print("C");
    //Impressão T3
    lcd.setCursor(10, 1);
    lcd.print("TE="); lcd.print(t3Atual, 0);  lcd.print("C");
    //Impressão T4
    lcd.setCursor(10, 2);
    lcd.print("TS="); lcd.print(t4Atual, 0);  lcd.print("C");
    //Impressão V1
    lcd.setCursor(0, 3);
    lcd.print("VZ="); lcd.print(v1Atual); lcd.print("L/min");; lcd.print(" ");
    //Obter tempo real, armazenando dados em tstamp
    lcd.setCursor(15, 3);
    tstamp = rtc.now();
    printnn(tstamp.hour());
    lcd.print(':');
    printnn(tstamp.minute());
    tempo_entre_impressoes = millis();
  }
  if (uartConectado == true) {
    if (millis() - tempo_entre_gravacoes > 20000 ) {


      logSDCard();

      tempo_entre_gravacoes = millis();
    }
  }
  //Reiniciando ESP-32 em caso de perda de comunicação bluetooth
  if (millis() - communication_timer > 28000) {
    Serial.println("Falha na comunicação entre os ESP-32, reiniciando");
    while (1) {
      communication_timer = millis();
      ESP.restart();
    }
  }

  if (millis() - lastWD >= 2000) {
    Serial.println("Resetting WDT...");
    esp_task_wdt_reset();
    lastWD = millis();

    uartConectado = false;
  }
}
