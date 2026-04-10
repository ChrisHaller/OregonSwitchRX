
#include <Arduino.h>
#include "OregonDecoder.h"


OregonDecoder::OregonDecoder()
{ 
    ResetDecoder(); 
}

void OregonDecoder::PrintState()
{
    Serial.println("m_State: " + String(m_State));
    Serial.println("m_Bits: " + String(m_Bits));
    Serial.println("m_TotalBits: " + String(m_TotalBits));
    Serial.println("m_Pos: " + String(m_Pos));
    Serial.println("m_Flip: " + String(m_Flip));
    Serial.println("m_Pulse: " + String(m_Pulse));
    //Serial.println("m_LastPulse: " + String(m_LastPulse));
    Serial.println("m_DataValid: " + String(m_DataValid));

    Serial.printf("GetSensorCode: %02x\n\r", GetSensorCode());
    Serial.printf("GetSensorId: %d / 0x%x\n\r", GetSensorId(), GetSensorId() );
    Serial.printf("GetSensorId2: %d / 0x%x\n\r", GetSensorId2(), GetSensorId2() );
    Serial.printf("GetChannelNumber: %d\n\r", GetChannelNumber());
    Serial.printf("GetBatteryFlag: %d\n\r", GetBatteryFlag());
    Serial.printf("GetTemperature: %.1f\n\r", GetTemperature());
    Serial.printf("GetHumidity: %d\n\r", GetHumidity());
    Serial.printf("GetChecksum: %hhx\n\r", GetChecksum());
    Serial.printf("CheckChecksum: %hhx\n\r", CheckChecksum());
}

bool OregonDecoder::IsDataValid()
{
    return m_DataValid;
}

bool OregonDecoder::DecodeMessage()
{
    bool result = false;
    ReverseNibbles();


    if(m_Pos >= 10 && m_Pos <= 20)     // Grösse der Message muss zwischen 10 und 20 Bytes sein
    {

        m_DataValid = true;        
    }

    m_DataValid = result;
    return m_DataValid;

}

// Liefert den 16 Bit Sensor-Code, welcher innerhalb der Message ab Offset 4 steht
// Bsp Message: a1d203260621099063
//               ****
word OregonDecoder::GetSensorCode()
{
    word result = (m_Data[0] & 0xf) << 8;       // "1" aus erstem Byte
    result = result | m_Data[1];                // mit "d2" aus zweitem Byte verknüpfen
    result = result << 8;                       // das ganze um 8 Bit nach links schieben, dadurch entsteht dort Platz für das letzte Nibble
    result = result | (m_Data[2] & 0xf);        // mit "03" aus dem dritten Byte verküpfen (dadurch kommt auch ein unerwünschtes Nibble dazu)

    return result >> 4;                         // das unerwünschte Nibble rausschieben.
}

byte OregonDecoder::GetChannelNumber()
{
    return (m_Data[2] & 0xf);
}

byte OregonDecoder::GetSensorId()
{
    return m_Data[3];
}

// es ist unklar ob auch bei der ID die Nibbles getauscht werden.
// Ausserdem habe ich bisher immer die ID der nicht vertauschten Nibbles verwendet (was vermutlich falsch ist)

byte OregonDecoder::GetSensorId2()
{
    return SwapNibble(GetSensorId());
}



byte OregonDecoder::GetBatteryFlag()
{
     return m_Data[4] >> 4;
}

// a1 d2 03 16 05 72 0820230 = 1.9 anstelle von 27.5
//  0  1  2  3  4  5
float OregonDecoder::GetTemperature()
{
    int sign = (m_Data[6]&0x80) ? -1 : 1;
    byte smallTemp = m_Data[4] & 0xf;
    word temp = ((m_Data[5] & 0xf) * 10 + (m_Data[5] >> 4)) * 10;
    float t = (temp + smallTemp) / (float)10;
    return t * sign;
}


byte OregonDecoder::GetHumidity()
{
    return m_Data[6] + (m_Data[7] >> 4) * 10;
}


IRAM_ATTR void OregonDecoder::SetPulse()
{
    unsigned long  currentMicros = micros();
    m_Pulse = currentMicros - m_LastPulse;
    m_LastPulse = currentMicros;
}


unsigned long  OregonDecoder::GetPulseWidth()
{
    cli();
    unsigned long  p = m_Pulse;
    m_Pulse = 0;
    sei();
    return p;
}

bool OregonDecoder::NextPulse (unsigned long width)
{
    if (m_State != DONE)
    {
        switch (Decode(width)) 
        {
            case -1: 
                ResetDecoder();
                break;

            case 1:  
                Done(); 
                break;
        }
    }
    return IsDone();

}


void OregonDecoder::ResetDecoder () 
{
    m_TotalBits = m_Bits = m_Pos = m_Flip = 0;
    m_State = UNKNOWN;
    m_DataValid = false;
}

bool OregonDecoder::IsDone () const
{ 
    return m_State == DONE; 
}


void OregonDecoder::Done() 
{
    while (m_Bits)
    {
        GotBit(0); // padding
    }
    //Serial.println("Set DONE in Done()");
    m_State = DONE;
}


const byte* OregonDecoder::GetData (byte& count) const 
{
    count = m_Pos;
    return m_Data;
}


void OregonDecoder::GotBit (char value) 
{
    if(!(m_TotalBits & 0x01))
    {
        m_Data[m_Pos] = (m_Data[m_Pos] >> 1) | (value ? 0x80 : 00);
    }
    m_TotalBits++;
    m_Pos = m_TotalBits >> 4;
    if (m_Pos >= sizeof m_Data) {
        ResetDecoder();
        return;
    }
    //Serial.println("Set OK in GotBit()");
    m_State = OK;
}



// store a bit using Manchester encoding
void OregonDecoder::Manchester (char value) 
{
    m_Flip ^= value; // manchester code, long pulse flips the bit
    GotBit(m_Flip);
}



int OregonDecoder::Decode (unsigned long width)
{
    if (width >= 200  && width < 1200) 
    {
        byte w = width >= 700;

        switch (m_State) 
        {
            case UNKNOWN:
                if (w != 0) 
                {
                    // Long pulse
                    ++m_Flip;
                } 
                else if (w == 0 && 24 <= m_Flip) 
                {
                    // Short pulse, start bit
                    m_Flip = 0;
                    //Serial.println("Set T0 in unknown");
                    m_State = T0;
                } 
                else 
                {
                    // Reset decoder
                    return -1;
                }
                break;
            
            case OK:
                if (w == 0) 
                {
                    // Short pulse
                    //Serial.println("Set T0 in ok");
                    m_State = T0;
                } 
                else 
                {
                    // Long pulse
                    Manchester(1);
                }
                break;

            case T0:
                if (w == 0) 
                {
                    // Second short pulse
                    Manchester(0);
                } 
                else 
                {
                    // Reset decoder
                    return -1;
                }
                break;
        }
    } 
    else if (width >= 2500  && m_Pos >= 8) 
    {
        return 1;
    } 
    else 
    {
        return -1;
    }
    return 0;
}

void OregonDecoder::ReverseNibbles () 
{
    if(m_Pos < 25 && m_Pos >= 10)          // Nibbles nur swappen, wenn die Messagelänge sinnvoll ist
    {
        for (byte i = 0; i < m_Pos; ++i)
        {
            m_Data[i] =  SwapNibble(m_Data[i]);
        }
    }
}

byte OregonDecoder::SwapNibble(byte b)
{
    return ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
}



/**
 * \brief    Sum data for checksum
 * \param    count      number of bit to sum
 * \param    data       Oregon message
 */
int OregonDecoder::Sum(byte count)
{
  int s = 0;
 
  for(byte i = 0; i<count;i++)
  {
    s += (m_Data[i]&0xF0) >> 4;
    s += (m_Data[i]&0xF);
  }
 
  if(int(count) != count)
    s += (m_Data[count]&0xF0) >> 4;
 
  return s;
}
 
/**
 * \brief    Calculate checksum
 * \param    data       Oregon message
 */
byte OregonDecoder::GetChecksum()
{
    byte DHTMode = 1;       // // Temperature + Humidity [THGR2228N]
    byte result = 0;

    switch(DHTMode)
    {
        case 0:
            {
                int s = ((Sum(6) + (m_Data[6]&0xF) - 0xa) & 0xff);
                result |=  (s&0x0F) << 4;     
                //m_Data[7] =  (s&0xF0) >> 4;    
            }
            break;

        case 1:
            result = ((Sum(8) - 0xa) & 0xFF);
            break;

        default:
            result = ((Sum(10) - 0xa) & 0xFF);
            break;

    }
    return result;
}

bool OregonDecoder::CheckChecksum()
{
    return m_Data[8] == SwapNibble(GetChecksum());
}
