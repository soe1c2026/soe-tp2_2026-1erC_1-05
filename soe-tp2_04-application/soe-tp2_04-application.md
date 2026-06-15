**TP 2 - Actividad 04 - Paso 02**

## Funciones de API como ISR

De acuerdo con el manual "Mastering the FreeRTOS Real Time Kernel", las funciones de la API que se pueden utilizar dentro de una rutina de servicio de interrupción (ISR) son exclusivamente aquellas diseñadas para ser "seguras para interrupciones" (Interrupt Safe API).

Las reglas y características que se deben tener en cuenta son:
* **Sufijo "FromISR"**: Todas las funciones de la API de FreeRTOS que están destinadas a ser utilizadas desde una ISR tienen el sufijo **"FromISR"** añadido al final de su nombre.
* **Regla estricta**: El manual indica explícitamente que **nunca** se debe llamar a una función de la API de FreeRTOS que no tenga "FromISR" en su nombre desde el interior de una ISR. Intentar hacerlo (por ejemplo, usar funciones que bloquean una tarea desde una interrupción) causará fallos en la aplicación.

El documento menciona múltiples ejemplos de este tipo de funciones para interactuar con los diferentes mecanismos del kernel dentro de una ISR. Algunos de ellos incluyen:

* **Manejo de Colas (Queues):** `xQueueSendToFrontFromISR()`, `xQueueSendToBackFromISR()`, `xQueueReceiveFromISR()`.
* **Semáforos:** `xSemaphoreGiveFromISR()`.
* **Notificaciones de Tareas (Task Notifications):** `vTaskNotifyGiveFromISR()`, `xTaskNotifyFromISR()`, `xTaskNotifyAndQueryFromISR()` (y sus versiones "Indexed").
* **Grupos de Eventos (Event Groups):** `xEventGroupSetBitsFromISR()`.
* **Temporizadores y Procesamiento Diferido:** `xTimerPendFunctionCallFromISR()`.
* **Secciones Críticas y Cambio de Contexto:** Las funciones `taskENTER_CRITICAL_FROM_ISR()` y `taskEXIT_CRITICAL_FROM_ISR()` para proteger secciones de código, así como las macros `portYIELD_FROM_ISR()` y `portEND_SWITCHING_ISR()` para solicitar cambios de contexto desde la ISR.

FreeRTOS implementa esta separación (una API para tareas y otra para interrupciones) para evitar que las funciones normales sean complejas y lentas, permitiendo así que el código dentro de las ISR se ejecute de la forma más rápida y eficiente posible.

## Métodos para delegar el procesamiento de interrupciones a una Tarea

La práctica recomendada es mantener las Rutinas de Servicio de Interrupción (ISR) lo más cortas posible y "diferir" o delegar cualquier procesamiento complejo, largo o no determinista a una tarea. A esta técnica se le conoce como **Procesamiento de Interrupción Diferido (Deferred Interrupt Processing)**.

El manual detalla cuatro métodos principales para implementar esta delegación:

#### 1. Semáforos Binarios (Binary Semaphores)

La tarea dedicada al procesamiento llama a `xSemaphoreTake()` para bloquearse indefinidamente esperando a que el semáforo esté disponible. Cuando ocurre la interrupción, la ISR ejecuta el mínimo trabajo necesario, limpia la interrupción de hardware y llama a `xSemaphoreGiveFromISR()`. Esto desbloquea a la tarea, permitiéndole ejecutar el resto del procesamiento.

Se utilizan para sincronizar una tarea específica con una interrupción.

#### 2. Semáforos Contadores (Counting Semaphores)

Si ocurre una segunda interrupción antes de que la tarea haya terminado de procesar la primera, un semáforo binario perdería el evento. Con un semáforo contador, cada vez que la ISR llama a `xSemaphoreGiveFromISR()`, el evento queda registrado (latched) incrementando el valor de conteo del semáforo. Esto asegura que la tarea procesará cada evento en orden sin que se pierdan solicitudes.

Son una extensión del método anterior, y son especialmente útiles cuando las interrupciones pueden ocurrir a una alta frecuencia.

#### 3. Notificaciones de Tareas (Task Notifications)

En lugar de usar un objeto intermedio como un semáforo, la ISR envía una notificación directamente a la tarea receptora. Para simular un semáforo desde la ISR, se utiliza la función `vTaskNotifyGiveFromISR()`. La tarea se bloquea esperando la notificación usando la función `ulTaskNotifyTake()`. También es posible enviar datos junto con el evento usando la función `xTaskNotifyFromISR()`.

Las notificaciones de tareas directas se presentan como una alternativa mucho más rápida y que consume menos memoria RAM en comparación con el uso de semáforos binarios o contadores.

#### 4. Procesamiento Diferido Centralizado (Deferring to the RTOS Daemon Task)

Los métodos anteriores requieren crear una tarea de FreeRTOS por cada interrupción a procesar. Para ahorrar recursos, FreeRTOS permite centralizar esto usando la Tarea Demonio (RTOS Daemon Task, también llamada tarea de servicio de temporizadores).

Dentro de la ISR, se utiliza la función `xTimerPendFunctionCallFromISR()`. Esto envía un puntero a una función de C convencional y sus parámetros a la cola de comandos del temporizador. La Tarea Demonio luego procesará ese comando y ejecutará la función delegada dentro de su propio contexto.

**Ventajas/Desventajas:** Reduce el uso de recursos y simplifica el código, pero se pierde flexibilidad ya que todas las interrupciones diferidas de este tipo se ejecutarán a la misma prioridad (la prioridad configurada para la Tarea Demonio) y están sujetas a los comandos que ya existían previamente en la cola.

**Nota importante:** En todos estos métodos, si la tarea que se despierta tiene una prioridad mayor que la tarea que estaba corriendo cuando ocurrió la interrupción, se debe solicitar un cambio de contexto (context switch) antes de salir de la ISR usando las macros `portYIELD_FROM_ISR()` o `portEND_SWITCHING_ISR()`. Esto garantiza que el procesador retorne inmediatamente a ejecutar la tarea encargada del procesamiento diferido.

### Uso de colas para transferir datos dentro y fuera de una rutina de servicio de interrupción

#### Enviar datos desde una ISR hacia una Tarea

Este es un escenario muy común donde un periférico de hardware (como un ADC, un puerto serie o un sensor) genera nuevos datos, dispara una interrupción, y la ISR envía esos datos a una tarea para que los procese.
* **Funciones a utilizar:** `xQueueSendToFrontFromISR()` o `xQueueSendToBackFromISR()`.
* **Funcionamiento:** La tarea receptora suele estar bloqueada esperando datos usando la función normal `xQueueReceive()`. Cuando ocurre la interrupción, la ISR lee el hardware, coloca el dato en la cola utilizando `xQueueSendToBackFromISR()` y limpia la bandera de la interrupción.
* **Consideración importante:** Nunca debes usar las funciones normales (como `xQueueSend`) dentro de la ISR. Además, las funciones `FromISR` **no permiten especificar un tiempo de bloqueo (Block Time)**. Si la cola está llena, la función retornará un error inmediatamente, ya que una ISR no puede entrar en estado de bloqueo.

#### Recibir datos en una ISR desde una Tarea

Aunque es menos común, las colas también se pueden usar para que una tarea alimente de datos a una interrupción. El ejemplo clásico es la transmisión por puerto serie (UART TX).

* **Función a utilizar:** `xQueueReceiveFromISR()`.
* **Funcionamiento:** Una tarea que quiere transmitir datos los coloca en una cola usando la función normal `xQueueSend()`. Luego, habilita la interrupción de transmisión del periférico. Cada vez que el periférico está listo para enviar un nuevo carácter, genera una interrupción. La ISR llama a `xQueueReceiveFromISR()` para sacar el siguiente carácter de la cola y lo envía al registro de hardware correspondiente. Si la cola se vacía, la ISR desactiva la interrupción de transmisión.

#### El Cambio de Contexto Obligatorio (`pxHigherPriorityTaskWoken`)

Tanto al enviar como al recibir desde una ISR, existe la posibilidad de que la acción desbloquee a una tarea que estaba esperando la cola. Si esa tarea recién desbloqueada tiene una prioridad **mayor** que la tarea que estaba en ejecución justo antes de que ocurriera la interrupción, se debe realizar un cambio de contexto para que, al salir de la ISR, el microcontrolador ejecute inmediatamente la tarea de alta prioridad.

Para manejar esto, todas las funciones de colas `FromISR` incluyen un parámetro llamado `pxHigherPriorityTaskWoken`:

1. Antes de llamar a la función de la cola dentro de la ISR, inicializar una variable en `pdFALSE` (por ejemplo: `BaseType_t xHigherPriorityTaskWoken = pdFALSE;`).
2. Pasar la dirección de esta variable como último parámetro a la función (ej. `&xHigherPriorityTaskWoken`).
3. Si la función de la cola desbloquea una tarea de mayor prioridad, FreeRTOS se cambiará automáticamente el valor de esa variable a `pdTRUE` internamente.
4. Al final de tu ISR, verificar esta variable y solicitar el cambio de contexto usando una macro específica del puerto (como `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)` o `portEND_SWITCHING_ISR(xHigherPriorityTaskWoken)`).

Si no se hace esto, la tarea de alta prioridad se quedará lista en la lista de preparados, pero el sistema volverá a la tarea original de menor prioridad hasta el siguiente "tick" del sistema, perdiendo la inmediatez característica de un sistema de tiempo real.

### Modelo de anidamiento de interrupciones disponible en algunas portaciones de FreeRTOS

En los puertos de la arquitectura que soportan el anidamiento de interrupciones, el modelo se rige principalmente por dos constantes de configuración que deben definirse en el archivo `FreeRTOSConfig.h`:

#### Las Constantes de Configuración

* **`configKERNEL_INTERRUPT_PRIORITY`**: Establece la prioridad del "tick interrupt" (la interrupción base del kernel). Esta constante debe configurarse siempre con **la prioridad de interrupción más baja posible**.
* **`configMAX_SYSCALL_INTERRUPT_PRIORITY`** (o `configMAX_API_CALL_INTERRUPT_PRIORITY` en puertos más nuevos): Establece la **prioridad máxima** desde la cual es seguro llamar a las funciones de la API de FreeRTOS diseñadas para interrupciones (las que terminan en `FromISR`).

#### Reglas del Modelo de Anidamiento

El uso de estas dos constantes divide las interrupciones de hardware en dos categorías o niveles distintos:

* **Interrupciones que usan la API del RTOS (Prioridad lógica menor o igual a `configMAX_SYSCALL_INTERRUPT_PRIORITY`):** Estas interrupciones tienen permitido interactuar con el kernel (por ejemplo, enviando datos a una cola o dando un semáforo). Al interactuar con el RTOS, estas interrupciones pueden ser bloqueadas o retrasadas temporalmente cuando el kernel entra en una "sección crítica".

* **Interrupciones que NO usan la API del RTOS (Prioridad lógica mayor a `configMAX_SYSCALL_INTERRUPT_PRIORITY`):** Estas interrupciones de muy alta prioridad **tienen estrictamente prohibido llamar a cualquier función de la API de FreeRTOS**. A cambio de esta restricción, el kernel de FreeRTOS nunca las retrasa ni las deshabilita, ni siquiera dentro de las secciones críticas del sistema operativo. Esto las hace ideales para operaciones de hardware con requisitos de tiempo real ultra-estrictos (como el control de motores de alta velocidad).

El manual resalta que este modelo es la fuente número uno de errores y solicitudes de soporte técnico debido a la confusión entre la prioridad lógica y la numérica, especialmente en arquitecturas como **ARM Cortex-M**.

En un Cortex-M, **los números más altos representan prioridades lógicas más bajas**. Por ejemplo, si un sistema configura `configMAX_SYSCALL_INTERRUPT_PRIORITY` con un valor numérico de **5**, significa que cualquier interrupción que quiera usar funciones de la API debe tener un valor numérico igual o mayor a 5 (por ejemplo, 5, 6, o 7). Si se le asigna a una interrupción el valor numérico de **3**, su prioridad lógica será _mayor_ que 5. Por lo tanto, si se llama a una función de la API desde esa interrupción de nivel 3, romperás el modelo de anidamiento, causando fallos intermitentes en la aplicación o sobrescrituras de memoria.









