**TP2 – Actividad 03 – Paso 02**

En el presente documento se abordarán los semáforos binarios y semáforos contadores de FreeRTOS. Se comenzará con la definición de cada uno de ellos, seguido de una explicación de cómo implementarlos y ejeplificaciones.

# Semáforos binarios

## Definición y funcionamiento
Un semáforo binario es un mecanismo de sincronización que tiene solo dos estados posibles: vacío o lleno (0 o 1 respectivamente). Se puede imaginar como una cola de mensajes que tiene capacidad para un solo elemento, pero donde el "dato" enviado no importa, solo importa si la cola tiene algo o está vacía.

Cuando una tarea necesita esperar a que ocurra un evento, intenta "tomar" (Take) el semáforo. Si el semáforo está "vacío" (0), la tarea entra en estado de bloqueo (Blocked) y no consume tiempo de CPU. Cuando el evento ocurre, otra tarea o una interrupción "da" (Give) el semáforo, poniéndolo en estado "lleno" (1). Esto desbloquea automáticamente a la tarea que estaba esperando, la cual "toma" el semáforo (volviéndolo a 0) y ejecuta su código.

## Implementación en FreeRTOS

### xSemaphoreCreateBinary()
Crea un semáforo binario y devuelve un handler por el cual el se puede referenciar el semáforo. La variable global `configSUPPORT_DYNAMIC_ALLOCATION` debe estar configurado en 1 en FreeRTOSConfig.h, o dejarlo indefinido (en cuyo caso se pondrá por defecto en 1), para esto la función de la API RTOS debe estar disponible.

Cada semáforo binario requiere una pequeña cantidad de RAM que se utiliza para mantener el estado del semáforo. Si un  semáforo binario se crea usando **xSemaphoreCreateBinary()** y luego la RAM necesaria se asigna automáticamente del heap de FreeRTOS. 

Los semáforos se crean en un estado 'vacío', lo que implica que el semáforo primero se debe 'dar' utilizando la función de la API **xSemaphoreGive()** antes de que pueda ser 'tomado' con la función **xSmaphoreTake()**. 

Los semáforos binarios se referencian mediante variables de tipo `SemaphoreHandle_t` y pueden ser utilizados en cualquier función de la API a nivel de tarea que tome un parámetro de ese tipo. Esta variable requieren incluir el archivo cabecera `semphr.h`. A diferencia de los mutex, los semáforos binarios sí pueden utilizarse en rutinas de servicio de interrupción (ISR).

**Valores de retorno:**

* **`NULL`** El semáforo no se pudo crear porque no había suficiente memoria _heap_ de FreeRTOS disponible.

* **Cualquier otro valor** El semáforo se creó con éxito. El valor devuelto es un manejador (_handle_) mediante el cual se puede hacer referencia al semáforo.

#### Ejemplo de implementación de xSemaphoreCreateBinary():
SemaphoreHandle_t xSemaphore;

void vATask( void * pvParameters )
{
    /* Intento de crear un semáforo binario */
    xSemaphore = xSemaphoreCreateBinary();

    if( xSemaphore == NULL )
    {
        /* No había suficiente heap de FreeRTOS para que el semáforo se creara exitosamente */
    }
    else
    {
        /* El semáforo se creó exitosamente y puede ser utilizado.
        Su hadle se almacena en la variable xSemaphore.
        Haciendo una llamada a xSemaphoreTake() acá, resultará en un fallo hasta que se haya dado el semáforo. */
    }
}

### xSemaphoreGive()
La macro que permite 'dar' o liberar un semáforo se implementa con `xSemaphoreGive( SemaphoreHandle_t xSemaphore );`
Este macro no se debe utilizar desde un ISR. Para este caso se debe utilizar la función `xSemaphoreGiveFromISR()` como alternativa.

El único parámetro de esta función es _xSemaphore_; el handler del semáforo que se quiere liberar. Este es el mismo hadler que se obtiene al crear el semáforo. 

Esta función devuelve:
* _pdTRUE_ si el semáforo fue liberado exitosamente.
* _pdFALSE_ si ocurrió un error.

#### Ejemplo de implementación de xSemaphoreGive():
SemaphoreHandle_t xSemaphore = NULL;

void vATask( void * pvParameters )
{
    // Crea el semáforo para proteger un recurso compartido. Como estamos usando
    xSemaphore = xSemaphoreCreateBinary();

    if( xSemaphore != NULL )
    {
        if( xSemaphoreGive( xSemaphore ) != pdTRUE )
        {
            // Esperaríamos que esta llamada fallara porque no podemos dar (liberar)
            // un semáforo sin antes "tomarlo" (obtenerlo)!
        }

        // Obtiene el semáforo - no se bloquea si el semáforo no está
        // disponible inmediatamente.
        if( xSemaphoreTake( xSemaphore, ( TickType_t ) 0 ) )
        {
            // Ahora tenemos el semáforo y podemos acceder al recurso compartido.
            // ...
            // Hemos terminado de acceder al recurso compartido, así que podemos liberar el
            // semáforo.
            if( xSemaphoreGive( xSemaphore ) != pdTRUE )
            {
                // No esperaríamos que esta llamada fallara porque debemos haber
                // obtenido el semáforo para llegar aquí.
            }
        }
    }
}

### xSemaphoreTake()
La macro que permite 'tomar' un semáforo se implementa con `xSemaphoreTake( SemaphoreHandle\_t xSemaphore, TickType\_t xTicksToWait);
`
Este macro no se debe utilizar desde un ISR. Para este caso se debe utilizar la función `xQueueReceiveFromISR()` como alternativa.

Los parámetros que recibe esta fución son:
* _xSemaphore_
  Un handler al semáforo que va a ser tomado. Este handler es el mismo que se obtiene al crear el semáforo.
* _xTicksToWait_
  Tiempo en ticks a esperar que el semáforo se libere. La macro `portTICK_PERIOD_MS` se puede utilizar para convertir los tick en tiempo en milisegundos.
  
La función devuelve:
* _pdTRUE_ si el semáforo se pudo devolver de manera satisfactoria.
* _pdFALSE_ si se expiró el tiempo de `xTicksToWait` para que el semáforo esté disponible.

#### Ejemplo de implementación de xSemaphoreTake():
/* Una tarea que usa el semáforo. */
void vAnotherTask( void * pvParameters )
{
    /* ... Hacer otras cosas. */

    if( xSemaphore != NULL )
    {
        /* Comprueba si podemos obtener el semáforo. Si el semáforo no está
           disponible, espera 10 ticks para ver si se libera. En este caso, xSemaphore es el Handle del semáforo con el que se quiere trabajar */
        if( xSemaphoreTake( xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            /* Pudimos obtener el semáforo y ahora podemos acceder al 
               recurso compartido. */

            /* ... */

            /* Hemos terminado de acceder al recurso compartido. Libera el
               semáforo. */
            xSemaphoreGive( xSemaphore );
        }
        else
        {
            /* No pudimos obtener el semáforo y, por lo tanto, no podemos acceder
               al recurso compartido de manera segura. */
        }
    }
}


# Semáforos contadores

## Definición y funcionamiento
Un semáforo contador es similar al binario, pero en lugar de tener solo dos estados, mantiene un valor numérico (un contador). Su capacidad no se limita a 1, sino a un valor máximo que se define al crearlo.
Cada vez que se "da" (Give) el semáforo, el contador interno incrementa en 1 (hasta llegar a su límite máximo). Cada vez que se "toma" (Take) el semáforo, el contador interno disminuye en 1. Si el contador está en `0`, cualquier tarea que intente "tomarlo" se bloqueará esperando a que alguien más lo "dé" e incremente el contador.

## Implementación en FreeRTOS

### xSemaphoreCreateCounting()
Crea un semáforo contador y devuelve un contador por el cual el se puede referenciar el semáforo. La variable global `configSUPPORT_DYNAMIC_ALLOCATION` debe estar configurado en 1 en FreeRTOSConfig.h, o dejarlo indefinido (en cuyo caso se pondrá por defecto en 1), para esto la función de la API RTOS debe estar disponible.

Al igual que los semáforos binarios, cada semáforo contador requiere una pequeña cantidad de RAM que se utiliza para mantener el estado del semáforo. Un semáforo contador se crea usando la función
`SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);` 

Los semáforos binarios se referencian mediante variables de tipo `SemaphoreHandle_t` y pueden ser utilizados en cualquier función de la API a nivel de tarea que tome un parámetro de ese tipo. Esta variable requieren incluir el archivo cabecera `semphr.h`. A diferencia de los mutex, los semáforos binarios sí pueden utilizarse en rutinas de servicio de interrupción (ISR).

La función `xSemaphoreCreateCounting` recibe los siguientes parámetros:
* **`uxMaxCount`**: El valor máximo de la cuenta que se puede alcanzar. Cuando el semáforo alcanza este valor, ya no puede ser "dado" (liberado).
* **`uxInitialCount`**: El valor de la cuenta asignado al semáforo en el momento de su creación.

Y devuelve:
* Si el semáforo se crea con éxito, se devuelve un **_handle_** al semáforo.
* Si el semáforo no se puede crear porque no se pudo asignar la memoria RAM necesaria para alojarlo, entonces se devuelve **`NULL`**.

El funcionamiento de los semáforos contadores es exáctamente igual la de los semáforos binarios; para tomarlos o darlos se deben utilizar las funciones de FreeRTOS `xSemaphoreGive()` y `xSemaphoreTake()` explicadas más arriba.

#### Ejemplo de implementación de xSemaphoreCreateCounting():
void vATask( void * pvParameters )
{
SemaphoreHandle_t xSemaphore;

    /* Crea un semáforo contador con un valor de cuenta máximo de 10 y un valor inicial de 0*/
    xSemaphore = xSemaphoreCreateCounting( 10, 0 );

    if( xSemaphore != NULL )
    {
        /* El semáforo se creó satisfactoriamente. */
    }
}

## Aplicación de gestión de recursos compartidos con semáforos
Se adaptó la interacción entre un LED y un botón para que la comunicación entre el task_btn y el task_LED se realice a través de semáforos.
Para ello se debió seguir los siguientes pasos:
1. Declarar como variable externa el handler del semáforo en app.h e inicializarlo en app.c
2. Crear el semáforo en app.c y agregarlo a la cola de registro.
3. En task_btn.c se reemplazó todas las zonas donde se empleó la función put_event_task_led, se reemplazó por un xSemaphoreGive. Además, por una cuestión de robustez del código se decidió inicializar el semáforo en xSemaphoreTake antes del bucle de la tarea del botón.
4. Por último, se modificó el evento del LED de BLINK a OFF (y viceversa) según si el semáforo está tommado o no y el flag del LED en task_led.c

   El resultado obtenido fue el esperado. Al presionar el botón, el LED parpadea y al dejar de presionarlo se apaga.
   Se adjunta una captura de pantalla de los mensajes impresos en la consola al momento de correr el código.

![Console screenshot.PNG](Screenshots\Console screenshot.PNG)

   