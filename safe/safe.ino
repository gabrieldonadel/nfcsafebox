/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <MFRC522.h>


#include <Servo.h>

Servo servo;


//Inicializa a biblioteca utilizando as portas de 8 a 11 para 
//ligacao ao motor 
  


ESP8266WiFiMulti WiFiMulti;
WiFiServer server(80);
char user[50] = "Doni";
unsigned long uid;
bool isDoorOpen = false;

#define RST_PIN         D3         // Configurable, see typical pin layout above
#define SS_PIN          D8        // Configurable, see typical pin layout above
 
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

unsigned long getID(){
  if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
    return -1;
  }
  unsigned long hex_num;
  hex_num =  mfrc522.uid.uidByte[0] << 24;
  hex_num += mfrc522.uid.uidByte[1] << 16;
  hex_num += mfrc522.uid.uidByte[2] <<  8;
  hex_num += mfrc522.uid.uidByte[3];
  mfrc522.PICC_HaltA(); // Stop reading
  return hex_num;
}
 

void setup() {


  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.printf("port d1 %d, port d7 %d\n", D1,D7);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("D0N1", "abcdef1234");

  // Inicia o Servidor no qual iremos ver no navegador e poder acionar o Relé
  server.begin();
  Serial.println("Servidor Iniciado");
 
  // Retorna o Valor do IP que estará nosso servidor na Rede.
  Serial.print("Usar essa URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  servo.attach(D9);
  servo.write(0);
}

void loop() {


  

  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    // Look for new cards
    if (mfrc522.PICC_IsNewCardPresent()) { 
        uid = getID();
        if(uid != -1){
          Serial.print("Card detected, UID: "); Serial.println(uid); 
          
           //Gira o motor no sentido anti-horario a 120 graus
        if(isDoorOpen){
          servo.write(0);
          }
         else{
          servo.write(180);
          }

          isDoorOpen = !isDoorOpen;
   
          Serial.print((String)"Door is Open "+isDoorOpen);
        } 
    
 
    }
 
 
    WiFiClient client = server.available();
    if (!client) {
      return;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(""); // Deixei em branco para quem adivinhar o que faz aqui. Não afeta o código, mas gera algo interessante (:
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
     
    client.print((String)"<body><h1> Bem vindo "+user+"</h1>");
    client.print("<p>Status da porta:");
    if(isDoorOpen){
      client.print("Aberta"); 
    }else{
      client.print("Fechada");
      }
    client.print("</p>");
    client.print(uid);
    client.print("</p>");
    client.print("</body>");
     
  }  
}
