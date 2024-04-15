/*

Uploads data to a website of your choosing.

*/

// drag these in from the arduino library manager.
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi information
const char* ssid     = "your_ssid_here";
const char* password = "your_wifi_password";

// remote host - replace this with where you want to send the data.
const char* host = "192.168.0.100";

// reporting interval in seconds
int interval = 30;

// Define which pin the DS18B20 is connected to and set up the OneWire
// and DallasTemperature objects.
#define ONE_WIRE_BUS D3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

float tempSum;
float tempAvg;
float tempLast = 0;

void setup() {
  // Initialize the LED_BUILTIN pin as an output
  pinMode(LED_BUILTIN, OUTPUT); 
  // adjust the interval so it's zero-indexed...
  interval--; 
  Serial.begin(115200);
  
  // prints "connecting to" and ssid to serial
  preConnectBanner();
  // connects to wifi, draws connection percentage bar - will just keep retrying until it works.
  wifiConnect();
  // shows successful connection attempt
  postConnectBanner();

  delay(1000);
}

void loop() {
  float temp;
  int count = 1;
  Serial.print("i c cur sum avg last\n");
  
  for(int i = interval; i >= 0; i--) {
    do {
      DS18B20.requestTemperatures(); 
      temp = DS18B20.getTempCByIndex(0);
    } while (temp == 85.0 || temp == (-127.0) || temp > 120); // ignore 0x00 and 0xff or "off chart high"

    /* 
       If the temperature is 2x higher than the current rolling average,
       drop it and replace it with the current rolling average.  Makes the
       system less capable of sudden < 60 second spikes, and tends to
       prefer early data in a cycle.
       
       The (i == interval) part of it is just for handling
       interval-long windows of weirdness where the temperature jumps
       by 10 degrees in a minute.  Useful for environmental monitoring
       where that's unlikely, maybe less so if you're trying to use it
       to monitor a boiling pot of water.  It's there because I would
       occasionally get spikes in my recorded data, this tends to
       smooth them out.

       Now, if your sensor goes offline for too long, you'll just
       flatline eventually, but you should notice this happening.
    */
    
    if(i == interval) {
      if((temp > (tempLast+10)) && (tempLast != 0)) {
        Serial.print("HIGH: Received temp "); Serial.print(temp); 
        Serial.print(" from sensor. Last cycle average is "); Serial.print(tempLast); 
        Serial.print(" Ignoring.\n");
        temp = tempLast;
      }
      else if((temp < (tempLast-10)) && (tempLast != 0)) {
        Serial.print("LOW: Received temp "); Serial.print(temp); 
        Serial.print(" from sensor. Last cycle average is "); Serial.print(tempLast); 
        Serial.print(" Ignoring.\n");
        temp = tempLast;
      }
    }
    else if(i != interval) {
      if((temp > (tempSum/(count-1))+10)) { 
        Serial.print("HIGH: Received temp "); Serial.print(temp); 
        Serial.print(" from sensor. Average is "); Serial.print(tempSum /(count-1)); 
        Serial.print(" Ignoring.\n");
        temp = (tempSum/(count-1));
      }
      else if((temp < (tempSum/(count-1))-10)) {
        Serial.print("LOW: Received temp "); Serial.print(temp);
        Serial.print(" from sensor. Average is "); Serial.print(tempSum /(count-1));
        Serial.print(" Ignoring.\n");
        temp = (tempSum/(count-1));
      }
    }

    // Displays the running countdown, increment of values,
    // temperature, sum, running average, and last reading.  All of
    // this is to smooth out any glitchiness from the sensor, which is
    // surprisingly common.
    
    tempSum += temp;
    Serial.print(i); Serial.print(" ");
    Serial.print(count); Serial.print(" ");
    Serial.print(temp); Serial.print(" ");
    Serial.print(tempSum); Serial.print(" ");
    Serial.print(tempSum/(count)); Serial.print(" ");
    Serial.print(tempLast); Serial.print("\n");

    // Blink the LED_BUILTIN every second so you know it's doing
    // something.
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(1000);
    count++;
  }

  // update the current tempLast at the end of the interval
  tempLast = tempAvg = (tempSum / (count-1));

  // send that data off to a remote site!
  sendTemperature(tempAvg);

  // Reset Average and Sum.
  tempAvg = tempSum = 0;

}

// Draw the first connection screen in the wificonnect...
void preConnectBanner() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
}

void postConnectBanner() {
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
}

void wifiConnect() {
  /*
    Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
    would try to act as both a client and an access-point and could cause
    network-issues with your other WiFi-devices on your WiFi-network.
  */
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int progress = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    progress++;
    if(progress > 100) { progress = 0; }
  }
  delay(1000);
}

void sendTemperature(float temp) {  
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  
  // if the connection fails, ensure the wifi client is still connected to the network
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    delay(1000);
    preConnectBanner();
    wifiConnect(); // reconnect?
    postConnectBanner(); // show confirmation that you're connected.
    
    delay(1000);
  }
  else {
    // We now create a URI for the request
    String url = "/cgi-bin/DS18B20.cgi?mac=";
    url += WiFi.macAddress();
    url += "&millis=";
    url += millis()/1000;
    url += "&value=";
    url += temp;
    
    Serial.print("Requesting URL: ");
    Serial.println(url);
    
    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n" +
                 "\r\n");
    unsigned long timeout = millis();
    
    // handle the server answering but not being functional...    
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    
    // The former at least captures 'HTTP/1.1 200 OK'
    if (client.available()) {
      // this is faster but ignores any return codes from the server,

      Serial.print("available\n");
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }  
    /*
      while(client.available()) {
      
      // This is slower because the server can time out and it can get a
      // little weird - if you were sending off vital data, maybe you
      // want to trap it and send an error handler and a timestamp for
      // the data when you reconnect? But most of the time that lost
      // data is invisible after a day anyway.
      
      Serial.print("available\n");
      String line = client.readStringUntil('\r');
      Serial.print(line);
      }  
    */
    Serial.print("out of while\n");
    client.stop();
  }
}
