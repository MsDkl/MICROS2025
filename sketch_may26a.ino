// === Pines para el MAX7219 ===
#define DATA_PIN 23   // Pin para enviar datos al MAX7219 (DIN)
#define CLK_PIN  18   // Pin de reloj para el MAX7219 (CLK)
#define CS_PIN    5   // Pin de carga (CS/LOAD) del MAX7219

// === Pines del joystick 1 (control de la serpiente) ===
#define VRX_PIN 34    // Movimiento horizontal
#define VRY_PIN 35    // Movimiento vertical

// === Pines del joystick 2 (control de la manzana en modo alternativo) ===
#define VRX2_PIN 32   // Movimiento horizontal
#define VRY2_PIN 33   // Movimiento vertical
#define SW2_PIN  25   // Botón del segundo joystick

// === Estructuras de datos ===
typedef struct Snake {
  int head[2];         // Posición de la cabeza [fila, columna]
  int body[40][2];     // Posiciones del cuerpo (máx 40 segmentos)
  int len;             // Longitud actual de la serpiente
  int dir[2];          // Dirección de movimiento [fila, columna]
} Snake;

typedef struct Apple {
  int rPos;            // Fila de la manzana
  int cPos;            // Columna de la manzana
} Apple;

// === Variables globales ===
byte pic[8] = {0};         // Matriz de 8 filas (cada byte representa una fila de la matriz LED)
Snake snake = {{1, 5}, {{0, 5}, {1, 5}}, 2, {1, 0}};  // Inicialización de la serpiente
Apple apple;

int obstaclesLevel0[][2] = {{3, 3}, {4, 4}, {2, 6}};
int obstaclesLevel1[][2] = {{1, 1}, {6, 6}, {5, 2}};
int obstaclesLevel2[][2] = {{0, 7}, {7, 0}, {3, 4}};
int* currentObstacles = (int*)obstaclesLevel0;  // Puntero al nivel actual de obstáculos
int numObstacles = 3;

int applesEaten = 0;     // Contador de manzanas comidas
float oldTime = 0;       // Tiempo anterior para calcular deltaTime
float timer = 0;         // Acumulador de tiempo
float updateRate = 3.0;  // Frecuencia de actualización del juego
int blinkCounter = 0;    // Contador para parpadeo de manzana
bool appleVisible = true;// Estado visible/invisible de la manzana (parpadeo)
int level = 1;           // Nivel actual del juego

// === Variables para el modo alternativo ===
bool alternateMode = false;     // Bandera de modo alternativo (manzana controlada)
bool lastButtonState = HIGH;    // Último estado del botón
int appleMoveCooldown = 0;      // Retardo para mover la manzana

// === Función que envía un byte al MAX7219 ===
void sendByte(byte reg, byte data) {
  digitalWrite(CS_PIN, LOW);
  shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, reg);   // Dirección del registro (1-8)
  shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, data);  // Datos para ese registro
  digitalWrite(CS_PIN, HIGH);
}

// === Borra toda la matriz LED ===
void clearMatrix() {
  for (byte i = 1; i <= 8; i++) {
    sendByte(i, 0x00);  // Apaga todas las filas
  }
}

// === Inicializa el MAX7219 ===
void initMAX7219() {
  sendByte(0x09, 0x00); // Sin decodificación
  sendByte(0x0A, 0x08); // Brillo medio
  sendByte(0x0B, 0x07); // Escaneo de las 8 filas
  sendByte(0x0C, 0x01); // Modo normal (no shutdown)
  sendByte(0x0F, 0x00); // Desactiva test
  clearMatrix();        // Limpia matriz al inicio
}

// === Muestra en la matriz el contenido de `pic[]` ===
void renderMatrix() {
  for (int i = 0; i < 8; i++) {
    sendByte(i + 1, pic[i]);  // Muestra fila por fila
  }
}

// === Muestra número de nivel alcanzado (0 a 9) ===
void showLevel() {
  clearMatrix();
  byte digits[9][8] = {
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00}, // 1
    {0x7E,0x06,0x3E,0x60,0x7E,0x00,0x00,0x00}, // 2
    {0x7E,0x06,0x3E,0x06,0x7E,0x00,0x00,0x00}, // 3
    {0x66,0x66,0x7E,0x06,0x06,0x00,0x00,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x7E,0x00,0x00,0x00}, // 5
    {0x3E,0x60,0x7C,0x66,0x3E,0x00,0x00,0x00}, // 6
    {0x7E,0x06,0x0C,0x18,0x18,0x00,0x00,0x00}, // 7
    {0x3E,0x66,0x3E,0x66,0x3E,0x00,0x00,0x00}, // 8
    {0x3E,0x66,0x3E,0x06,0x3E,0x00,0x00,0x00}  // 9
  };

  int digitIndex = level - 1;
  if (digitIndex < 0) digitIndex = 0;
  if (digitIndex > 8) digitIndex = 8;

  for (int i = 0; i < 8; i++) {
    sendByte(i + 1, digits[digitIndex][i]);
  }
  delay(1500);
  clearMatrix();
}

// === Genera una manzana en una posición válida (no sobre el cuerpo ni obstáculos) ===
void generateApple() {
  bool valid = false;
  while (!valid) {
    apple.rPos = random(0, 8);
    apple.cPos = random(0, 8);
    valid = true;
    for (int i = 0; i < snake.len; i++) {
      if (snake.body[i][0] == apple.rPos && snake.body[i][1] == apple.cPos)
        valid = false;
    }
    for (int i = 0; i < numObstacles; i++) {
      int* o = &currentObstacles[i * 2];
      if (o[0] == apple.rPos && o[1] == apple.cPos)
        valid = false;
    }
  }
}

// === Animación de "game over" ===
void gameOverAnimation() {
  for (int k = 0; k < 3; k++) {
    clearMatrix();
    delay(200);
    for (int i = 1; i <= 8; i++) sendByte(i, 0xFF);
    delay(200);
  }
  showLevel();  // Muestra el nivel alcanzado
}

// === Reinicia la serpiente al comenzar de nuevo ===
void resetSnake() {
  snake = {{1, 5}, {{0, 5}, {1, 5}}, 2, {1, 0}};
  updateRate = 3.0;
  level = 1;
  applesEaten = 0;
  currentObstacles = (int*)obstaclesLevel0;
  generateApple();
}

// === Desplaza el cuerpo de la serpiente eliminando el primer segmento ===
void removeFirst() {
  for (int j = 1; j < snake.len; j++) {
    snake.body[j - 1][0] = snake.body[j][0];
    snake.body[j - 1][1] = snake.body[j][1];
  }
}

// === Cambia el conjunto de obstáculos según el nivel ===
void updateObstacles() {
  if (level == 1) currentObstacles = (int*)obstaclesLevel0;
  else if (level == 2) currentObstacles = (int*)obstaclesLevel1;
  else currentObstacles = (int*)obstaclesLevel2;

  numObstacles = 3;
}

// === Lógica principal del juego (actualiza serpiente, manzana, colisiones) ===
void updateGame() {
  for (int j = 0; j < 8; j++) pic[j] = 0;

  int newHead[2] = {snake.head[0] + snake.dir[0], snake.head[1] + snake.dir[1]};
  if (newHead[0] == 8) newHead[0] = 0;
  if (newHead[0] == -1) newHead[0] = 7;
  if (newHead[1] == 8) newHead[1] = 0;
  if (newHead[1] == -1) newHead[1] = 7;

  for (int j = 0; j < snake.len; j++) {
    if (snake.body[j][0] == newHead[0] && snake.body[j][1] == newHead[1]) {
      gameOverAnimation();
      resetSnake();
      return;
    }
  }

  for (int i = 0; i < numObstacles; i++) {
    int* o = &currentObstacles[i*2];
    if (newHead[0] == o[0] && newHead[1] == o[1]) {
      gameOverAnimation();
      resetSnake();
      return;
    }
  }

  if (newHead[0] == apple.rPos && newHead[1] == apple.cPos) {
    snake.len++;
    applesEaten++;
    updateRate += 0.25;

    if (applesEaten % 2 == 0 && level < 10) {
      level++;
      updateObstacles();
      updateRate += 0.5;
    }
    generateApple();
  } else {
    removeFirst();
  }

  snake.body[snake.len - 1][0] = newHead[0];
  snake.body[snake.len - 1][1] = newHead[1];
  snake.head[0] = newHead[0];
  snake.head[1] = newHead[1];

  for (int j = 0; j < snake.len; j++) {
    pic[snake.body[j][0]] |= 1 << (7 - snake.body[j][1]);
  }

  blinkCounter++;
  if (blinkCounter >= 2) {
    blinkCounter = 0;
    appleVisible = !appleVisible;
  }
  if (appleVisible) {
    pic[apple.rPos] |= 1 << (7 - apple.cPos);
  }

  for (int i = 0; i < numObstacles; i++) {
    int* o = &currentObstacles[i*2];
    pic[o[0]] |= 1 << (7 - o[1]);
  }
}

// === Calcula el deltaTime para velocidad constante ===
float calculateDeltaTime() {
  float currentTime = millis();
  float dt = currentTime - oldTime;
  oldTime = currentTime;
  return dt;
}

// === Configuración inicial ===
void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  pinMode(VRX_PIN, INPUT);
  pinMode(VRY_PIN, INPUT);
  pinMode(VRX2_PIN, INPUT);
  pinMode(VRY2_PIN, INPUT);
  pinMode(SW2_PIN, INPUT_PULLUP);
  digitalWrite(CS_PIN, HIGH);

  Serial.begin(9600);
  randomSeed(analogRead(0));
  initMAX7219();
  generateApple();
}

// === Bucle principal del juego ===
void loop() {
  float deltaTime = calculateDeltaTime();
  timer += deltaTime;

  // Movimiento de la serpiente
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);

  if (xVal < 1000 && snake.dir[1] == 0) {
    snake.dir[0] = 0; snake.dir[1] = 1;
  } else if (xVal > 3000 && snake.dir[1] == 0) {
    snake.dir[0] = 0; snake.dir[1] = -1;
  } else if (yVal < 1000 && snake.dir[0] == 0) {
    snake.dir[0] = -1; snake.dir[1] = 0;
  } else if (yVal > 3000 && snake.dir[0] == 0) {
    snake.dir[0] = 1; snake.dir[1] = 0;
  }

  if (timer > 1000 / updateRate) {
    timer = 0;
    updateGame();
  }

  renderMatrix();

  // Cambiar a modo alternativo
  bool buttonState = digitalRead(SW2_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    alternateMode = !alternateMode;
    showLevel();
  }
  lastButtonState = buttonState;

  // Mover la manzana en modo alternativo
  if (alternateMode) {
    int ax = analogRead(VRX2_PIN);
    int ay = analogRead(VRY2_PIN);

    if (appleMoveCooldown <= 0) {
      int newR = apple.rPos;
      int newC = apple.cPos;

      if (ax < 1000) newC++;
      else if (ax > 3000) newC--;
      else if (ay < 1000) newR--;
      else if (ay > 3000) newR++;

      if (newR < 0) newR = 7;
      if (newR > 7) newR = 0;
      if (newC < 0) newC = 7;
      if (newC > 7) newC = 0;

      bool valid = true;
      for (int i = 0; i < snake.len; i++) {
        if (snake.body[i][0] == newR && snake.body[i][1] == newC)
          valid = false;
      }
      for (int i = 0; i < numObstacles; i++) {
        int* o = &currentObstacles[i * 2];
        if (o[0] == newR && o[1] == newC)
          valid = false;
      }

      if (valid) {
        apple.rPos = newR;
        apple.cPos = newC;
        appleMoveCooldown = 150;
      }
    }
  }

  if (appleMoveCooldown > 0) {
    appleMoveCooldown -= deltaTime;
  }
}
