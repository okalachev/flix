// Votage measure code

//                 ______        if R1=33k R2=100k, Vin (full LIPO 1S Cell) = 4.35V
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

float R1 = 33000.0;
float R2 = 100000.0;
float buf;

float voltage; // output measure result in Volts
uint16_t bat_raw;

void volt_measure()
{
  
bat_raw = analogRead(v_input);
buf = (bat_raw * 3.25) / 4096;
voltage = buf / (R2/ (R1+R2)); //comp measures



}

void print_volt()
{

Serial.print (voltage);

return;

}