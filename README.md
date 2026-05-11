# CONTROL DE CORRIENTE DE TENS MEDIANTE ESP32
Sistema de monitorización y seguridad para un estimulador eléctrico transcutáneo (TENS), basado en el ESP32-S3. El proyecto utiliza un motor de adquisición continua mediante DMA para garantizar la detección de picos de corriente sin sobrecargar la CPU.

## 🛠️ Arquitectura Técnica

* Hardware (Productor) : El ADC1 funciona en modo continuo, llenando buffers de 256 bytes vía DMA a una frecuencia de 20-40kHz.

* Sincronización: Una interrupción (ISR) envía una notificación de tarea (vTaskNotify) solo cuando el buffer está listo.

* Software (Consumidor): Una tarea dedicada de FreeRTOS (Prioridad 10) procesa los datos, calcula el valor de cresta (pico) y la media, y ejecuta la lógica de corte de seguridad en microsegundos.

## 🚀 Características Principales
* Procesamiento No Bloqueante: El uso de notificaciones permite que la CPU descanse mientras el hardware recolecta muestras.

* Calibración por eFuse: Integración con el esquema de calibración de Espressif para compensar la no linealidad del ADC del S3.

* Protección Crítica: Límite de seguridad de 50mA con respuesta inmediata para evitar quemaduras o fibrilación.

* Optimización FPU: Cálculos realizados en punto flotante simple (float32) aprovechando la unidad de punto flotante del LX7.

* Rango de Muestreo: Configurado a 20kHz (sobremuestreo para pulsos de TENS de 100-500µs).

* Resolución: 12 bits (ADC_BITWIDTH_12).

## 📁 Estructura del Proyecto
📦 ESP32-S3_TENS_Project

┣ 📂 lib
 
 ┃ ┣ 📂 current_monitor   : Lógica de seguridad y muestreo DMA (20kHz)
 
 ┃ ┗ 📂 hbridge_driver     : Control del Puente en H (4kHz PWM / 0.5 DC)
 
 ┣ 📂 src
 
 ┃ ┗ 📜 main.c             : Orquestador del sistema y gestión de tareas
 
 ┗ 📜 platformio.ini       : Configuración del entorno y dependencias



```mermaid
sequenceDiagram
    autonumber

    participant App as Cliente
    participant Gen as SessionGenerator
    participant Timer as Retry Timer
    participant SIP as RdSipCall
    participant JSON as sessions.json

    %% --- Startup ---
    App->>App: Constructor()
    App->>App: Create SEGE sessions (localUri)
    App->>Timer: start(5s)

    %% --- Configuration ---
    App->>App: trataNewConfig()
    App->>App: find()
    alt Radios found
        App->>Gen: start()
        loop numSipSessions times
            Gen-->>App: newUriReceived(remoteUri)
            App->>App: Assign remoteUri to next SEGE
        end
    else Invalid IPRN
        App->>JSON: Write ERROR: Wrong IP (all sessions)
        App->>App: status=error, retryPending=true
    end

    %% --- Retry loop ---
    loop every 5 seconds
        Timer-->>App: timeout()
        loop for each SessionInfo
            alt status == connected
                App->>App: skip
            else retryPending == true
                App->>App: skip
            else remoteUri empty
                App->>App: skip
            else eligible for retry
                App->>App: retryPending=true
                App->>SIP: newCall(localUri, remoteUri)
            end
        end
    end

    %% --- SIP call lifecycle ---
    SIP-->>App: statusChanged(CALLING / EARLY)
    App->>App: Resolve session by call pointer

    SIP-->>App: statusChanged(CONFIRMED)
    App->>App: status=connected
    App->>App: retryPending=false
    App->>JSON: Store connected=true

    %% --- Disconnect ---
    SIP-->>App: statusChanged(IDLE)
    App->>App: status=disconnected
    App->>App: destroy call
    App->>App: retryPending=false
    App->>JSON: Store connected=false

    %% --- Retry resumes ---
    Timer-->>App: timeout()
    App->>SIP: newCall(void)

    ```
 
