/*
 * ============================================================================
 * Proyecto: Sistema de Control Inalámbrico y Seguridad Activa para Móvil Robótico
 * Microcontrolador: ESP32 Dev Module
 * Actuadores: L298N (2 Motores DC), Buzzer pasivo/activo, LEDs direccionales
 * Sensores: Ultrasónico HC-SR04
 * Comunicación: Servidor Web embebido en modo Access Point (AP)
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>

// --- Credenciales del Punto de Acceso (Access Point) ---
const char* ssid_AP = "Carrito_ESP32";
const char* pass_AP = "12345678";

WebServer server(80);

// --- Asignación de Pines GPIO (Ajustables según montaje físico) ---
// Puente H L298N
const int PIN_IN1 = 26; // Motor Izquierdo Adelante
const int PIN_IN2 = 27; // Motor Izquierdo Atrás
const int PIN_IN3 = 14; // Motor Derecho Adelante
const int PIN_IN4 = 12; // Motor Derecho Atrás

// Sensor Ultrasónico HC-SR04
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;

// Alerta Acústica (Buzzer)
const int PIN_BUZZER = 19;

// Indicadores Luminosos LED
const int LED_ADELANTE  = 21;
const int LED_ATRAS     = 22;
const int LED_IZQUIERDA = 23;
const int LED_DERECHA   = 4;

// --- Variables de Estado y Temporización ---
enum EstadoMovimiento { DETENIDO, ADELANTE, ATRAS, IZQUIERDA, DERECHA };
EstadoMovimiento estadoActual = DETENIDO;

long duracionPulso = 0;
float distanciaActual = 100.0; // Distancia en centímetros

unsigned long ultimaMedicion = 0;
const unsigned long intervaloMedicion = 60; // Muestreo cada 60ms

unsigned long ultimoBuzzer = 0;
bool estadoBuzzer = false;

// --- Funciones de Control Motriz ---
void apagarLEDs() {
  digitalWrite(LED_ADELANTE, LOW);
  digitalWrite(LED_ATRAS, LOW);
  digitalWrite(LED_IZQUIERDA, LOW);
  digitalWrite(LED_DERECHA, LOW);
}

void detener() {
  estadoActual = DETENIDO;
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  apagarLEDs();
}

void moverAdelante() {
  // Verificación de seguridad: No avanzar si la distancia crítica está activa
  if (distanciaActual <= 10.0 && distanciaActual > 0.0) {
    detener();
    return;
  }
  estadoActual = ADELANTE;
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  
  apagarLEDs();
  digitalWrite(LED_ADELANTE, HIGH);
}

void moverAtras() {
  estadoActual = ATRAS;
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  
  apagarLEDs();
  digitalWrite(LED_ATRAS, HIGH);
}

void girarIzquierda() {
  estadoActual = IZQUIERDA;
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  
  apagarLEDs();
  digitalWrite(LED_IZQUIERDA, HIGH);
}

void girarDerecha() {
  estadoActual = DERECHA;
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  
  apagarLEDs();
  digitalWrite(LED_DERECHA, HIGH);
}

// --- Medición Ultrasónica ---
void medirDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Timeout de 20ms (~3.4 metros máximo para no bloquear el loop)
  duracionPulso = pulseIn(PIN_ECHO, HIGH, 20000);

  if (duracionPulso == 0) {
    distanciaActual = 150.0; // Fuera de rango o sin eco
  } else {
    distanciaActual = (duracionPulso * 0.0343) / 2.0;
  }
}

// --- Lógica de Alertas Acústicas y Frenado Autónomo ---
void gestionarSeguridad() {
  unsigned long tiempoActual = millis();

  // Bloqueo activo de avance
  if (distanciaActual <= 10.0 && distanciaActual > 0.0) {
    if (estadoActual == ADELANTE) {
      detener();
    }
  }

  // Modulación del zumbador
  if (distanciaActual <= 10.0 && distanciaActual > 0.0) {
    // Continuo
    digitalWrite(PIN_BUZZER, HIGH);
  } 
  else if (distanciaActual <= 20.0 && distanciaActual > 10.0) {
    // Intermitencia de alta frecuencia (100ms)
    if (tiempoActual - ultimoBuzzer >= 100) {
      ultimoBuzzer = tiempoActual;
      estadoBuzzer = !estadoBuzzer;
      digitalWrite(PIN_BUZZER, estadoBuzzer ? HIGH : LOW);
    }
  } 
  else if (distanciaActual <= 30.0 && distanciaActual > 20.0) {
    // Intermitencia de baja frecuencia (250ms)
    if (tiempoActual - ultimoBuzzer >= 250) {
      ultimoBuzzer = tiempoActual;
      estadoBuzzer = !estadoBuzzer;
      digitalWrite(PIN_BUZZER, estadoBuzzer ? HIGH : LOW);
    }
  } 
  else {
    digitalWrite(PIN_BUZZER, LOW);
  }
}

// --- Interfaz Web HTML/CSS embebida ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Telemetria Movil ESP32</title>
  <style>
    body { font-family: -apple-system, sans-serif; text-align: center; background: #121212; color: #fff; margin: 0; padding: 20px; }
    h2 { margin-bottom: 20px; font-weight: 400; }
    .grid { display: grid; grid-template-columns: repeat(3, 80px); grid-gap: 15px; justify-content: center; margin: 30px auto; }
    button {
      width: 80px; height: 80px; font-size: 22px; font-weight: bold;
      border: none; border-radius: 12px; background: #2a2a2a; color: #fff;
      box-shadow: 0 4px #1a1a1a; cursor: pointer; user-select: none;
    }
    button:active { background: #00adb5; transform: translateY(4px); box-shadow: none; }
    .stop-btn { background: #d32f2f; }
    .stop-btn:active { background: #b71c1c; }
  </style>
</head>
<body>
  <h2>Panel de Conducción</h2>
  <div class="grid">
    <div></div>
    <button onclick="enviar('adelante')">▲</button>
    <div></div>
    <button onclick="enviar('izquierda')">◄</button>
    <button class="stop-btn" onclick="enviar('detener')">■</button>
    <button onclick="enviar('derecha')">►</button>
    <div></div>
    <button onclick="enviar('atras')">▼</button>
    <div></div>
  </div>
  <script>
    function enviar(accion) {
      fetch('/' + accion, { method: 'POST' });
    }
  </script>
</body>
</html>
)rawliteral";

// --- Rutas del Servidor HTTP ---
void setupRutas() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/adelante", HTTP_POST, []() {
    moverAdelante();
    server.send(200, "text/plain", "OK");
  });

  server.on("/atras", HTTP_POST, []() {
    moverAtras();
    server.send(200, "text/plain", "OK");
  });

  server.on("/izquierda", HTTP_POST, []() {
    girarIzquierda();
    server.send(200, "text/plain", "OK");
  });

  server.on("/derecha", HTTP_POST, []() {
    girarDerecha();
    server.send(200, "text/plain", "OK");
  });

  server.on("/detener", HTTP_POST, []() {
    detener();
    server.send(200, "text/plain", "OK");
  });
}

// --- Configuración Inicial ---
void setup() {
  Serial.begin(115200);

  // Configuración de I/O de potencia
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // Configuración de I/O del sensor ultrasónico
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // Configuración de señalización acústica y luminosa
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(LED_ADELANTE, OUTPUT);
  pinMode(LED_ATRAS, OUTPUT);
  pinMode(LED_IZQUIERDA, OUTPUT);
  pinMode(LED_DERECHA, OUTPUT);

  detener();

  // Iniciar Access Point
  WiFi.softAP(ssid_AP, pass_AP);
  Serial.println("Punto de acceso activado");
  Serial.print("Dirección IP asignada: ");
  Serial.println(WiFi.softAPIP()); // Por defecto: 192.168.4.1

  setupRutas();
  server.begin();
}

// --- Bucle Principal de Control ---
void loop() {
  server.handleClient();

  // Muestreo del sensor cada 60ms
  if (millis() - ultimaMedicion >= intervaloMedicion) {
    ultimaMedicion = millis();
    medirDistancia();
  }

  gestionarSeguridad();
}