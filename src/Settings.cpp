#include <Arduino.h>
#include "Settings.h"


Settings::Settings()
{ 

}


void printHexDump(const char* data, int length) {
  for (int i = 0; i < length; i++) {
    char c = data[i];
    
    // Gib den Charakter in Hexadezimalform aus
    if (c < 0x10) {
      Serial.print('0');
    }
    Serial.print(c, HEX);
    
    // Füge ein Leerzeichen oder eine neue Zeile nach jeder Gruppe von Bytes hinzu
    if ((i + 1) % 16 == 0) {
      Serial.println();
    } else {
      Serial.print(' ');
    }
  }
  Serial.println(); // Füge eine abschließende Zeile am Ende hinzu
}


void Settings::Init()
{ 
    //Serial.println("Settings::Init()");
    m_Preferences.begin("oregonrx", false);
    //m_Preferences.clear();
    ReadPrefs();

    char* b = GetMQTTServername();
    printHexDump(b, 16);

    if(strcmp(GetMQTTServername(), "") == 0)
    {
        //Serial.println("Set default settings");
        SetMQTTServername(DefaultMqttBroker);
        SetMQTTPort(DefaultMqttPort);
        SetMQTTUsername(DefaultMqttUsername);
        SetMQTTPassword(DefaultMqttPassword);
        Serial.println("Set default settings");
    }
    ReadPrefs();

    //String a = GetMQTTServername();
    //Serial.println("Setting server = " + String(a));
}


void Settings::ReadPrefs()
{
    m_Preferences.getString("mqttservername", "").toCharArray(m_mqttServername, sizeof(m_mqttServername));
    m_Preferences.getString("mqttusername", "").toCharArray(m_mqttUsername, sizeof(m_mqttUsername));
    m_Preferences.getString("mqttpassword", "").toCharArray(m_mqttPassword, sizeof(m_mqttPassword));
    m_mqttPort =   m_Preferences.getInt("mqttport", 0);
}


char* Settings::GetMQTTServername()
{
    return m_mqttServername;
}

char* Settings::GetMQTTUsername()
{
    return m_mqttUsername;
}

char* Settings::GetMQTTPassword()
{
    return m_mqttPassword;
}

int Settings::GetMQTTPort()
{
    return m_mqttPort;
}



void Settings::SetMQTTServername(String mqttServername)
{
    m_Preferences.putString("mqttservername", mqttServername);
}

void Settings::SetMQTTUsername(String mqttUsername)
{
    m_Preferences.putString("mqttusername", mqttUsername);
}

void Settings::SetMQTTPassword(String mqttPassword)
{
    m_Preferences.putString("mqttpassword", mqttPassword);
}

void Settings::SetMQTTPort(int mqttPort)
{
    m_Preferences.putInt("mqttport", mqttPort);
}

