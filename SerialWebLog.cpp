#include "SerialWebLog.h"
#include <ezTime.h>

#define LOG_START 			"\n----------------------------------------------\n"
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

		Serial.begin(9600);
		this->print(LOG_START);
		this->printf("Starting \"%s\" MicroController...\n", hostname);

		WiFi.setPhyMode(WIFI_PHY_MODE_11G);
		WiFi.setSleepMode(WIFI_NONE_SLEEP);
		WiFi.mode(WIFI_STA);	//if it gets disconnected
		WiFi.disconnect();
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
	if (buffer != temp) {
		delete[] buffer;
	}
	return len;	
}

void SerialWebLog::print(const String &s) {
	textLog += TIMESTAMP_LOG + s;
	Serial.print(s);
}

void SerialWebLog::print(const char* text){
	textLog += TIMESTAMP_LOG + String(text);
	Serial.print(String(text));
}

void SerialWebLog::update(){
	webserver->handleClient();
}

void SerialWebLog::handleLog(){
	webserver->send(200, "text/plain", textLog);
}

void SerialWebLog::handleReset(){		
	ESP.restart();
}

void SerialWebLog::clearLog(){		
	webserver->send(200, "text/html", "<html><body>Clear Log OK!</body></html>");
	textLog = "";
	print(LOG_START);
}


void SerialWebLog::handleRoot(){

	float mem = ESP.getFreeHeap() / 1024.0f;
	static String a = "<html> \
<header><style>body{line-height: 150%;}</style></header>\
<body>\
<p align='center'><br><h1>";

	static String b = "</h1><br><iframe src='/log' width=80% height=70%></iframe> <br><br>\
<a href='/reset'>Reset ESP</a> &vert; \
<a href='/clearLog'>Clear Log</a>";

	static String closure = "</p>\
</body>\
</html>";

	char aux[64];
	uint64_t now = millis64();
	uint32_t min = now / (1000 * 60);
	uint32_t hour = now / (1000 * 60 * 60);
	uint32_t day = now / (1000 * 60 * 60 * 25);

	String mydate = TIMESTAMP_FORMAT;
	sprintf(aux, "%.1f KB free<br>%s<br>uptime: %d days, %d hours, %d minutes.", mem, mydate.c_str(), day, hour, min);

	webserver->send(200, "text/html", a + String(WiFi.getHostname()) + b + compiledExtraHTML + String(aux) + closure);
}


void SerialWebLog::addHtmlExtraMenuOption(const String & title, const String & URL){
	extraHTML[title] = URL;
	compiledExtraHTML = "";
	for (auto & it : extraHTML){
		compiledExtraHTML += " | <a href='" + it.second + "'>" + it.first + "</a> ";
	}
	compiledExtraHTML += "<br>";
}
