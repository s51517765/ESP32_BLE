#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <Wire.h>

// Constants
#define SERVICE_UUID        "55725ac1-066c-48b5-8700-2d9fb3603c5e"
#define CHARACTERISTIC_UUID "69ddb59c-d601-4ea4-ba83-44f679a670ba"
#define BLE_DEVICE_NAME     "ESP32"
// GPIO 6〜11,32,33は非推奨
#define LED_PIN             2
#define BUTTON_PIN          4

// Variables
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool bleOn = false;
bool ledState = false;
bool buttonPressed = false;
int buttonCount = 0;
uint32_t time_ms = 0;

// Android（Kotlin）側からデータを受信するコールバック
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            Serial.print("Receive data from Android: length = ");
            Serial.print(rxValue.length());
            Serial.println("):");
            String receivedString = "";
            for (int i = 0; i < rxValue.length(); i++) {
                receivedString += rxValue[i];
            }
            Serial.print("Received Data: ");
            Serial.println(receivedString);
            
            // Androidから受け取った指示に応じて処理を分けるならここで
        }
    }
};

// サーバー接続状態のコールバック
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Connect Android");
    };
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Disconnect Android");
    }
};

void i2c_scan(){
    byte error, address;
    int nDevices;

    Serial.println("Scanning I2C...");

    nDevices = 0;
    for (address = 1; address < 127; address++ )
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.print(address, HEX);
            Serial.println("  !");

            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("Unknown error at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    BLEDevice::init(BLE_DEVICE_NAME);
    
    // Server設定
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Service設定
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Characteristic設定 (WRITE と NOTIFY を両方許可)
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE  |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // コールバックを設定
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    pCharacteristic->addDescriptor(new BLE2902());
    
    pService->start();
    
    // Advertising設定
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    
    pAdvertising->setScanResponse(true); 
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();

    Wire.begin();
    Serial.println("Start ESP32 BLE Server...");
}

void loop() {
    // disconnecting
    if(!deviceConnected && oldDeviceConnected){
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising();
        Serial.println("restartAdvertising");
        oldDeviceConnected = deviceConnected;
        bleOn = false;
        digitalWrite(LED_PIN,LOW);
    }
    
    // connecting
    if(deviceConnected && !oldDeviceConnected)
    {
        oldDeviceConnected = deviceConnected;
        bleOn = true;
    }

    // BUTTON (Notify送信処理)
    if(digitalRead(BUTTON_PIN) == LOW){
        if(buttonPressed == false){
            buttonCount++;
            String str="";
#if 0
            // データ1個
            str = String(buttonCount);
#else
            // データ3個
            str = String(buttonCount) + "," + String(buttonCount*2) + "," + String((buttonCount*buttonCount)%1000);
#endif
            Serial.print("Notify Send: ");
            Serial.println(str);
            
            // Notify送信
            if( deviceConnected ){
                pCharacteristic->setValue((uint8_t*)str.c_str(), str.length());
                pCharacteristic->notify();
            }
            buttonPressed = true;
            delay(50);
        }
    }
    else{
        if(buttonPressed == true){
            buttonPressed = false;
            delay(50);
        }
    }
    
    // I2C スキャン ＆ Lチカ
    if (millis() - time_ms > 1000)
    {
        time_ms = millis();
        i2c_scan();    
        
        // LED blink
        if(bleOn)
        {
            if(ledState){
                digitalWrite(LED_PIN,HIGH);
            }
            else
            {
                digitalWrite(LED_PIN,LOW);
            }
            ledState = !ledState;
        }
    }
}