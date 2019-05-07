#include <PubSubClient.h> //pubsub library for mqtt integration
#include <ESP8266WiFi.h> //library for wifi integration
#include <ArduinoJson.h> //json library integration for working with json

//wifi and pubsub setup
#define wifi_ssid "CYB"
#define wifi_password ""
WiFiClient espClient;
PubSubClient mqtt(espClient);

//mqtt server and login credentials
#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server
char mac[6]; //unique id

const int warningLEDs = 12;
const int requestShutdown = 13;

int warningStatus = LOW;
int requestStatus = LOW;

void setup() {
  // start the serial connection
  Serial.begin(115200);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  // setting up the leds
  pinMode(warningLEDs, OUTPUT);
  pinMode(requestShutdown, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); //for signifying an incoming message

  setup_wifi(); //start wifi
  mqtt.setServer(mqtt_server, 1883); //start mqtt server
  mqtt.setCallback(callback); //register the callback function

  // wait for serial monitor to open
  while (! Serial);
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
  WiFi.macAddress().toCharArray(mac, 4);            //5C:CF:7F:F0:B0:C1 for example
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("colinyb/sensorplatform"); //subscribing to 'colinyb/sensorplatform'
    } else { // print the state of the mqtt connection
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

  // make the change to the LEDs based on MQTT data from sensor platform
  digitalWrite(warningLEDs, warningStatus);
  digitalWrite(requestShutdown, requestStatus);

  delay(2500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);

  DynamicJsonBuffer  jsonBuffer; //creating DJB instance named jsonBuffer
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { // Serial fail message
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
  Serial.println(root["tempF"].as<String>());
  Serial.println(root["externalTemp"].as<String>());

  // if external temp exceeds 65º or internal temp exceeds 80º, turn on only red LEDs,
  // if internal temp exceeds 75º, turn on only green LEDs,
  // if no temp limit is exceeded, turn all LEDs off.
  if (root["externalTemp"] >= 65.0 || root["tempF"] >= 80.0) {
    requestStatus = HIGH;
    warningStatus = LOW;
  } else if (root["tempF"] >= 75.0) {
    requestStatus = LOW;
    warningStatus = HIGH;
  } else {
    requestStatus = LOW;
    warningStatus = LOW;
  }
}
