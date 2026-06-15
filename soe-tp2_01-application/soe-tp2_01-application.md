**TP2 – Actividad 01 – Paso 06**

A continuación, se presenta un análisis detallado y la explicación del funcionamiento del código fuente provisto en los archivos del proyecto, el cual corresponde a una aplicación para un microcontrolador STM32F103RB configurada con el sistema operativo de tiempo real **FreeRTOS** (a través de la capa de abstracción CMSIS-OS) y la biblioteca **STM32Cube HAL**.

### 1. Evolución de las variables `SysTick` y `SystemCoreClock`

Un punto crítico y fundamental que se debe notar al analizar este código es el siguiente: **el loop principal (`while (1)`) de `main.c` NUNCA se llega a ejecutar.** Esto se debe a que inmediatamente antes se invoca a `osKernelStart()`, función que transfiere el control total del procesador al planificador (_scheduler_) de FreeRTOS. El planificador toma el control de los hilos/tareas y nunca retorna a la función `main()`.

Sabiendo esto, la evolución de las variables desde el inicio hasta el arranque del sistema operativo es la siguiente:

#### Variable `SystemCoreClock` (Representa la frecuencia del reloj del núcleo en Hz):

* **Al iniciar (`Reset_Handler`):** El microcontrolador arranca utilizando su oscilador interno por defecto (en la familia STM32F1, el HSI de 8 MHz). Por lo tanto, la variable inicia con su valor de reset de fábrica o el configurado por el archivo de sistema base (típicamente `8000000`).

* **Durante `SystemClock_Config()`:** Se reconfiguran los registros de reloj para activar el multiplicador PLL. Al finalizar esta función, internamente la HAL ejecuta `SystemCoreClockUpdate()`, actualizando la variable al valor real calculado según los nuevos registros.

* **Antes de `osKernelStart()`:** La variable `SystemCoreClock` se estabiliza de forma definitiva en **`64000000`** (64 MHz), reflejando la configuración del reloj del sistema.

#### Variable de tiempo / `SysTick`:

* **Desde `Reset_Handler` hasta `osKernelStart()`:** El temporizador de hardware _SysTick_ permanece inactivo o no es utilizado por el sistema operativo.

* **La variable de tiempo de la HAL (`uwTick`):** En los proyectos generados con FreeRTOS, la HAL de ST cambia su base de tiempo de SysTick a un temporizador de hardware (en este caso **TIM1**, como se deduce del callback de interrupción). Desde que se ejecuta `HAL_Init()`, `uwTick` empieza a incrementarse de a 1 unidad cada 1 milisegundo mediante las interrupciones de TIM1.

* **Al ejecutar `osKernelStart()`:** El kernel de FreeRTOS configura y activa finalmente el periférico _SysTick_ hardware utilizando el valor de `FreeRTOSConfig.h` (`configTICK_RATE_HZ` = 1000). A partir de este instante, la variable interna del sistema operativo que cuenta los ticks de FreeRTOS (`xTickCount`) evoluciona incrementándose cada 1 ms, permitiendo la conmutación de tareas.

### 2. Comportamiento del programa desde `Reset_Handler` hasta antes de `osKernelStart()`

El flujo de ejecución secuencial del microcontrolador sigue el siguiente orden jerárquico y lógico:

1. **`Reset_Handler` (Archivo de arranque `.s`):** Es el punto de entrada físico tras un reset. Inicializa el puntero de pila (Stack Pointer), copia los valores de las variables inicializadas desde la memoria Flash a la memoria RAM (sección `.data`), llena con ceros la sección de variables no inicializadas (sección `.bss`), llama a `SystemInit()` para configuraciones eléctricas elementales y finalmente salta a la función `main()`.

2. **Entrada a `main()` e Inicialización de Monitor (`main.c`):** Si está activa la macro de Semihosting (`LOGGER_CONFIG_USE_SEMIHOSTING`), se inicializan las funciones del monitor para permitir la depuración de texto a través del debugger.

3. **`HAL_Init()`:** Inicializa la interfaz de la memoria Flash, configura la prioridad del grupo de interrupciones y arranca la base de tiempo interna de la HAL utilizando el **Timer 1 (TIM1)**, el cual generará una interrupción cada 1 ms para actualizar la variable global `uwTick`.

4. **`SystemClock_Config()`:** Modifica los relojes del sistema. Toma el oscilador interno rápido HSI (8 MHz), lo divide por 2 (4 MHz) y lo multiplica mediante el PLL por 16, logrando que el reloj del sistema (SYSCLK) y del núcleo (HCLK/AHB) sea de **64 MHz**. Configura el bus APB1 a 32 MHz (64 MHz / 2) y el bus APB2 a 64 MHz (64 MHz / 1).

5. **`MX_GPIO_Init()`:** Energiza los relojes de los puertos de entrada/salida (GPIOC, GPIOD, GPIOA, GPIOB). Configura el pin del usuario `B1_Pin` como una entrada de interrupción externa (EXTI) con prioridad 5, y el pin del LED integrado `LD2_Pin` como salida digital.

6. **`MX_USART2_UART_Init()`:** Configura el puerto serie USART2 a una velocidad de 115200 baudios, 8 bits de datos, 1 bit de parada y sin paridad para tareas de comunicación o logging.

7. **`MX_TIM2_Init()`:** Configura el **Timer 2 (TIM2)**. Dado que el bus APB1 tiene un divisor de 2, la frecuencia de reloj que alimenta a los timers de ese bus se multiplica automáticamente por 2, resultando en un reloj de entrada de 64 MHz para el TIM2. Con un Preescaler de $2-1=1$ (divide por 2 la entrada $\rightarrow$ 32 MHz) y un Periodo de $32000-1=31999$ (cuenta 32000 pulsos), el temporizador se configura para desbordar y generar una interrupción exactamente cada **1 milisegundo** ($32\text{ MHz} / 32000 = 1000\text{ Hz}$).

8. **Habilitación de Periféricos y Aplicación:** \* Se inicia el TIM2 en modo interrupción con `HAL_TIM_Base_Start_IT(&htim2)`.

   * Se ejecuta la inicialización de la lógica del usuario mediante `app_init()`.

   * Se define y crea el hilo inicial del sistema operativo llamado `defaultTask` asignándole la función `StartDefaultTask`.

9. **`osKernelStart()`:** Se invoca al planificador, configurando los vectores de interrupción del OS (SysTick, PendSV, SVC) y deteniendo el flujo secuencial de `main.c` para dar paso a la ejecución de las tareas de FreeRTOS por prioridades.

### 3. Interacción de `SysTick` y los Timers con FreeRTOS

En este proyecto, las interacciones con FreeRTOS están estrictamente delimitadas para evitar conflictos de recursos:

* **`SysTick` con FreeRTOS:**

  * **Cómo interactúa:** En `FreeRTOSConfig.h` se define `#define xPortSysTickHandler SysTick_Handler`. Esto mapea de forma directa la interrupción física del SysTick al manejador de ticks nativo del sistema operativo.

  * **Para qué sirve:** Es el **corazón o "latido" del sistema operativo** (OS Tick). Cada vez que el temporizador SysTick interrumpe (cada 1 ms, determinado por `configTICK_RATE_HZ`), FreeRTOS incrementa su contador de tiempo interno. Esto le permite verificar si el tiempo de bloqueo de alguna tarea ha expirado (por ejemplo, hilos esperando en un `osDelay()`) y evaluar si corresponde realizar un cambio de contexto (preemción) para cederle la CPU a otra tarea de igual o mayor prioridad.

* **Timer 2 (`TIM2`) con FreeRTOS:**

  * **Cómo interactúa:** En `FreeRTOSConfig.h` se encuentra habilitada la opción de estadísticas de consumo de CPU (`configGENERATE_RUN_TIME_STATS 1`). Las macros de configuración asocian la preparación de este contador a `configureTimerForRunTimeStats` y la lectura del valor actual a `getRunTimeCounterValue`. Estas funciones manipulan la variable `ulHighFrequencyTimerTicks`, la cual es incrementada de forma continua dentro de la rutina de interrupción del temporizador TIM2 administrada en `main.c`.

  * **Para qué sirve:** Sirve para proveer una métrica de tiempo de alta resolución para la **recolección de estadísticas de tiempo de ejecución de las tareas** (_Run-Time Stats_). FreeRTOS utiliza este contador independiente para medir con precisión cuántos ticks de reloj consume cada tarea de forma individual, permitiendo al desarrollador conocer el porcentaje exacto de uso de CPU de cada hilo para tareas de optimización y diagnóstico. _(Nota: Aunque típicamente este contador debería ser entre 10 y 100 veces más rápido que el tick del OS, en este código está configurado a la misma velocidad de 1 ms, pero conserva su propósito de profiling)._

### 4. Interacción del Timer 2 (`TIM2`) con la HAL de STM32

El **Timer 2** interactúa directamente con la capa HAL del proyecto bajo el modelo estándar de manejo de eventos e interrupciones de STMicroelectronics:

* **Cómo interactúa:** 1. El módulo de hardware de TIM2 es configurado mediante la estructura de control `htim2` dentro de la función HAL `MX_TIM2_Init()`.

  2\. Al habilitarse mediante `HAL_TIM_Base_Start_IT(&htim2)`, el periférico empieza a contar de forma autónoma.

  3\. Cada vez que el contador llega a su periodo límite (cada 1 ms), el hardware del microcontrolador dispara una solicitud de interrupción global hacia el procesador.

  4\. Esta solicitud es capturada en el archivo `stm32f1xx_it.c` por la función de interrupción nativa `TIM2_IRQHandler(void)`.

  5\. Dentro de esta, se invoca a la función despachadora de la biblioteca de ST: `HAL_TIM_IRQHandler(&htim2)`. Esta función de la HAL se encarga de bajo nivel de verificar qué bandera disparó el evento, limpiar los registros de interrupción del hardware para evitar bucles infinitos y, finalmente, llamar a la función callback débil (_weak callback_) llamada `HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)`.

  6\. En `main.c`, el desarrollador reescribe este callback. Al evaluar que la instancia que interrumpió es efectivamente `TIM2` (`if (htim->Instance == TIM2)`), se procede a incrementar la variable `ulHighFrequencyTimerTicks++`.

* **Para qué sirve:** Sirve para abstraer al programador de la manipulación directa de los registros de hardware del temporizador (como limpiar banderas de estado del flag de update `TIM_SR_UIF`). La HAL actúa como un puente seguro y ordenado que traduce un evento físico de desbordamiento de hardware en una ejecución controlada de software (el incremento de la variable de estadísticas), garantizando la portabilidad y el correcto manejo de las interrupciones del microcontrolador.

**TP2 – Actividad 01 – Paso 08**

Este conjunto de archivos implementa una aplicación de **Sistema Embebido Reactivo basado en Eventos (Event-Triggered System)** sobre el sistema operativo de tiempo real **FreeRTOS**. El objetivo principal del programa es controlar el estado de un LED (`LD2_Pin`) a partir de las pulsaciones de un botón de usuario (`B1_Pin`), implementando un mecanismo de filtrado de rebotes (debouncing) mediante máquinas de estado finito (Statecharts).

A continuación, se presenta un análisis detallado del funcionamiento de cada archivo y de la arquitectura global del sistema.

### 1. Análisis de `app.c` (Inicialización del Sistema)

Este archivo actúa como el módulo de configuración y arranque de la lógica de la aplicación antes de que el planificador (_scheduler_) de FreeRTOS tome el control total.

* **Variables Globales de Control:** Declara tres contadores globales (`g_app_tick_cnt`, `g_task_idle_cnt`, `g_app_stack_overflow_cnt`) que sirven para monitorear el rendimiento, el tiempo de ocio de la CPU y la salud de la memoria del sistema.

* **Función `app_init(void)`:** \* Inicializa los contadores en cero.

  * Envía mensajes de diagnóstico por el puerto serie usando el módulo `LOGGER_INFO`.

  * **Creación de Tareas:** Llama a la función de FreeRTOS `xTaskCreate` para instanciar dos hilos de ejecución independientes:

    1. `task_btn` (Nombre de depuración: `"Task BTN"`).

    2. `task_led` (Nombre de depuración: `"Task LED"`).

  * **Configuración de Prioridades:** Ambas tareas se crean con la **misma prioridad** (`tskIDLE_PRIORITY + 1ul`) y el mismo tamaño de pila (`2 * configMINIMAL_STACK_SIZE`). Al tener igual prioridad, FreeRTOS utilizará un esquema de tiempo compartido (_Time Slicing / Round-Robin_) para alternar la ejecución entre ellas cuando ambas estén listas.

  * **Validación de Recursos:** Utiliza `configASSERT(pdPASS == ret)` para detener inmediatamente el microcontrolador si la memoria RAM (Heap) es insuficiente para crear las tareas.

### 2. Análisis de `task_btn.c` (Detección del Botón y Debouncing)

Este archivo se encarga de leer el estado físico del botón de la placa y filtrar los ruidos eléctricos (rebotes) mediante software.

* **Estructura de Control:** Define la variable de estado `task_btn_dta` que almacena el evento actual, el estado de la máquina, los ticks de tiempo de referencia y los datos de hardware del periférico GPIO.

* **Lazo de la Tarea `task_btn`:** Se ejecuta de forma periódica dentro de un bucle infinito `for (;;)`. Al final de cada ciclo, la tarea se bloquea a sí misma de forma voluntaria durante **50 milisegundos** mediante `vTaskDelay(BTN_TICK_DEL_MAX)`. Esto libera la CPU para que otras tareas puedan ejecutarse.

* **Máquina de Estados (`task_btn_statechart`):**

  * **Lectura de Entrada:** En cada iteración lee el pin físico. Si el botón está presionado, genera el evento interno `EV_BTN_DOWN`; de lo contrario, genera `EV_BTN_UP`.

  * **Estados:**

    * `ST_BTN_UP` (Botón en reposo, suelto): Si detecta `EV_BTN_DOWN`, guarda el tiempo actual (`xTaskGetTickCount()`) y transiciona a `ST_BTN_FALLING` (Flanco de bajada).

    * `ST_BTN_FALLING` (Filtrando rebote de presión): Espera a que pasen al menos 50 ms (`DEL_BTN_MAX`). Si pasado ese tiempo el botón _sigue_ presionado, confirma que es una pulsación real, imprime un log, **envía el evento `EV_LED_BLINK` al LED** y pasa a `ST_BTN_DOWN`. Si el botón se soltó antes de los 50 ms, se asume que fue un rebote/ruido y regresa a `ST_BTN_UP`.

    * `ST_BTN_DOWN` (Botón presionado de forma estable): Si detecta que el botón se soltó (`EV_BTN_UP`), guarda el tiempo y pasa a `ST_BTN_RISING` (Flanco de subida).

    * `ST_BTN_RISING` (Filtrando rebote de liberación): Espera 50 ms. Si el botón se mantiene suelto de forma estable, imprime un log, **envía el evento `EV_LED_OFF` al LED** y regresa a `ST_BTN_UP`.

### 3. Análisis de `task_led.c` (Control de Modos del LED)

Este archivo maneja los efectos visuales del LED basándose en las órdenes que recibe desde la tarea del botón.

* **Lazo de la Tarea `task_led`:** A diferencia de la tarea del botón, esta utiliza **`vTaskDelayUntil`**. Este método garantiza una **periodicidad absoluta y exacta** de 50 ms, compensando el tiempo que el microcontrolador tarda en ejecutar las instrucciones de la propia tarea, lo cual es crítico para que los tiempos de parpadeo del LED no sufran desvíos (_drift_).

* **Máquina de Estados (`task_led_statechart`):**

  * `ST_LED_OFF` (LED apagado): El sistema permanece aquí de forma pasiva. En cuanto la bandera `task_led_dta.flag` se vuelve `true` y el evento recibido es `EV_LED_BLINK`, la tarea limpia la bandera, guarda el tiempo inicial, enciende físicamente el LED (`HAL_GPIO_WritePin`) y cambia al estado `ST_LED_BLINK`.

  * `ST_LED_BLINK` (LED parpadeando):

    * _Condición de salida:_ Si la bandera se activa con el evento `EV_LED_OFF`, limpia la bandera, apaga el LED y regresa a `ST_LED_OFF`.

    * _Acción interna:_ Si no hay eventos de apagado, evalúa constantemente el tiempo transcurrido. Cada vez que pasan 500 ms (`DEL_LED_MAX`), reinicia el contador de tiempo e invierte el estado físico del pin mediante `HAL_GPIO_TogglePin`, produciendo un parpadeo simétrico (500 ms encendido, 500 ms apagado).

### 4. Análisis de `task_led_interface.c` (Comunicación entre Tareas)

Este archivo implementa el canal de comunicación asíncrono entre hilos de ejecución.

* **Función `put_event_task_led(task_led_ev_t event)`:** Es una función de interfaz expuesta públicamente. Cuando la tarea del botón (`task_btn.c`) confirma un cambio de estado válido en el hardware, invoca a esta función pasando como argumento el nuevo comando (`EV_LED_BLINK` o `EV_LED_OFF`).

* La función realiza una escritura directa sobre la estructura de datos interna del LED (`task_led_dta.event = event`) y establece la bandera de aviso en verdadero (`task_led_dta.flag = true`).

_(Nota de arquitectura: Al no utilizar primitivas nativas de FreeRTOS como colas o semáforos para proteger esta estructura, el sistema se apoya en que la asignación de estas variables es atómica en la arquitectura ARM Cortex-M, evitando condiciones de carrera básicas)._

### 5. Análisis de `freertos.c` (Funciones Hook / Callback de Monitoreo)

Este archivo contiene las funciones "Hook", las cuales son devoluciones de llamada que el Kernel de FreeRTOS invoca automáticamente cuando ocurren eventos específicos en el sistema operativo:

1. **`vApplicationIdleHook(void)`:** Se ejecuta repetidamente dentro de la tarea `Idle` (la tarea de menor prioridad de FreeRTOS, que solo corre cuando ninguna otra tarea del usuario necesita CPU). Cada vez que entra aquí, incrementa el contador `g_task_idle_cnt`. Sirve para medir indirectamente cuánta CPU libre o "desocupada" le queda al sistema.

2. **`vApplicationTickHook(void)`:** Se ejecuta dentro de la Interrupción del Reloj del Sistema (_RTOS Tick ISR_) exactamente cada 1 milisegundo. Incrementa de forma matemática la variable `g_app_tick_cnt`, funcionando como un reloj de precisión global para la capa de la aplicación.

3. **`vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)`:** Es una rutina de seguridad crítica. Si alguna de las tareas se excede del tamaño de memoria RAM asignado en su pila (Stack), el kernel lo detecta e invoca esta función. El código detiene las interrupciones del procesador mediante `taskENTER_CRITICAL()` y entra en un bucle infinito mediante `configASSERT( 0 )`. Esto congela el sistema para que el desarrollador pueda conectar un depurador de hardware e identificar qué tarea corrompió la memoria.

### Resumen del Comportamiento Dinámico del Sistema

Cuando el sistema inicia, ambas tareas corren en ciclos de 50 ms de manera alternada.

1. **Estado Inicial:** El LED está apagado y el botón en reposo. La CPU pasa la mayor parte del tiempo en la tarea `Idle`, incrementando `g_task_idle_cnt`.

2. **Acción de Presionar:** Al presionar el botón, `task_btn` detecta la señal, espera 50 ms para comprobar que no sea ruido eléctrico y escribe de forma segura en la interfaz del LED el comando de parpadeo.

3. **Respuesta del LED:** En el siguiente ciclo de 50 ms, `task_led` lee su estructura, descubre la bandera en `true`, cambia a su modo de parpadeo y empieza a conmutar el pin del LED cada 500 ms de forma matemática exacta.

4. **Acción de Soltar:** Al liberar el botón, `task_btn` vuelve a filtrar los rebotes de apertura por 50 ms y envía la señal de apagado, provocando que `task_led` limpie sus variables y apague el LED físicamente, retornando al reposo.




