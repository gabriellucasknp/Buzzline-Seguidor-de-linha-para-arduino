// Buzzline - Seguidor de linha usando SENSORES DE LUZ 

// ------------------- CONFIGURAÇÃO DE PINOS ---------------------
const uint8_t PIN_BUTTON   = 2;    // Botão para iniciar / calibrar (INPUT_PULLUP)
const uint8_t PIN_LED      = 13;   // LED indicador
const uint8_t PIN_MARKER   = A0;   // Opcional: sensor de marcador (analógico), pode remover se não usar

// Motores: esquema PWM + direção (ajuste para seu driver)
const uint8_t MOTOR_L_PWM  = 5;    // PWM motor esquerdo
const uint8_t MOTOR_L_DIR  = 4;    // DIR motor esquerdo
const uint8_t MOTOR_R_PWM  = 6;    // PWM motor direito
const uint8_t MOTOR_R_DIR  = 7;    // DIR motor direito

// Sensores de luz (analógicos). Geralmente 3 sensores para comportamento simples:
// coloque o sensor do centro sobre a linha quando pedir.
const uint8_t SENSOR_LEFT   = A1;
const uint8_t SENSOR_CENTER = A2;
const uint8_t SENSOR_RIGHT  = A3;

// ------------------- PARÂMETROS DE COMPORTAMENTO ----------------
int baseSpeed = 200;        // velocidade base (0-255)
int turnSpeed = 120;        // velocidade reduzida no lado interno da curva
int thresholdLine = 600;    // limiar para detectar linha: valor analógico (0..1023). Ajustar após calibração.
bool lineIsDark = true;     // true se a linha for mais escura que o fundo (preto sobre branco)

// Calibração simples: armazena valores para branco e linha
int calibWhiteLeft = 1023, calibWhiteCenter = 1023, calibWhiteRight = 1023;
int calibLineLeft  = 0, calibLineCenter  = 0, calibLineRight  = 0;

// Estado de execução
bool running = false;
int lastDirection = 0; // -1 = viu à esquerda por último, 0 = centro, +1 = direita

// ------------------- FUNÇÕES AUXILIARES -------------------------
void setMotor(int left, int right) {
  // left/right: -255..255 (negativo reverso)
  if (left >= 0) {
    digitalWrite(MOTOR_L_DIR, HIGH);
    analogWrite(MOTOR_L_PWM, constrain(left, 0, 255));
  } else {
    digitalWrite(MOTOR_L_DIR, LOW);
    analogWrite(MOTOR_L_PWM, constrain(-left, 0, 255));
  }

  if (right >= 0) {
    digitalWrite(MOTOR_R_DIR, HIGH);
    analogWrite(MOTOR_R_PWM, constrain(right, 0, 255));
  } else {
    digitalWrite(MOTOR_R_DIR, LOW);
    analogWrite(MOTOR_R_PWM, constrain(-right, 0, 255));
  }
}

int readSensor(uint8_t pin) {
  return analogRead(pin); // 0..1023
}

bool detectLineValue(int value, int whiteRef, int lineRef) {
  // Decisão robusta baseada em referências calibradas.
  // Se a linha for mais escura: valor menor no lineRef; caso contrário, maior.
  if (lineIsDark) {
    // quanto menor em relação ao branco, mais chance de ser linha
    return value < (whiteRef + lineRef) / 2 && value < thresholdLine;
  } else {
    return value > (whiteRef + lineRef) / 2 && value > thresholdLine;
  }
}

void stopRobot() {
  setMotor(0, 0);
  digitalWrite(PIN_LED, LOW);
  running = false;
}

// ------------------- CALIBRAÇÃO SIMPLES -------------------------
void calibrate() {
  // Instruções: coloque o robô sobre o FUNDO (sem a linha) e aperte o botão.
  // Depois coloque sobre a LINHA e aperte novamente.
  Serial.println("CALIBRACAO: coloque sobre o FUNDO e pressione o botao.");
  while (digitalRead(PIN_BUTTON) == HIGH) delay(10); // aguarda pressionar (PULLUP)
  delay(200); // debounce

  // Ler várias amostras e tirar média
  long sumL = 0, sumC = 0, sumR = 0;
  const int SAMPLES = 20;
  for (int i = 0; i < SAMPLES; i++) {
    sumL += readSensor(SENSOR_LEFT);
    sumC += readSensor(SENSOR_CENTER);
    sumR += readSensor(SENSOR_RIGHT);
    delay(20);
  }
  calibWhiteLeft = sumL / SAMPLES;
  calibWhiteCenter = sumC / SAMPLES;
  calibWhiteRight = sumR / SAMPLES;
  Serial.print("Fundo medido -> L:"); Serial.print(calibWhiteLeft);
  Serial.print(" C:"); Serial.print(calibWhiteCenter);
  Serial.print(" R:"); Serial.println(calibWhiteRight);

  Serial.println("Agora coloque sobre a LINHA e pressione o botao.");
  while (digitalRead(PIN_BUTTON) == HIGH) delay(10);
  delay(200);

  sumL = sumC = sumR = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sumL += readSensor(SENSOR_LEFT);
    sumC += readSensor(SENSOR_CENTER);
    sumR += readSensor(SENSOR_RIGHT);
    delay(20);
  }
  calibLineLeft = sumL / SAMPLES;
  calibLineCenter = sumC / SAMPLES;
  calibLineRight = sumR / SAMPLES;
  Serial.print("Linha medido -> L:"); Serial.print(calibLineLeft);
  Serial.print(" C:"); Serial.print(calibLineCenter);
  Serial.print(" R:"); Serial.println(calibLineRight);

  // Ajuste automático do limiar se necessário (média das leituras)
  int avgWhite = (calibWhiteLeft + calibWhiteCenter + calibWhiteRight) / 3;
  int avgLine  = (calibLineLeft + calibLineCenter + calibLineRight) / 3;
  thresholdLine = (avgWhite + avgLine) / 2;
  Serial.print("Threshold ajustado para: "); Serial.println(thresholdLine);

  // Detecta se a linha é mais escura ou clara que o fundo
  lineIsDark = (avgLine < avgWhite);
  Serial.print("Linha escura? ");
  Serial.println(lineIsDark ? "SIM" : "NAO");
}

// ------------------- SETUP -------------------------------------
void setup() {
  pinMode(MOTOR_L_PWM, OUTPUT);
  pinMode(MOTOR_R_PWM, OUTPUT);
  pinMode(MOTOR_L_DIR, OUTPUT);
  pinMode(MOTOR_R_DIR, OUTPUT);

  pinMode(PIN_BUTTON, INPUT_PULLUP); // ajuste se usar resistor externo
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_MARKER, INPUT);

  analogWrite(MOTOR_L_PWM, 0);
  analogWrite(MOTOR_R_PWM, 0);

  Serial.begin(115200);
  Serial.println("Buzzline - Light Sensors (Sem PID)");
  Serial.println("Pressione o botao para iniciar calibracao.");

  // Espera e calibra
  while (digitalRead(PIN_BUTTON) == HIGH) delay(10); // aguarda pressionar para iniciar calibração
  delay(200);
  calibrate();

  // Após calibração, aguarda outro pressionar para iniciar corrida
  Serial.println("Pressione o botao novamente para iniciar a corrida.");
  while (digitalRead(PIN_BUTTON) == HIGH) delay(10);
  delay(200);

  running = true;
  digitalWrite(PIN_LED, HIGH);
  Serial.println("Iniciando...");
}

// ------------------- LOOP PRINCIPAL -----------------------------
void loop() {
  if (!running) {
    // permite reiniciar com botão
    if (digitalRead(PIN_BUTTON) == LOW) {
      Serial.println("Reiniciando: calibracao novamente.");
      delay(200);
      calibrate();
      running = true;
      digitalWrite(PIN_LED, HIGH);
      delay(200);
    }
    return;
  }

  // Leitura dos sensores
  int vL = readSensor(SENSOR_LEFT);
  int vC = readSensor(SENSOR_CENTER);
  int vR = readSensor(SENSOR_RIGHT);

  // Detecta presença de linha em cada sensor usando calibração
  bool leftOn  = detectLineValue(vL, calibWhiteLeft, calibLineLeft);
  bool centerOn= detectLineValue(vC, calibWhiteCenter, calibLineCenter);
  bool rightOn = detectLineValue(vR, calibWhiteRight, calibLineRight);

  // Lógica simples de tomada de decisão (bang-bang com memória)
  if (centerOn) {
    // Linha centralizada: segue reto
    setMotor(baseSpeed, baseSpeed);
    lastDirection = 0;
  } else if (leftOn && !rightOn) {
    // Linha à esquerda: gira para esquerda reduzindo velocidade esquerda
    setMotor(turnSpeed, baseSpeed);
    lastDirection = -1;
  } else if (rightOn && !leftOn) {
    // Linha à direita: gira para direita reduzindo velocidade direita
    setMotor(baseSpeed, turnSpeed);
    lastDirection = +1;
  } else if (leftOn && rightOn) {
    // Caso raro (linha larga ou cruzamento): vai reto ou reduz velocidade
    setMotor(turnSpeed, turnSpeed);
    lastDirection = 0;
  } else {
    // Nenhum sensor detectou: usa última direção conhecida para procurar linha
    if (lastDirection < 0) {
      // procurar à esquerda
      setMotor(turnSpeed, -turnSpeed/2); // giro leve para esquerda
    } else if (lastDirection > 0) {
      // procurar à direita
      setMotor(-turnSpeed/2, turnSpeed); // giro leve para direita
    } else {
      // último era centro: seguir reto devagar
      setMotor(turnSpeed, turnSpeed);
    }
  }

  // Checa marcador opcional (se quiser parar depois de um marcador físico)
  if (analogRead(PIN_MARKER) < 50) { // ajuste do limiar conforme sensor
    Serial.println("Marcador detectado - parada.");
    stopRobot();
  }

  // Telemetria ocasional para ajuste
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  if (now - lastPrint > 300) {
    Serial.print("L:"); Serial.print(vL);
    Serial.print(" C:"); Serial.print(vC);
    Serial.print(" R:"); Serial.print(vR);
    Serial.print("  Det:["); Serial.print(leftOn); Serial.print(",");
    Serial.print(centerOn); Serial.print(","); Serial.print(rightOn); Serial.println("]");
    lastPrint = now;
  }

  delay(10); // pequeno atraso para estabilidade
}
