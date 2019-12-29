void getTemp(){
   if(Sensor1.getTemperature(&groupTemperatureData)){
   uint16_t tmp = groupTemperatureData;
   
  int r = tmp - previousTMP;
  if( r < 0){
    r = -r;
  }

  if (r < 50 || error >= 5){
    previousTMP = tmp;
    error = 0;
  }
  else{
    tmp = previousTMP;
    error++;
  }
 
  total = total - readings[readIndex];
  readings[readIndex] = tmp;  
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
   readIndex = 0;
  }

  groupTemperatureData = total / numReadings;
   }
   groupTemperature = Sensor1.calc_Celsius(&groupTemperatureData);
}
