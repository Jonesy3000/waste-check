#include <Arduino.h>
#include "HX711.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX17043.h"
#include "driver/rtc_io.h"


// Defining load cell amplifier pins
#define DOUT  13 // Load cell 1
#define CLK  12 
#define DOUT2  19 // Load cell 2
#define CLK2  5
#define DOUT3  32 // Load cell 3
#define CLK3  33
#define DOUT4  27 // Load cell 4
#define CLK4  14

// Defining LED pins
gpio_num_t green_LED = GPIO_NUM_15;
gpio_num_t red_LED = GPIO_NUM_2;

RTC_DATA_ATTR float SLEEP_TIME = 1; //sleep time in hours

// Defining each of the four HX711
HX711 scale1;
HX711 scale2;
HX711 scale3;
HX711 scale4;

// Setting calibration factors as determined in testing
float calibration_factor1 = -17900; // Load cell 1
float calibration_factor2 = -18100; // Load cell 2
float calibration_factor3 = -17900; // Load cell 3
float calibration_factor4 = -17100; // Load cell 4

// Setting global variables for each weight
double one;
double two;
double three;
double four;
double total;

// Access point SSID and password
const char* ssid = "SmartBin2.0";
const char* password = "Capstone2021!";

// MQTT server address, as defined by Pi IP address
const char* mqtt_server = "192.168.20.30";

// Defining the client for pub/sub nature of MQTT
WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];

// Running deep sleep mode by calling function
void deepSleep(){
  Serial.println("Nigh Nigh....");

  esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000 * 60 * 60); //Originally in microseconds, adjusted with time set in preferences above

  esp_deep_sleep_start(); //starts deep sleep
}

// Setting initial values for the RTC memory that replicate previous bin weights before sleep
RTC_DATA_ATTR float scale1_prev = 0; 
RTC_DATA_ATTR float scale2_prev = 0;
RTC_DATA_ATTR float scale3_prev = 0;
RTC_DATA_ATTR float scale4_prev = 0;

// Verification for taring through iterations
RTC_DATA_ATTR unsigned int doITare = 0;

// Verification of change to times
RTC_DATA_ATTR unsigned int time20 = 0;
RTC_DATA_ATTR unsigned int time1 = 0;

void begin_scales() {
  scale1.begin(DOUT, CLK); //Sets each of the scales as per channels above - scale 1, 2, 3, 4
  scale2.begin(DOUT2, CLK2); 
  scale3.begin(DOUT3, CLK3);
  scale4.begin(DOUT4, CLK4);
}

void tare_scales() {
  scale1.tare(); //Reset the scale to 0
  scale2.tare(); //Reset the scale to 0
  scale3.tare(); //Reset the scale to 0
  scale4.tare(); //Reset the scale to 0
}

void test_tare() {
  //Once set to zero, this pulls value from previous iteration and ensures bin reads fine
  if (doITare > 0) {
    scale1.set_offset(scale1_prev);
    scale2.set_offset(scale2_prev);
    scale3.set_offset(scale3_prev);
    scale4.set_offset(scale4_prev);
  }

  //Increase value such that bin will import values on next iteration
  if (doITare == 0) {
  doITare++;
  }
}

void set_cal() {
  scale1.set_scale(calibration_factor1); //Adjust to this calibration factor
  scale2.set_scale(calibration_factor2); //Adjust to this calibration factor
  scale3.set_scale(calibration_factor3); //Adjust to this calibration factor
  scale4.set_scale(calibration_factor4); //Adjust to this calibration factor
}

void read_weights() {
  one = scale1.get_units();
  two = scale2.get_units();
  three = scale3.get_units();
  four = scale4.get_units();

  total = one + two + three + four; //summing four values for total
}

void pub_weights() {
  char oneString[8];
  dtostrf(one, 1, 2, oneString);
  Serial.print("Scale one weight: ");
  Serial.println(oneString);
  client.publish("esp32/one", oneString);

  char twoString[8];
  dtostrf(two, 1, 2, twoString);
  Serial.print("Scale two weight: ");
  Serial.println(twoString);
  client.publish("esp32/two", twoString);

  char threeString[8];
  dtostrf(three, 1, 2, threeString);
  Serial.print("Scale three weight: ");
  Serial.println(threeString);
  client.publish("esp32/three", threeString);

  char fourString[8];
  dtostrf(four, 1, 2, fourString);
  Serial.print("Scale four weight: ");
  Serial.println(fourString);
  client.publish("esp32/four", fourString);

  char totalString[8];
  dtostrf(total, 1, 2, totalString);
  Serial.print("Total weight: ");
  Serial.println(totalString);
  client.publish("esp32/total", totalString);
}

void battery_read() {
  // Setting parameters
  Wire.begin(4, 0);
  FuelGauge.begin();

  // Reading voltage and percentage
  float percentage = FuelGauge.percent();
  float voltage = FuelGauge.voltage() / 1000;

  // Printing voltage for debugging
  Serial.print("Battery voltage: ");
  Serial.println(voltage);

  // Publishing percentage to MQTT server
  char batteryString[8];
  dtostrf(percentage, 1, 2, batteryString);
  Serial.print("Battery percentage ");
  Serial.println(batteryString);
  client.publish("esp32/battery", batteryString);

  // LED setting
  rtc_gpio_init(green_LED);
  rtc_gpio_init(red_LED); 
  rtc_gpio_set_direction(green_LED,RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_direction(red_LED,RTC_GPIO_MODE_OUTPUT_ONLY);

  if (percentage < 10) {
    // Green LED off
    rtc_gpio_hold_dis(green_LED);
    rtc_gpio_set_level(green_LED,0); //GPIO LOW
    rtc_gpio_hold_en(green_LED);

    // Red LED on
    rtc_gpio_hold_dis(red_LED);
    rtc_gpio_set_level(red_LED,1); //GPIO HIGH
    rtc_gpio_hold_en(red_LED);
  }

  else if (percentage > 100) {
    // Green LED on
    rtc_gpio_hold_dis(green_LED);
    rtc_gpio_set_level(green_LED,1); //GPIO HIGH
    rtc_gpio_hold_en(green_LED);

    // Red LED off
    rtc_gpio_hold_dis(red_LED);
    rtc_gpio_set_level(red_LED,0); //GPIO HIGH
    rtc_gpio_hold_en(red_LED);
  }

  else {
    // Both off
    rtc_gpio_hold_dis(green_LED);
    rtc_gpio_set_level(green_LED,0); //GPIO LOW
    rtc_gpio_hold_en(green_LED);

    rtc_gpio_hold_dis(red_LED);
    rtc_gpio_set_level(red_LED,0); //GPIO HIGH
    rtc_gpio_hold_en(red_LED);
  }
}

void set_prev() {
  scale1_prev = scale1.get_offset(); //Setting the weight value at the end of the iteration for next round
  scale2_prev = scale2.get_offset();
  scale3_prev = scale3.get_offset();
  scale4_prev = scale4.get_offset();
}

void check_messages(){
  for (int i=0; i < 10; i++) {
    client.loop();
    delay(100);
    }
}

//Program to connect to wifi
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//for receiving messages
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  // Changes the output state according to the message
  if (String(topic) == "esp32/tare") {
    Serial.print("Now taring...");
    // Taring the scale if the command is received
    if(messageTemp == "true"){
      scale1.tare(); //Reset the scale to 0
      scale2.tare(); //Reset the scale to 0
      scale3.tare(); //Reset the scale to 0
      scale4.tare(); //Reset the scale to 0
      scale1_prev = scale1.get_offset(); // Saving the recently tared offsets
      scale2_prev = scale2.get_offset();
      scale3_prev = scale3.get_offset();
      scale4_prev = scale4.get_offset();
      Serial.println("Successfully tared");
      client.publish("esp32/hastared", "Successfully tared!");
    }
  }
}

void reconnect() {
  // Loop until we're reconnected to the mqtt server - more or less basic code
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      //subscribing to the tare channel
      client.subscribe("esp32/tare", 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200); // Starting serial
  setup_wifi(); // Connecting to wifi
  client.setServer(mqtt_server, 1883); //Sets mqtt server, standard port
  client.setCallback(callback); //sets the callback function for receiving messages

  // Starting scales and setting channels
  begin_scales();
  
  // Starting and setting scale data
  tare_scales();

  // Confirm tare
  test_tare();

  // Set calibration factors
  set_cal();

  //connecting to MQTT client
  if (!client.connected()) {
    reconnect();
    }
  
  //retrieving values from each of the four scales
  read_weights();
  pub_weights();

  // read battery and set state of lights
  battery_read();
  
  // Setting previous values to constant characters
  set_prev();

  delay(1000);

  //Checking for any new messages on subscribed channels
  check_messages();

  //Returning to deep sleep
  deepSleep();
}

void loop() { // Nothing in the loop as we are using deep sleep mode
  }