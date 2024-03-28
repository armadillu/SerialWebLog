#include "SerialWebLog.h"
#include <ESP8266mDNS.h>
#include <ezTime.h>

#define LOG_START 			"----------------------------------------------\n"
#define TIMESTAMP_FORMAT 	(timeStatus() == timeSet ? UTC.dateTime("d-M-Y H:i:s T"): "[unknonwn time]")
#define TIMESTAMP_LOG  		(TIMESTAMP_FORMAT + " >> ")

uint64_t millis64() {
	static uint32_t low32, high32;
	uint32_t new_low32 = millis();
	if (new_low32 < low32) high32++;
	low32 = new_low32;
	return (uint64_t) high32 << 32 | low32;
}


void SerialWebLog::setup(const char * hostname, const char * SSID, const char * wifi_pass){

	if(webserver == nullptr){

		textLog.reserve(maxLogSize);

		Serial.begin(9600);
		this->print(LOG_START);
		this->printf("Starting \"%s\" MicroController...\n", hostname);

		//register to some events
		stationConnectedHandler = WiFi.onStationModeConnected([this](const WiFiEventStationModeConnected& evt){
			this->printf("WIFI::onStationModeConnected() to channel %d - %02x:%02x:%02x:%02x:%02x:%02x\n", evt.channel, evt.bssid[0], evt.bssid[1],
				evt.bssid[2], evt.bssid[3],evt.bssid[4], evt.bssid[5]);
		});

		stationDisconnectedHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& evt){
			this->printf("WIFI::onStationDisconnected(): %d - %02x:%02x:%02x:%02x:%02x:%02x\n", evt.reason, evt.bssid[0], evt.bssid[1],
				evt.bssid[2], evt.bssid[3],evt.bssid[4], evt.bssid[5]);
		});


		WiFi.setPhyMode(WIFI_PHY_MODE_11G);
		WiFi.setSleepMode(WIFI_NONE_SLEEP);
		WiFi.mode(WIFI_STA);	//if it gets disconnected
		WiFi.disconnect(true);
		WiFi.setAutoReconnect(true);
		WiFi.setHostname(hostname);
		WiFi.begin(SSID, wifi_pass);

		this->printf("Trying to connect to %s ...\n", SSID);
		if (WiFi.waitForConnectResult() != WL_CONNECTED) {
			delay(5000);
			ESP.restart();
		}
		this->printf("Connected to %s IP address \"%s\"\n", SSID, WiFi.localIP().toString().c_str());

		this->printf("NTP Sync start...\n");

		//setServer("1.es.pool.ntp.org");
		//setInterval(60 * 60 * 12 /*seconds*/); //every 12 hours
		waitForSync();
		this->printf("NTP Sync done!\n");

		webserver = new ESP8266WebServer(80);

		webserver->on("/", [this](){handleRoot();});
		webserver->on("/reset", [this](){handleReset();});
		webserver->on("/log", [this](){handleLog();});
		webserver->on("/clearLog", [this](){clearLog();});
		
		webserver->begin();

		if (!MDNS.begin(hostname)){
			this->print("Error setting up mDNS responder!"); 
		}else{
			this->print("mDNS started!\n"); 
		}
		MDNS.addService("http", "tcp", 80);

	}else{
		this->print("SerialWebLog:: Trying to setup twice! Error!\n");
	}
}


ESP8266WebServer* SerialWebLog::getServer(){
	return webserver;
}


size_t SerialWebLog::printf(const char *format, ...) {
	va_list arg;
	va_start(arg, format);
	static char temp[64];
	char* buffer = temp;
	size_t len = vsnprintf(temp, sizeof(temp), format, arg);
	va_end(arg);
	if (len > sizeof(temp) - 1) {
		buffer = new (std::nothrow) char[len + 1];
		if (!buffer) {
			return 0;
		}
		va_start(arg, format);
		vsnprintf(buffer, len + 1, format, arg);
		va_end(arg);
	}
	Serial.print(buffer);
	textLog += TIMESTAMP_LOG + String(buffer);
	trimLog();
	if (buffer != temp) {
		delete[] buffer;
	}
	return len;	
}

void SerialWebLog::print(const String &s) {
	textLog += TIMESTAMP_LOG + s;
	Serial.print(s);
	trimLog();
}

void SerialWebLog::print(const char* text){
	textLog += TIMESTAMP_LOG + String(text);
	Serial.print(String(text));
	trimLog();
}

void SerialWebLog::update(){
	webserver->handleClient();
	MDNS.update();
}

void SerialWebLog::handleLog(){
	webserver->send(200, "text/plain", textLog);
}

void SerialWebLog::handleReset(){
	webserver->send(200, "text/plain", "Reset OK!");
	delay(300);
	ESP.restart();
}

void SerialWebLog::clearLog(){		
	webserver->send(200, "text/plain", "Clear Log OK!");
	textLog = "";
	Serial.print("Cleared Log!\n");
	print(LOG_START);
}

void SerialWebLog::trimLog(){
	if(textLog.length() > maxLogSize){
		uint32_t index = 0;
		bool done = false;
		while(!done){
			index++;
			if(index >= textLog.length() - 1){
				done = true;
				//Serial.print("Can't trim end of log!\n");
				textLog = "";
			}else{
				if(textLog[index] == '\n'){
					if (textLog.length() - index < maxLogSize ){
						//Serial.printf("trim log at index %d!", index);
						done = true;
						textLog = "(...)" + textLog.substring(index);					
					}
				}
			}
		}
	}
}


void SerialWebLog::handleRoot(){

	float mem = ESP.getFreeHeap() / 1024.0f;
	static String a1 = "<html><header><style>body{line-height: 150%; text-align:center;}</style><title>";
	static String a2 = "</title></header><body><br><h1>";
	static String b = "</h1><br><iframe src='/log' name='a' width='80%' height='70%'></iframe> <br><br><a href='/log' target='a'>Log</a> &vert; <a href='/reset' target='a'>Reset ESP</a> &vert; <a href='/clearLog' target='a'>Clear Log</a>";
	static String closure = "</p></body></html>";
	String hstname = String(WiFi.getHostname());

	static char aux[128];
	uint64_t now = millis64();
	uint32_t min = (now / (1000 * 60))%60;
	uint32_t hour = (now / (1000 * 60 * 60))%24;
	uint32_t day = now / (1000 * 60 * 60 * 25);

	String mydate = TIMESTAMP_FORMAT;
	sprintf(aux, "%.1f KB free | WiFi RSSI: %ddBm<br>Date: %s<br>Uptime: %d days, %d hours, %d minutes.", mem, WiFi.RSSI(), mydate.c_str(), day, hour, min);

	webserver->send(200, "text/html", a1 + hstname + a2 + hstname + b + compiledExtraHTML + String(aux) + closure);
}


void SerialWebLog::addHtmlExtraMenuOption(const String & title, const String & URL){
	extraHTML[title] = URL;
	compiledExtraHTML = "";
	for (auto & it : extraHTML){
		compiledExtraHTML += " | <a href='" + it.second + "' target='a'>" + it.first + "</a> ";
	}
	compiledExtraHTML += "<br>";
}
