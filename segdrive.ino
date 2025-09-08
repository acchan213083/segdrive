#define motor1 2
#define motor2 3
#define DIO 6
#define CLK 7

bool state = false;
int t = 5;
int cicle = 0;

void setup() {
  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);
}

void loop() {
  drive();
  display();
  t -= 1;
  delay(1000);

  if(t == 0 && state == true){
    state = false;
    t == 5;
    if(cicle == 10){
      finish();
    }
    display();
  } else if (t == 0 && state == false) {
    state = true;
    cicle += 1;
    t = 45;
    display();
  }
}

void drive(){
  if(state == true) {
    digitalWrite(motor1, HIGH);
    digitalWrite(motor2, HIGH);
  } else if(state == false){
    digitalWrite(motor1, LOW);
    digitalWrite(motor2, LOW);
  } 
}

void display() {
  
}

void finish(){

}
