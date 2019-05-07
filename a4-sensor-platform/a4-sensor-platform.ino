#include <Wire.h>  // for I2C communications
#include <Adafruit_Sensor.h>  // the generic Adafruit sensor library used with both sensors
#include <DHT.h>   // temperature and humidity sensor library
#include <DHT_U.h> // unified DHT library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPL115A2.h> // Barometric pressure sensor library
#include <PubSubClient.h> //pubsub library for mqtt integration
#include <ESP8266WiFi.h> //library for wifi integration
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //json library integration for working with json

//wifi and pubsub setup
#define wifi_ssid "CYB"
#define wifi_password "m1ck3yM0us3"
WiFiClient espClient;
PubSubClient mqtt(espClient);
char mac[6]; //unique id
char message[201]; //setting message length
unsigned long currentTimer, timer, currentApiTimer, apiTimer; //creating a timer and a counter

// api keys
const char* key = "";
const char* weatherkey = "";

//mqtt server and login credentials
#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

// pin connected to DH22 data line
#define DATA_PIN 12

// external temp variable
String externalTemp;

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

// create MPL115A2 instance
Adafruit_MPL115A2 mpl115a2;

// create OLED Display instance on an ESP8266
// set OLED_RESET to pin -1 (or another), because we are using default I2C pins D4/D5.
#define OLED_RESET -1

Adafruit_SSD1306 display(OLED_RESET); //creating an ssd1306 instance named display

void setup() {
  // start the serial connection
  Serial.begin(115200);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  // set up the OLED display
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting up...");
  display.display();
  // init done

  display.println("Starting WiFi & MQTT");
  display.display();
  setup_wifi(); //start wifi
  mqtt.setServer(mqtt_server, 1883); //start mqtt server
  mqtt.setCallback(callback); //register the callback function
  timer = apiTimer = millis(); //set timers

  // wait for serial monitor to open
  while (! Serial);

  Serial.println("Sensor Platform Started");

  // make initial API call
  externalTemp = getMet();

  // initialize dht22
  dht.begin();

  // initialize MPL115A2
  mpl115a2.begin();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
  WiFi.macAddress().toCharArray(mac, 6);            //5C:CF:7F:F0:B0:C1 for example
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("colinyb/+"); //subscribing to 'colinyb' and all subtopics below that topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) { //handles reconnecting and initial connect
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active'

  currentTimer = currentApiTimer = millis(); //updating the timers

  if (currentApiTimer - apiTimer > 1800000) { //a periodic call, every 30 minutes
    externalTemp = getMet();
    apiTimer = currentApiTimer;
  }

  if (currentTimer - timer > 10000) { //a periodic report, every 10 seconds
    //-------------GET THE TEMPERATURE--------------//
    // the Adafruit_Sensor library provides a way of getting 'events' from sensors
    //getEvent returns data from the sensor
    sensors_event_t event; //creating sensor_event_t instance named event
    dht.temperature().getEvent(&event); //reading temperature
  
    float celsius = event.temperature; //assigning temperature to celsius
    float fahrenheit = (celsius * 1.8) + 32; //calculating fahrenheit
  
    Serial.print("Celsius: ");
    Serial.print(celsius);
    Serial.println("C");
  
    Serial.print("Fahrenheit: ");
    Serial.print(fahrenheit);
    Serial.println("F");
  
  
    //-------------GET THE HUMIDITY--------------//
    dht.humidity().getEvent(&event); //reading humidity
    float humidity = event.relative_humidity; //assigning humidity to humidity
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  
    //-------------GET THE PRESSURE--------------//
    // The Adafruit_Sensor library doesn't support the MPL1152, so we'll just grab data from it
    // with methods provided by its library
  
    float pressureKPA = 0; //creating a variable for pressure
  
    pressureKPA = mpl115a2.getPressure(); //reading pressure
    Serial.print("Pressure (kPa): "); 
    Serial.print(pressureKPA, 4); 
    Serial.println(" kPa");

    //creating variables to store the data
    char str_tempf[6];
    char str_etemp[6];
    char str_humd[6];
    char str_press[6];

    //converting to char arrays
    externalTemp.toCharArray(str_etemp, 6);
    dtostrf(fahrenheit, 4, 2, str_tempf);
    dtostrf(humidity, 4, 2, str_humd);
    dtostrf(pressureKPA, 4, 2, str_press);
    // building message
    sprintf(message, "{\"tempF\": %s, \"externalTemp\": %s, \"humidity\": %s, \"pressure\": %s}", str_tempf, str_etemp, str_humd, str_press);
    mqtt.publish("colinyb/sensorplatform", message); //publishing to mqtt
    Serial.println("publishing");
    timer = currentTimer; //resetting timer

    //-------------UPDATE THE DISPLAY--------------//
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Temp.   : ");
    display.print(fahrenheit);
    display.println("'F");
    display.print("Temp.   : ");
    display.print(celsius);
    display.println("'C");
    display.print("Humidity: ");
    display.print(humidity);
    display.println("%");
    display.print("Pressure: ");
    display.print(pressureKPA);
    display.println(" kPa");
    display.display();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //creating DJB instance named jsonBuffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { // Serial fail message
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
}

String getMet() {
  HTTPClient theClient; //creates an HTTPClient object named theClient
  String apistring = "http://api.openweathermap.org/data/2.5/weather?q=" + getGeo() + "&units=imperial&appid=" + weatherkey; //concatonating the api request url
  theClient.begin(apistring); //make the request
  int httpCode = theClient.GET(); //get the HTTP code (-1 is fail)

  if (httpCode > 0) { //test if the request failed
    if (httpCode == 200) { //if successful...
      DynamicJsonBuffer jsonBuffer; //create a DynamicJsonBuffer object named jsonBuffer
      String payload = theClient.getString(); //get the string of json data from the request and assign it to payload
      Serial.println("Parsing...");
      Serial.println(payload);
      JsonObject& root = jsonBuffer.parse(payload); //set the json data to the variable root
      
      if (!root.success()) { //check if the parsing worked correctly
        Serial.println("parseObject() failed");
        Serial.println(payload); //print what the json data is in a string form
        return("error");
      } // return the temperature
      return(root["main"]["temp"].as<String>());
    } else { //print error if the request wasnt successful
      Serial.println("Had an error connecting to the network.");
    }
  }
}

String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json"); //Make the request
  int httpCode = theClient.GET(); //get the http code for the request

  if (httpCode > 0) {
    if (httpCode == 200) { //making sure the request was successful

      DynamicJsonBuffer jsonBuffer; // create a dynamicjsonbuffer object named jsonbuffer

      String payload = theClient.getString(); //get the data from the api call and assign it to the string object called payload
      JsonObject& root = jsonBuffer.parse(payload); //create a jsonObject called root and use the jsonbuffer to parse the payload string to json accessible data
      ipAddress = root["ip"].as<String>();

    } else { //error message for unsuccessful request
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  return ipAddress; //returning the ipAddress 
}

String getGeo() {
  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + key); //return IP as .json object
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return("error");
      }

      return(root["city"].as<String>()); //return the city name

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return("error");
    }
  }
}
