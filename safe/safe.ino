//Includes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <EEPROM.h>            // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>               // RC522 Module uses SPI protocol
#include <MFRC522.h>           // Library for Mifare RC522 Devices
#include <Servo.h>             // Control the micro servo


//Constants

#define wipeB A0     // Button pin for WipeMode
#define redLed D14    // Set Led Pins
#define greenLed D2 
#define yellowLed D10
#define RST_PIN         D3         // Configurable, see typical pin layout above
#define SS_PIN          D8        // Configurable, see typical pin layout above  
#define LED_ON HIGH
#define LED_OFF LOW 


//Global variables
Servo servo;
ESP8266WiFiMulti WiFiMulti;
WiFiServer server(80);
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

bool programMode = false;  // initialize programming mode to false
uint8_t successRead;       // Variable integer to keep if we have Successful Read from Reader
byte storedCard[4];        // Stores an ID read from EEPROM
byte readCard[4];          // Stores scanned ID read from RFID Module
byte masterCard[4];        // Stores master card's ID read from EEPROM 
bool isDoorOpen = false;
int httpMode = 0;
char data[50];


void setup() { 
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(wipeB, INPUT);   // Enable pin's pull up resistor
  digitalWrite(redLed, LED_OFF);  // Make sure led is off
  digitalWrite(greenLed, LED_OFF);  // Make sure led is off
  digitalWrite(yellowLed, LED_OFF); // Make sure led is off

  Serial.begin(115200); 
  EEPROM.begin(2048);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  // START WIPING
  //Wipe Code - If the Button (wipeB) Pressed while setup run (powered on) it wipes EEPROM
  if (analogRead(wipeB) >= 1000) {                              // when button pressed pin should get low, button connected to ground
    digitalWrite(redLed, LED_ON);                               // Red Led stays on to inform user we are going to wipe
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    bool buttonState = monitorWipeButton(6000);                 // Give user enough time to cancel operation
    if (buttonState == true && analogRead(wipeB) >= 1000) {     // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {                              //If EEPROM address 0
                                                                // do nothing, already clear, go to the next address in order to save time
                                                                //and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);                                   // if not write 0 to clear, it takes 3.3mS
        }
        EEPROM.commit();  
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      digitalWrite(redLed, LED_OFF);  // visualize a successful wipe
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
    }
    else {
      Serial.println(F("Wiping Cancelled")); // Show some feedback that the wipe button did not pressed for 15 seconds
      digitalWrite(redLed, LED_OFF);
    }
  }
  //END WIPING

  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'

  if (EEPROM.read(1) != 143) {
      Serial.println(F("No Master Card Defined"));
      Serial.println(F("Scan A PICC to Define as Master Card"));
      do {
        successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
        digitalWrite(yellowLed, LED_ON);    // Visualize Master Card need to be defined
        delay(200);
        digitalWrite(yellowLed, LED_OFF);
        delay(200);
      }
      while (!successRead);                  // Program will not go further while you not get a successful read
      for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
        EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
      }
      EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
      EEPROM.commit();  
      Serial.println(F("Master Card Defined"));
    }

    Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
  //EEPROM LOADED

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t); 
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("D0N1", "abcdef1234");

  // Inicia o Servidor no qual iremos ver no navegador e poder acionar o Relé
  server.begin();
  Serial.println("Servver Started");
 
  // Retorna o Valor do IP que estará nosso servidor na Rede.
  Serial.print("Usar essa URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  servo.attach(D9);
  servo.write(0);
}

void loop() {
  do { 
      renderServer(httpMode); 
    
    
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
   if (analogRead(wipeB) >= 1000) { // Check if button is pressed
      // Visualize normal operation is iterrupted by pressing wipe button Red is like more Warning to user
      digitalWrite(redLed, LED_ON);  // Make sure led is off
      digitalWrite(greenLed, LED_OFF);  // Make sure led is off
      digitalWrite(yellowLed, LED_OFF); // Make sure led is off
      // Give some feedback
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
      if (buttonState == true && analogRead(wipeB) >= 1000) {    // If button still be pressed, wipe EEPROM
        EEPROM.write(1, 0);                  // Reset Magic Number.
        EEPROM.commit();  
        Serial.println(F("Master Card Erased from device"));
        Serial.println(F("Please reset to re-program Master Card"));
        while (1);
      }
      Serial.println(F("Master Card Erase Cancelled"));
    } if (programMode) {
      cycleLeds();              // Program Mode cycles through Red Green Blue waiting to read a new card
    }else{
      normalModeOn();
    }
  }while (!successRead);  
  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      httpMode = 0;
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  } else {
    if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      httpMode = 1;
      Serial.println(F("Hello Master - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));
        granted();         // Open the door 
        sendPostRequest();
      }
      else {      // If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        denied();
      }
    }
    
    renderServer(httpMode);
  }
      
}

void renderServer(int type){
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClient client = server.available();
    if (!client) {
      return;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");
    client.println("<!DOCTYPE HTML>");
    client.println("<html>"); 
    client.println("<head><title>Cofre NFC</title><meta http-equiv='refresh' content='2'></head>");
     
    client.print((String)"<body><h1> Bem vindo </h1>");
    client.print("<p>Status da porta: </p>");
    if(isDoorOpen){
      client.print("<p style='color:green;'>Aberta</p>"); 
    }else{
      client.print("<p style='color:red;'>Fechada</p>");
    }
    if(type == 1){
      client.print("<p'>Modo cadastro ativado</p>");
    }else if(type == 2){
      client.print("<p style='color:gree;'>Novo cartao adicionado</p>");
    }else if(type == 4){
      client.print("<p style='color:red;'>Acesso negado</p>");
    } 
    client.print("</body>");
     
  }  
}

void sendPostRequest(){
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;    //Declare object of class HTTPClient

    http.begin("http://api-dso2.herokuapp.com/log");      //Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded", false, true);
     
    printf("%02X%02X%02X%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
    sprintf(data, "card_uid=%02X%02X%02X%02X&door_status=%s",readCard[0], readCard[1], readCard[2], readCard[3], isDoorOpen?"open":"closed");
    int httpCode = http.POST(data); //Send the request

    Serial.println(httpCode);   //Print HTTP return code
    http.writeToStream(&Serial);  // Print the response body
  }
  
}

void changeDoorStatus(){
  if(isDoorOpen){
    servo.write(0);
  }else{
    servo.write(180);
  } 
  isDoorOpen = !isDoorOpen;
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted () {
  changeDoorStatus();             // unlock door 
  digitalWrite(yellowLed, LED_OFF);   // Turn off blue LED
  digitalWrite(redLed, LED_OFF);  // Turn off red LED
  digitalWrite(greenLed, LED_ON);   // Turn on green LED
  httpMode = 0;
  delay(1000);            // Hold green LED on for a second
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_ON);   // Turn on red LED
  httpMode = 4;
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() { 

  // Getting ready for Reading PICCs
  delay(0);
  if (mfrc522.PICC_IsNewCardPresent()) {
    if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
      return 0;
    }
    // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
    // I think we should assume every PICC as they have 4 byte UID
    // Until we support 7 byte PICCs
    Serial.println(F("Scanned PICC's UID:"));
    for ( uint8_t i = 0; i < 4; i++) {  //
      readCard[i] = mfrc522.uid.uidByte[i];
      Serial.print(readCard[i], HEX);
    }
    Serial.println("");
    mfrc522.PICC_HaltA(); // Stop reading
    return 1;
  }
  return 0;
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(yellowLed, LED_ON);  // Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF);  // Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure Green LED is off
 
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();  
    httpMode = 2;
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    EEPROM.commit();  
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo ( byte a[], byte b[] ) {   
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] ) {     // IF a != b then false, because: one fails, all fail
       return false;
    }
  }
  return true;  
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
bool findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i < count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
    }
    else {    // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is on
  delay(200); 
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200); 
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200); 
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200); 
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(yellowLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);  // Make sure blue LED is on
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
bool isMaster( byte test[] ) {
  return checkTwo(test, masterCard);
}

bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (analogRead(wipeB) < 1000)
        return false;
    }
    yield(); 
  }
  return true;
}
