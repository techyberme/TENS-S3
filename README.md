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


 
