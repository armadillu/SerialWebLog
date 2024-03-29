#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#define WEB_SERVER_TYPE ESP8266WebServer
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <WebServer.h>
#define WEB_SERVER_TYPE WebServer
#endif

#include <WiFiUdp.h>
#include <map>

uint64_t millis64();

class SerialWebLog{
public:

	SerialWebLog(){};

	void setup(const char * hostname, const char * SSID, const char * wifi_pass, const uint8_t* bssid = nullptr);
	void begin();

	void update();	
	WEB_SERVER_TYPE* getServer();
	
	// logging 
	size_t printf(const char *format, ...);
	void print(const String &s);
	void print(const char* text);

	// HTTP API
	void handleLog(); 	// 	/log
	void handleReset(); // 	/reset
	void clearLog(); 	// 	/clearLog
	void handleRoot();	// 	/

	//add extra options to html menu
	void addHtmlExtraMenuOption(const String & title, const String & URL);


protected:

	void trimLog();

	WEB_SERVER_TYPE * webserver = nullptr;
	
	String textLog;

	std::map<String, String> extraHTML;
	String compiledExtraHTML = "<br>";
	const uint32_t maxLogSize = 1024 * 5; //5kb of logs at max

	//
	#ifdef ARDUINO_ARCH_ESP8266
	WiFiEventHandler stationConnectedHandler;
	WiFiEventHandler stationDisconnectedHandler;
	#endif

	#ifdef ARDUINO_ARCH_ESP32
	WiFiEventId_t wifi32connectEvent;
	WiFiEventId_t wifi32disconnectEvent;
	#endif

};
