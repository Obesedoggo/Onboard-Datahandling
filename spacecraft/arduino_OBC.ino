void setup() {
  serial.begin(9600);
  int x = 0;

}

void loop() {
  if(serial.available()){
    x++;
    serial.println(x + "th message recieved");
  }

}
