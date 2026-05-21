#include <HX711_ADC.h>

const int HX711_dout = 16;
const int HX711_sck  = 4;

HX711_ADC LoadCell(HX711_dout, HX711_sck);

void setup() {
  Serial.begin(115200);

  LoadCell.begin();
  LoadCell.start(2000);
  LoadCell.tare();

  LoadCell.setCalFactor(342.512);

  while (!LoadCell.update()); // tunggu siap
}

void loop() {
  if (LoadCell.update()) {
    float i = LoadCell.getData();

    if (i < 0) i = 0;

    Serial.print("Weight[g]: ");
    Serial.println(i);
  }
}