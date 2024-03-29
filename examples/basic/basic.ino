#include "WifiPass.h"	//define wifi SSID & pass
#include <SerialWebLog.h>

/////////////////////////////////////
#ifdef ARDUINO_ARCH_ESP8266
#define HOST_NAME "TEST_8266"
#endif
#ifdef ARDUINO_ARCH_ESP32
#define HOST_NAME "TEST_32"
#endif
/////////////////////////////////////

SerialWebLog mylog;

void handleHello();

void setup() {

	mylog.setup(HOST_NAME, ssid, password);

	// if you want to constrain connection to a single known AP, use this instead (for a more stable connection)	
	//byte bssid[6] = {0x60,0x8d,0x26,0xeb,0xe2,0x8b}; //60:8d:26:eb:e2:8b Orangebox6
	//mylog.setup(HOST_NAME, ssid, password, bssid);

	//add an extra endpoint
	mylog.getServer()->on("/hello", handleHello);
	mylog.addHtmlExtraMenuOption("Hello", "/hello");
}

void loop() {
	
	mylog.update();

	static int count = 0;
	//mylog.printf("log line... %d\n", count);
	count++;
	
	delay(100);
}


void handleHello(){
	mylog.getServer()->send(200, "text/plain", "Hello!");
}
