#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <map>

uint64_t millis64();

class SerialWebLog{
public:

	ESP8266WebServer * webserver = nullptr;
	String textLog;

	SerialWebLog(){};

	void setup(const char * hostname, const char * SSID, const char * wifi_pass);
	void begin();

	void update();
	ESP8266WebServer* getServer();
	
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

	std::map<String, String> extraHTML;
	String compiledExtraHTML = "<br>";

};