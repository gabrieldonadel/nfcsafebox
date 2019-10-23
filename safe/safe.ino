/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;
WiFiServer server(80);
char user[50] = "Doni";

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("donadel", "123456789");

  // Inicia o Servidor no qual iremos ver no navegador e poder acionar o Relé
  server.begin();
  Serial.println("Servidor Iniciado");
 
  // Retorna o Valor do IP que estará nosso servidor na Rede.
  Serial.print("Usar essa URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {
 
    WiFiClient client = server.available();
    if (!client) {
      return;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(""); // Deixei em branco para quem adivinhar o que faz aqui. Não afeta o código, mas gera algo interessante (:
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
     
    client.print((String)"<body><h1> Bem vindo"+user+"</h1><p>Status da porta: Aberta</p></body>");
    
    delay(3000);
  }  
}
