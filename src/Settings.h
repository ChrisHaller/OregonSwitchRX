#ifndef Settings_h
#define Settings_h
#include <Arduino.h>
#include <Preferences.h>


#define DefaultMqttBroker  "192.168.100.15"
#define DefaultMqttUsername  "mqtt"
#define DefaultMqttPassword "joshua"
#define DefaultMqttPort  1883


class Settings
{
public:
    Settings();
    void Init();
    void ReadPrefs();
    void SetMQTTServername(String mqttServername);
    void SetMQTTPort(int mqttPort);
    void SetMQTTUsername(String mqttUsername);
    void SetMQTTPassword(String mqttPassword);

    char* GetMQTTServername();
    int GetMQTTPort();
    char* GetMQTTUsername();
    char* GetMQTTPassword();

private:
    Preferences m_Preferences;
    char m_mqttServername[64];
    char m_mqttUsername[16];
    char m_mqttPassword[16];
    int m_mqttPort;
};

#endif

