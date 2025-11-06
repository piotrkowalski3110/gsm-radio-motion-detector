#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

#define LED_PIN 3 // PIN LED
#define CE_PIN 8  // PIN CE modułu nRF24L01
#define CSN_PIN 9 // PIN CSN modułu nRF24L01

RF24 radio(CE_PIN, CSN_PIN);
RF24Network network(radio);

const uint16_t this_node = 01;   // Adres tego węzła (odbiornika)
const uint16_t sender_node = 00; // Adres nadajnika

void setup()
{
    Serial.begin(9600);
    SPI.begin();
    radio.begin();
    network.begin(90, this_node);
    radio.setDataRate(RF24_2MBPS);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void loop()
{
    network.update();
    while (network.available())
    {
        RF24NetworkHeader header;
        int movement;
        network.read(header, &movement, sizeof(movement));

        digitalWrite(LED_PIN, movement);

        Serial.print("Otrzymany stan: ");
        Serial.println(movement);
    }
}