#include <SoftwareSerial.h>
#include <DHT11.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

#define DHT_PIN 0                   // Pin DHT11
#define BUZZER_PIN 1                // Pin BUZZER
#define SIM800L_TX_PIN 2            // Pin TX modułu SIM800L
#define SIM800L_RX_PIN 3            // Pin RX modułu SIM800L
#define RCWL_0516_PIN 5             // Pin RCWL_0516
#define DS1302_CLK 6                // Pin CLK modułu DS1302
#define DS1302_DAT 7                // Pin DAT modułu DS1302
#define DS1302_RST 4                // Pin RST modułu DS1302
#define nRF_CE 8                    // Pin CE modułu nRF
#define nRF_CSN 9                   // Pin CSN modułu nRF
#define PHONE_NUMBER "+48XXXXXXXXX" // numer telefonu odbiorcy

SoftwareSerial sim800l(SIM800L_TX_PIN, SIM800L_RX_PIN);
ThreeWire myWire(DS1302_DAT, DS1302_CLK, DS1302_RST);
RtcDS1302<ThreeWire> Rtc(myWire);
DHT11 dht11(DHT_PIN);
RF24 radio(nRF_CE, nRF_CSN);
RF24Network network(radio);

const uint16_t this_node = 00; // Adres nadajnika
const uint16_t node01 = 01;    // Adres odbiornika

char Received_SMS;                               // otrzymana wiadomość SMS
String RSMS;                                     // odczytana wiadomość SMS
String Data_SMS;                                 // wiadomość SMS do wysłania
short TEMP_OK = -1, HUMID_OK = -1, BUZZ_OK = -1; // flaga komend SMS dla temperatury, wilgotności i buzzera
int temperature = 0, humidity = 0;               // zmienne temperatury i wilgotności
int movement = 0;                                // zmienne odczytane z RCWL_0516
RtcDateTime movementTime;                        // zmienne czasu odczytane z DS1302
int lastMinuteSend = -1;                         // zmienna pomocnicza do wysyłania wiadomości SMS co minute

void setup()
{
    Serial.begin(9600);
    pinMode(BUZZER_PIN, OUTPUT);   // Pin BUZZER jako wyjście
    pinMode(RCWL_0516_PIN, INPUT); // Pin RCWL_0516 jako wejście

    // inicjalizacja modułu nRF24L01
    Serial.println("Initializing nRF24L01...");
    SPI.begin();
    radio.begin();
    network.begin(90, this_node);
    radio.setDataRate(RF24_2MBPS);
    Serial.println("Done.");

    Serial.println();

    // inicjalizacja modułu SIM800L
    Serial.println("Initializing SIM800L...");
    sim800l.begin(9600);
    delay(3000);
    ReceiveMode();
    Serial.println("Done.");

    Serial.println();

    // inicjalizacja modułu DS1302
    Serial.println("Initializing DS1302...");
    Rtc.Begin();
    Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    Rtc.SetIsWriteProtected(false);
    Rtc.SetIsRunning(true);
    Serial.println("Done.");
}

void loop()
{
    network.update();
    int result = dht11.readTemperatureHumidity(temperature, humidity);
    RSMS = "";

    while (sim800l.available() > 0)
    {
        Received_SMS = sim800l.read();       // odczyt wiadomości SMS
        Serial.print(Received_SMS);          // wysłanie do monitora szeregowego otrzymanej wiadomości SMS
        RSMS.concat(Received_SMS);           // przepisanie otrzymanej wiadomości SMS do zmiennej RSMS
        TEMP_OK = RSMS.indexOf("getTemp");   // sprawdzenie czy wiadomość zawiera komende getTemp
        HUMID_OK = RSMS.indexOf("getHumid"); // sprawdzenie czy wiadomość zawiera komende getHumid
        BUZZ_OK = RSMS.indexOf("getBuzz");   // sprawdzenie czy wiadomość zawiera komende getBuzz
    }
    sendTemperature(); // wysłanie temperatury jeśli komenda została otrzymana
    sendHumidity();    // wysłanie wilgotności jeśli komenda została otrzymana
    runBuzzer();       // uruchomienie buzzera jeśli komenda została otrzymana
    checkMovement();   // wysłanie informacji o ruchu jeśli wykryto ruch
}

// przestawienie modułu SIM800L w tryb odbioru
void ReceiveMode()
{
    sim800l.println("AT");
    updateSerial();
    sim800l.println("AT+CMGF=1");
    updateSerial();
    sim800l.println("AT+CNMI=2,2,0,0,0");
    updateSerial();
}

// przestawienie modułu SIM800L w tryb wysyłania i wysłanie wiadomości SMS
void Send_Data()
{
    Serial.println("Sending Data...");
    sim800l.println("AT+CMGF=1");
    delay(500);
    sim800l.println("AT+CMGS=\"" PHONE_NUMBER "\"");
    delay(500);
    sim800l.println(Data_SMS);
    delay(500);
    sim800l.println((char)26);
    delay(500);
    sim800l.println();
    Serial.println("Data Sent.");
}

// obsługa komunikacji szeregowej między monitorem a modułem SIM800L
void updateSerial()
{
    delay(500);
    while (Serial.available())
    {
        sim800l.write(Serial.read());
    }
    while (sim800l.available())
    {
        Serial.write(sim800l.read());
    }
}

// wysłanie informacji o temperaturze w odpowiedzi na komende
void sendTemperature()
{
    if (TEMP_OK != -1)
    {
        Data_SMS = "Temperatura: " + String(temperature) + "C";
        Send_Data();
        ReceiveMode();
        TEMP_OK = -1;
        Data_SMS = "";
    }
}

// wysłanie informacji o wilgotności w odpowiedzi na komende
void sendHumidity()
{
    if (HUMID_OK != -1)
    {
        Data_SMS = "Wilgotnosc: " + String(humidity) + "%";
        Send_Data();
        ReceiveMode();
        HUMID_OK = -1;
        Data_SMS = "";
    }
}

// uruchomienie buzzera w odpowiedzi na komende
void runBuzzer()
{
    if (BUZZ_OK != -1)
    {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(1000);
        digitalWrite(BUZZER_PIN, LOW);
        BUZZ_OK = -1;
    }
}

// detekcja ruchu i wysłanie informacji o ruchu
void checkMovement()
{
    movement = digitalRead(RCWL_0516_PIN);
    RF24NetworkHeader header1(node01);
    bool ok = network.write(header1, &movement, sizeof(movement));
    if (movement == HIGH)
    {
        movementTime = Rtc.GetDateTime();
        if (movementTime.Minute() != lastMinuteSend)
        {
            Data_SMS = "Wykryto ruch!";
            Send_Data();
            ReceiveMode();
            Data_SMS = "";
            lastMinuteSend = movementTime.Minute();
        }
    }
}