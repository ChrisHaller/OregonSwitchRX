#ifndef OregonDecoder_h
#define OregonDecoder_h
#include <Arduino.h>

class OregonDecoder
{
public:

    OregonDecoder();

    unsigned long GetPulseWidth();
    IRAM_ATTR void SetPulse();
    bool NextPulse (unsigned long width);
    void ResetDecoder();
    bool IsDone() const;
    void Done();
    const byte* GetData (byte& count) const;
    void GotBit (char value);
    void Manchester (char value);
    int Decode (unsigned long width); 
    bool IsDataValid();
    bool DecodeMessage();
    void PrintState();
    void ReverseNibbles ();
    word GetSensorCode();
    byte GetChannelNumber();
    byte GetSensorId();
    byte GetSensorId2();
    float GetTemperature();
    byte GetHumidity();
    byte GetBatteryFlag();
    byte SwapNibble(byte b);
    int Sum(byte count);
    byte GetChecksum();
    bool CheckChecksum();

    //void AlignTail (byte max);
    //void ReverseBits ();
    



    enum { UNKNOWN, T0, T1, T2, T3, OK, DONE };

    byte m_Data[25];

private:
    volatile unsigned long  m_Pulse;              // Zeit in ms zwischen zwei Interrupts (Pulse vom 433Mhz RX)
    volatile unsigned long  m_LastPulse;
    bool m_DataValid;

    byte m_TotalBits;
    byte m_Bits; 
    byte m_Flip;
    byte m_State;
    byte m_Pos;
    byte m_Pad;
    


};


#endif
