#include <EEPROM.h>
#include <Stepper.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <BH1750.h>
#include <stdlib.h>

#define STEPS_per_Rev 2048

//각도 주소값을 정의함.
int addr = 1;

//각도값을 정의함.
double pos;

//조도값을 정의함
double lux;

//각도와 스텝의 비(rate) 정의함.
double rate = 360./2048.;

      //pin번호 정의. 
        //LCD pins
const int rs = 8,
          en = 9,
          d4 = 10,
          d5 = 11,
          d6 = 12,
          d7 = 13,
          
        //BH1750 pins
          pin_SDA = A4,
          pin_SCL = A5,
          
        //Stepper pins
          S_pin1 = 5,
          S_pin2 = 4,
          S_pin3 = 3,
          S_pin4 = 2;

Stepper stepper(STEPS_per_Rev,
                S_pin1,
                S_pin3,
                S_pin2,
                S_pin4);


BH1750 lightMeter;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


//setup 함수.
void setup() {  

  //핀 설정
  pinMode(S_pin1, OUTPUT);
  pinMode(S_pin2, OUTPUT);
  pinMode(S_pin3, OUTPUT);
  pinMode(S_pin4, OUTPUT);
  
  //최근 저장된 각도를 불러옴.
  pos = EEPROM.read(addr);

  //serial monitor를 사용함.
  Serial.begin(9600);

  //I2C 통신을 위해 wire객체를 사용함.
  //Wire.begin();
  Wire.begin(pin_SCL);
  Wire.begin(pin_SDA);

  //BH1750 사용.
  //몇 가지 모드가 있는데,
  //CONTINUOUS_HIGH_RES_MODE_2는 0.25 lx 간격으로
  //0~32767(65536/2-1)lx만큼의 범위에서
  //측정 가능함.
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2);

  //Serial Monitor에 메세지 표시
  print_serial(pos, 0);
  
  //LCD 모니터에도 
  //Serial Monitor와 동일한 내용 표시
  //최종적으로는 외부 컨트롤러를 달아
  //컴퓨터에 연결하지 않고 LCD로 값을
  //확인 가능하도록 하고, 
  //컨트롤러에서 직접 입력을 받아들일 것임.
  lcd.begin(16, 2);
  print_lcd(pos, 0);
  
}

void loop() {
  //시리얼 메세지.
  init_message();
  
  //시리얼 입력을 기다렸다가 받아들임.
  String com = wait_string_input();

  //작업을 수행함.
  control(com);

  //동작 수행 후 약간의 딜레이.
  delay(1000);
}



/************* 함수 **************/

void control(String command) {
  //명령어에 따라 반응
  if (command == String("MOVE\n") || command == String("move\n")) {
    Serial.println("각도를 입력해 주세요.");
    String pos0 = wait_string_input();
    double pos1 = pos0.toDouble();
    
    //목표 각도를 시리얼 모니터에 표시함.
    Serial.print("Input Angle: ");
    Serial.println(pos0);
    
    move_to(pos1, "off");
    
  } else if (command == "MEASURE\n" || command == "measure\n" || command == "MEAS\n") {
    Serial.println(".");
    //BH1750이 조도를 측정함.
    measure();
    
  } else if (command == "SWEEP\n" || command == "sweep\n") {
    Serial.println("시작할 각도를 입력해 주세요.");
    String pos0 = wait_string_input();
    double pos1 = pos0.toDouble();
    
    //목표 각도를 시리얼 모니터에 표시함.
    Serial.print("Initial Angle: ");
    Serial.print(pos0);
    
    Serial.println("움직일 각도를 입력해 주세요.");
    String angle0 = wait_string_input();
    double angle1 = angle0.toDouble();
    
    //움직일 각도를 시리얼 모니터에 표시함.
    Serial.print("\tAngle to move: ");
    Serial.print(angle0);
    
    sweep(pos1, angle1);
        
  } else {
    Serial.println("Error: invalid command.");
  
  }
}

 
void print_lcd(int step_pos, double latest_lux) {
  
  double step_pos0 = double(step_pos*rate);
  
  //LCD초기화
  lcd.clear();
  
  //Degree  Position
  //:(pos)  :(lux)
  lcd.setCursor(0, 0);
  lcd.print("Degree Int.(lux)");
  
  lcd.setCursor(0, 1);
  lcd.print(":");
  lcd.print(step_pos0);

  lcd.setCursor(7, 1);
  lcd.print(":");
  lcd.print(latest_lux);
  
  //함수를 끝냄
  return;
}



void print_serial(int step_pos, double latest_lux) {

  double step_pos0 = double(step_pos*rate);
  
  //Serial monitor에 다음 내용을 출력함.
  //Degree: (pos)   Light: (lux) lx
  Serial.print("Degree: ");
  Serial.print(step_pos0);
  Serial.print("\t");
  
  Serial.print("Intensity: ");
  Serial.print(latest_lux);
  Serial.println(" lx");
}



void init_message() {
  Serial.println("Mode:");
  Serial.println("\t MOVE: 해당 각도로 이동");
  Serial.println("\t MEASURE: 현재 위치에서 측정");
  Serial.println("\t SWEEP: 초기 각도에서 원하는 각도만큼 이동하며 측정");
  Serial.println("\t (0.18도 간격으로 회전.)");
}



String wait_string_input() {
  
  //String변수를 정의함.
  String serial_input = "";
  
  //시리얼 입력을 기다림.
  while (HIGH){
    if(Serial.available()>0){
    serial_input = Serial.readString();
    }
    
    if (serial_input != "")
      break;
  }
  
  //시리얼 입력으로 받은 String변수를 반환함.
  return serial_input;
}



void move_to(double pos_input, String sweep_mode) {
  //이동해야 할 스텝을 구함.
  int val = int(pos_input/rate - pos);
  int mov;
  
  //회전 방향을 정함.
  if (val >= 0) {
    mov = +1;
  } else{
    mov = -1;
  }

  val = mov*val;
  
  for (int i = 0; i < val; i++) {
    //스텝만큼 회전시킴.
    stepper.step(mov);
    
    //회전시킨 만큼 위치값을 보정함.
    pos = pos + mov;
    
    if (sweep_mode == "on") {
      measure();
    } else {
      //lcd출력.
      print_lcd(pos,lux);
      
      //시리얼 출력
      print_serial(pos,lux);
    }
    
    if (pos >= 2048) {
      pos -= 2048;
    } else if (pos < 0) {
      pos += 2048;
    }
    
    //EEPROM에 기록함
    EEPROM.write(addr, pos);
  }
}



void measure() {
  //BH1750이 측정하기 위한 지연 시간을 줌.
  delay(200); //high resolution: delay >= 120ms
  
  //BH1750이 조도를 측정함.
  lux = double(lightMeter.readLightLevel());

  //lcd출력.
  print_lcd(pos,lux);
  
  //시리얼 출력
  print_serial(pos,lux);
  
  return;
}



void sweep(double pos_initial, double sweep_angle) {

  //초기 각도로 이동함.
  move_to(pos_initial, "off");

  //각도를 스텝으로 변환.
  move_to(sweep_angle+pos_initial, "on");

}
