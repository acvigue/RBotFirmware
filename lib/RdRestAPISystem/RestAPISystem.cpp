// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#include "RestAPISystem.h"
#include "RestAPIEndpoints.h"

static const char* MODULE_PREFIX = "RestAPISystem: ";

String RestAPISystem::_systemVersion;

RestAPISystem::RestAPISystem(WiFiManager& wifiManager, WireGuardManager& wireGuardManager, MQTTManager& mqttManager,
            RdOTAUpdate& otaUpdate, NetLog& netLog,
            FileManager& fileManager, NTPClient& ntpClient,
            CommandScheduler& commandScheduler, ConfigBase& hwConfig,
            const char* systemType, const char* systemVersion) :
            _wifiManager(wifiManager), _wireGuardManager(wireGuardManager), _mqttManager(mqttManager), 
            _otaUpdate(otaUpdate), _netLog(netLog),
            _fileManager(fileManager), _ntpClient(ntpClient),
            _commandScheduler(commandScheduler),
            _hwConfig(hwConfig)
{
    _deviceRestartPending = false;
    _deviceRestartMs = 0;
    _updateCheckPending = false;
    _updateCheckMs = 0;
    _systemType = systemType;
    _systemVersion = systemVersion;
}

void RestAPISystem::setup(RestAPIEndpoints &endpoints)
{
    endpoints.addEndpoint("wpsk", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiSetPSK, this, std::placeholders::_1, std::placeholders::_2),
                    "Setup WiFi SSID/password/hostname (PSK)");
    endpoints.addEndpoint("wopen", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiSetOPEN, this, std::placeholders::_1, std::placeholders::_2),
                    "Setup WiFi SSID/hostname (OPEN)");
    endpoints.addEndpoint("wpeap", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiSetPEAP, this, std::placeholders::_1, std::placeholders::_2),
                    "Setup WiFi SSID/identity/username/password/hostname (PEAP)");
    endpoints.addEndpoint("wc", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiClear, this, std::placeholders::_1, std::placeholders::_2),
                    "Clear WiFi settings");
    endpoints.addEndpoint("wax", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiExtAntenna, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set external WiFi Antenna");
    endpoints.addEndpoint("wai", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiWifiIntAntenna, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set internal WiFi Antenna");
    endpoints.addEndpoint("mq", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiMQTTSet, this, std::placeholders::_1, std::placeholders::_2),
                    "Setup MQTT server/port/intopic/outtopic .. not ~ replaces / in topics");
    endpoints.addEndpoint("reset", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiReset, this, std::placeholders::_1, std::placeholders::_2), 
                    "Restart program");
    endpoints.addEndpoint("checkupdate", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiCheckUpdate, this, std::placeholders::_1, std::placeholders::_2), 
                    "Check for updates");
    endpoints.addEndpoint("v", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiGetVersion, this, std::placeholders::_1, std::placeholders::_2), 
                    "Get version info");
    endpoints.addEndpoint("loglevel", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogLevel, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log level");
    endpoints.addEndpoint("logmqtt", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogMQTT, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log to MQTT /enable/topic");
    endpoints.addEndpoint("loghttp", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogHTTP, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log to HTTP /enable/host/port/url");
    endpoints.addEndpoint("logpt", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogPT, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log to Papertail /enable/host/port");
    endpoints.addEndpoint("logserial", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogSerial, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log to serial /enable/port");
    endpoints.addEndpoint("logcmd", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogCmdSerial, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set log to cmdSerial /enable/port");
    endpoints.addEndpoint("logconfig", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNetLogGetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                    "Get log configuration");
    endpoints.addEndpoint("ntp", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNTPSetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                    "Set NTP to tz/server (~ replaces / in timezone)");
    endpoints.addEndpoint("ntpget", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiNTPGetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                    "get ntp config");
    endpoints.addEndpoint("reformatfs", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiReformatFS, this, std::placeholders::_1, std::placeholders::_2), 
                    "Reformat file system e.g. /spiffs");
    endpoints.addEndpoint("filelist", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiFileList, this, std::placeholders::_1, std::placeholders::_2), 
                    "List files in folder e.g. /spiffs/folder ... ~ for / in folder");
    endpoints.addEndpoint("fileread", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiFileRead, this, std::placeholders::_1, std::placeholders::_2), 
                    "Read file ... name", "text/plain");
    endpoints.addEndpoint("deleteFile", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                    std::bind(&RestAPISystem::apiDeleteFile, this, std::placeholders::_1, std::placeholders::_2), 
                    "Delete file e.g. /spiffs/filename ... ~ for / in filename");
    endpoints.addEndpoint("uploadtofileman", 
                    RestAPIEndpointDef::ENDPOINT_CALLBACK, 
                    RestAPIEndpointDef::ENDPOINT_POST,
                    std::bind(&RestAPISystem::apiUploadToFileManComplete, this, 
                            std::placeholders::_1, std::placeholders::_2),
                    "Upload file", "application/json", 
                    NULL, 
                    true, 
                    NULL,
                    NULL,
                    std::bind(&RestAPISystem::apiUploadToFileManPart, this, 
                            std::placeholders::_1, std::placeholders::_2, 
                            std::placeholders::_3, std::placeholders::_4,
                            std::placeholders::_5, std::placeholders::_6,
                            std::placeholders::_7));

    // Gat and set cmd scheduler JSON
    endpoints.addEndpoint("cmdScheduleGet", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&RestAPISystem::apiCmdSchedGetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                "Set command scheduler");
    endpoints.addEndpoint("cmdScheduleSet", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&RestAPISystem::apiPostCmdSchedule, this, std::placeholders::_1, std::placeholders::_2),
                            "Set command schedule", "application/json", NULL, true, NULL, 
                            std::bind(&RestAPISystem::apiPostCmdScheduleBody, this, 
                            std::placeholders::_1, std::placeholders::_2, 
                            std::placeholders::_3, std::placeholders::_4,
                            std::placeholders::_5));

    //Wireguard
    endpoints.addEndpoint("wireGuardGet", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&RestAPISystem::apiWireGuardGetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                "Get WireGuard settings");
    endpoints.addEndpoint("wireGuardSet", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                            std::bind(&RestAPISystem::apiPostWireGuard, this, std::placeholders::_1, std::placeholders::_2),
                            "Set WireGuard settings", "application/json", NULL, true, NULL, 
                            std::bind(&RestAPISystem::apiPostWireGuardBody, this, 
                            std::placeholders::_1, std::placeholders::_2, 
                            std::placeholders::_3, std::placeholders::_4,
                            std::placeholders::_5));
    
    //***EXPUNGED***
    endpoints.addEndpoint("***EXPUNGED***Get", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                std::bind(&RestAPISystem::api***EXPUNGED***GetConfig, this, std::placeholders::_1, std::placeholders::_2), 
                "Get ***EXPUNGED*** settings");
    }

String RestAPISystem::getWifiStatusStr()
{
    if (WiFi.status() == WL_CONNECTED)
        return "C";
    if (WiFi.status() == WL_NO_SHIELD)
        return "4";
    if (WiFi.status() == WL_IDLE_STATUS)
        return "I";
    if (WiFi.status() == WL_NO_SSID_AVAIL)
        return "N";
    if (WiFi.status() == WL_SCAN_COMPLETED)
        return "S";
    if (WiFi.status() == WL_CONNECT_FAILED)
        return "F";
    if (WiFi.status() == WL_CONNECTION_LOST)
        return "L";
    return "D";
}

int RestAPISystem::reportHealth(int bitPosStart, unsigned long *pOutHash, String *pOutStr)
{
    // Generate hash if required
    if (pOutHash)
    {
        unsigned long hashVal = (WiFi.status() == WL_CONNECTED);
        hashVal = hashVal << bitPosStart;
        *pOutHash += hashVal;
        *pOutHash ^= WiFi.localIP();
    }
    // Generate JSON string if needed
    if (pOutStr)
    {
        byte mac[6];
        WiFi.macAddress(mac);
        String macStr = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" +
                        String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
        String sOut = "\"wifiIP\":\"" + WiFi.localIP().toString() + "\",\"wifiConn\":\"" + getWifiStatusStr() + "\"";
        sOut += ",\"ssid\":\"" + WiFi.SSID() + "\",\"MAC\":\"" + macStr + "\",\"RSSI\":" + String(WiFi.RSSI());
        sOut += ",\"espV\":\"" + _systemVersion + " (built " + __DATE__ + " " + __TIME__ + ")\"";
        *pOutStr = sOut;
    }
    // Return number of bits in hash
    return 8;
}

void RestAPISystem::service()
{
    // Check restart pending
    if (_deviceRestartPending)
    {
        if (Utils::isTimeout(millis(), _deviceRestartMs, DEVICE_RESTART_DELAY_MS))
        {
            _deviceRestartPending = false;
            ESP.restart();
        }
    }
    // Check for update pending
    if (_updateCheckPending)
    {
        if (Utils::isTimeout(millis(), _updateCheckMs, DEVICE_UPDATE_DELAY_MS))
        {
            _updateCheckPending = false;
            Log.notice("%sservice start update check\n", MODULE_PREFIX);
            _otaUpdate.requestUpdateCheck();
        }
    }
}

void RestAPISystem::apiWifiSetPSK(String &reqStr, String &respStr)
{
    bool rslt = false;
    // Get SSID
    String ssid = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sWiFi SSID %s\n", MODULE_PREFIX, ssid.c_str());
    // Get pw
    String pw = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sWiFi PW %s\n", MODULE_PREFIX, pw.c_str());
    // Get hostname
    String hostname = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
    Log.trace("%sHostname %s\n", MODULE_PREFIX, hostname.c_str());
    // Check if both SSID and pw have now been set
    if (ssid.length() != 0 && pw.length() != 0)
    {
        Log.notice("%sWiFi Credentials Set SSID %s hostname %s\n", MODULE_PREFIX, ssid.c_str(), hostname.c_str());
        _wifiManager.setCredentialsPSK(ssid, pw, hostname, true);
        rslt = true;
    }
    Utils::setJsonBoolResult(respStr, rslt);
}

void RestAPISystem::apiWifiSetOPEN(String &reqStr, String &respStr)
{
    bool rslt = false;
    // Get SSID
    String ssid = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sWiFi SSID %s\n", MODULE_PREFIX, ssid.c_str());
    // Get pw
    String hostname = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sHostname %s\n", MODULE_PREFIX, hostname.c_str());

    // Check if both SSID and pw have now been set
    if (ssid.length() != 0)
    {
        Log.notice("%sWiFi Credentials Set SSID %s hostname %s\n", MODULE_PREFIX, ssid.c_str(), hostname.c_str());
        _wifiManager.setCredentialsOPEN(ssid, hostname, true);
        rslt = true;
    }
    Utils::setJsonBoolResult(respStr, rslt);
}

void RestAPISystem::apiWifiSetPEAP(String &reqStr, String &respStr)
{
    bool rslt = false;
    // Get SSID
    String ssid = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sWiFi SSID %s\n", MODULE_PREFIX, ssid.c_str());
    // Get identity
    String identity = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sWiFi Identity %s\n", MODULE_PREFIX, identity.c_str());
    // Get username
    String username = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
    Log.trace("%sWiFi Username %s\n", MODULE_PREFIX, username.c_str());
    // Get password
    String password = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 4);
    Log.trace("%sWiFi Password %s\n", MODULE_PREFIX, password.c_str());
    // Get hostname
    String hostname = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 5);
    Log.trace("%sHostname %s\n", MODULE_PREFIX, hostname.c_str());
    // Check if both SSID and pw have now been set
    if (ssid.length() != 0 && username.length() != 0)
    {
        Log.notice("%sWiFi Credentials Set SSID %s hostname %s\n", MODULE_PREFIX, ssid.c_str(), hostname.c_str());
        _wifiManager.setCredentialsPEAP(ssid, identity, username, password, hostname, true);
        rslt = true;
    }
    Utils::setJsonBoolResult(respStr, rslt);
}

void RestAPISystem::apiWifiClear(String &reqStr, String &respStr)
{
    // Clear stored SSIDs
    // wifiManager.clearCredentials();
    Log.notice("%sCleared WiFi Credentials\n", MODULE_PREFIX);
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiWifiExtAntenna(String &reqStr, String &respStr)
{
    Log.notice("%sSet external antenna - not supported\n", MODULE_PREFIX);
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiWifiIntAntenna(String &reqStr, String &respStr)
{
    Log.notice("%sSet internal antenna - not supported\n", MODULE_PREFIX);
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiMQTTSet(String &reqStr, String &respStr)
{
    // Get Server
    String server = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sMQTTServer %s\n", MODULE_PREFIX, server.c_str());
    // Get mqtt in topic
    String inTopic = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    inTopic.replace("~", "/");
    Log.trace("%sMQTTInTopic %s\n", MODULE_PREFIX, inTopic.c_str());
    // Get mqtt out topic
    String outTopic = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
    outTopic.replace("~", "/");
    Log.trace("%sMQTTOutTopic %s\n", MODULE_PREFIX, outTopic.c_str());
    // Get mqtt username
    String username = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 4);
    Log.trace("%sMQTTUsername %s\n", MODULE_PREFIX, username.c_str());
    // Get mqtt password
    String password = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 5);
    Log.trace("%sMQTTPassword %s\n", MODULE_PREFIX, password.c_str());
    // Get port
    int portNum = MQTTManager::DEFAULT_MQTT_PORT;
    String port = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 6);
    if (port.length() != 0)
        portNum = port.toInt();
    Log.trace("%sMQTTPort %d\n", MODULE_PREFIX, portNum);
    _mqttManager.setMQTTServer(server, inTopic, outTopic, username, password, portNum);
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiReset(String &reqStr, String& respStr)
{
    // Register that a restart is required but don't restart immediately
    // as the acknowledgement would not get through
    Utils::setJsonBoolResult(respStr, true);

    // Restart the device
    _deviceRestartPending = true;
    _deviceRestartMs = millis();
}

void RestAPISystem::apiNetLogLevel(String &reqStr, String &respStr)
{
    // Set the logging level for network logging
    String logLevel = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sNetLogLevel %s\n", MODULE_PREFIX, logLevel.c_str());
    _netLog.setLogLevel(logLevel.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogMQTT(String &reqStr, String &respStr)
{
    // Set MQTT as a destination for logging
    String onOffFlag = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    String topicStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sNetLogMQTT %s, topic %s\n", MODULE_PREFIX, onOffFlag.c_str(), topicStr.c_str());
    _netLog.setMQTT(onOffFlag != "0", topicStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogSerial(String &reqStr, String &respStr)
{
    // Set Serial as a destination for logging
    String onOffFlag = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    String portStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sNetLogSerial enabled %s, port %s\n", MODULE_PREFIX, onOffFlag.c_str(), portStr.c_str());
    _netLog.setSerial(onOffFlag != "0", portStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogCmdSerial(String &reqStr, String &respStr)
{
    // Set CommandSerial as a destination for logging
    String onOffFlag = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    Log.trace("%sNetLogCmdSerial enabled %s\n", MODULE_PREFIX, onOffFlag.c_str());
    _netLog.setCmdSerial(onOffFlag != "0");
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogHTTP(String &reqStr, String &respStr)
{
    // Set HTTP as a destination for logging
    String onOffFlag = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    String ipAddrOrHostname = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    String httpPortStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
    String urlStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 4);
    Log.trace("%sNetLogHTTP %s, ipHost %s, port %s, url %s\n", MODULE_PREFIX, 
                        onOffFlag.c_str(), ipAddrOrHostname.c_str(), httpPortStr.c_str(), urlStr.c_str());
    _netLog.setHTTP(onOffFlag != "0", ipAddrOrHostname.c_str(), httpPortStr.c_str(), urlStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogPT(String &reqStr, String &respStr)
{
    // Set PaperTrail as a destination for logging
    String onOffFlag = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    String hostName = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    String portStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
    Log.trace("%sNetLogPT %s, host %s, port %s\n", MODULE_PREFIX, 
                        onOffFlag.c_str(), hostName.c_str(), portStr.c_str());
    _netLog.setPapertrail(onOffFlag != "0", hostName.c_str(), portStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiNetLogGetConfig(String &reqStr, String &respStr)
{
    String configStr;
    _netLog.getConfig(configStr);
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());    
}

void RestAPISystem::apiCmdSchedGetConfig(String &reqStr, String &respStr)
{
    // Get config
    String configStr;
    _commandScheduler.getConfig(configStr);
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiWireGuardGetConfig(String &reqStr, String &respStr)
{
    // Get config
    String configStr;
    _wireGuardManager.getConfig(configStr);
    configStr = "\"wireGuard\":" + configStr;
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::api***EXPUNGED***GetConfig(String &reqStr, String &respStr)
{
    // Get config
    String configStr;
    ConfigBase wcConfig;
    wcConfig.setConfigData(_hwConfig.getString("***EXPUNGED***", "").c_str());
    configStr = "\"***EXPUNGED***\":{\"email\":\"" + wcConfig.getString("email", "") + "\",\"password\":\"" + wcConfig.getString("password", "") + "\",\"sisbot_id\":\"" + wcConfig.getString("sisbot_id", "") + "\",\"sisbot_mac\":\"" + wcConfig.getString("sisbot_mac", "") + "\"}";
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiPostCmdSchedule(String &reqStr, String &respStr)
{
    Log.notice("%sPostCmdSchedule %s\n", MODULE_PREFIX, reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(respStr, true);      
}

void RestAPISystem::apiPostCmdScheduleBody(String& reqStr, uint8_t *pData, size_t len, size_t index, size_t total)
{
    Log.notice("%sPostCmdScheduleBody len %d\n", MODULE_PREFIX, len);
    // Store the settings
    _commandScheduler.setConfig(pData, len);
}

void RestAPISystem::apiPostWireGuard(String &reqStr, String &respStr)
{
    Log.notice("%sPostWireGuard %s\n", MODULE_PREFIX, reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(respStr, true);      
}

void RestAPISystem::apiPostWireGuardBody(String& reqStr, uint8_t *pData, size_t len, size_t index, size_t total)
{
    Log.notice("%sPostWireGuardBody len %d\n", MODULE_PREFIX, len);
    // Store the settings
    _wireGuardManager.setConfig(pData, len);
}

void RestAPISystem::apiNTPGetConfig(String &reqStr, String &respStr)
{
    // Get NTP config
    String configStr;
    _ntpClient.getConfig(configStr);
    configStr = "\"ntp\":" + configStr;
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiNTPSetConfig(String &reqStr, String &respStr)
{
    // Set NTP
    String tzStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    tzStr.replace("~", "/");
    String serverStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    Log.trace("%sNNTPSetup TZ %s Server %s\n", MODULE_PREFIX, 
            tzStr.c_str(),
            serverStr.c_str());
    _ntpClient.setConfig(tzStr.c_str(), serverStr.c_str());
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiCheckUpdate(String &reqStr, String& respStr)
{
    // Register that an update check is required but don't start immediately
    // as the TCP stack doesn't seem to connect if the server is the same
    Utils::setJsonBoolResult(respStr, true);

    // Check for updates on update server
    _updateCheckPending = true;
    _updateCheckMs = millis();
}

void RestAPISystem::apiGetVersion(String &reqStr, String& respStr)
{
    respStr = "{\"sysType\":\""+ _systemType + "\", \"version\":\"" + _systemVersion + "\"}";
}

// Format file system
void RestAPISystem::apiReformatFS(String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    _fileManager.reformat(fileSystemStr, respStr);
}

// List files on a file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs, can be blank to default)
// The second part of the path is the folder - note that / must be replaced with ~ in folder
void RestAPISystem::apiFileList(String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Folder
    String folderStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    folderStr.replace("~", "/");
    if (folderStr.length() == 0)
        folderStr = "/";
    _fileManager.getFilesJSON(fileSystemStr, folderStr, respStr);
}

// Read file contents
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
void RestAPISystem::apiFileRead(String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String fileNameStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    fileNameStr.replace("~", "/");
    respStr = _fileManager.getFileContents(fileSystemStr, fileNameStr);
}

// Delete file on the file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the filename - note that / must be replaced with ~ in filename
void RestAPISystem::apiDeleteFile(String &reqStr, String& respStr)
{
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String filenameStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    bool rslt = false;
    filenameStr.replace("~", "/");
    if (filenameStr.length() != 0)
        rslt = _fileManager.deleteFile(fileSystemStr, filenameStr);
    Utils::setJsonBoolResult(respStr, rslt);
    Log.trace("%sdeleteFile fs %s, filename %s rslt %s\n", MODULE_PREFIX, 
                        fileSystemStr.c_str(), filenameStr.c_str(),
                        rslt ? "ok" : "fail");
}

// Upload file to file system - completed
void RestAPISystem::apiUploadToFileManComplete(String &reqStr, String &respStr)
{
    Log.trace("%sapiUploadToFileManComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    _fileManager.uploadAPIBlocksComplete();
    Utils::setJsonBoolResult(respStr, true);
}

// Upload file to file system - part of file (from HTTP POST file)
void RestAPISystem::apiUploadToFileManPart(String& req, String& filename, size_t contentLen, size_t index, 
                uint8_t *data, size_t len, bool finalBlock)
{
    Log.verbose("%sapiUpToFileMan %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0)
        _fileManager.uploadAPIBlockHandler("", req, filename, contentLen, index, data, len, finalBlock);
}

