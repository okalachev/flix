// Votage measure code

//                 ______        if R1=27k R2=100k, Vin (full LIPO 1S Cell) = 4.2V
//    vin +BAT ---|__R1__|----+
//                            |
//                            |----------- vout to pin IO32
//                            |
//                            __
//                           |  |
//                           |R2|
//                           |  |
//                            |
//                           GND


float voltage; // output measure result in Volts
uint16_t bat_raw;

void volt_measure()
{
  
bat_raw = analogRead(v_input);
voltage = bat_raw * (3.3 / 4096.0);



}

void print_volt()
{

Serial.print (voltage);
return;

}