#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// Función para parsear los argumentos de la línea de comandos
void parseArguments(int argc, char *argv[], char **nombre,
                    char **archivoSolicitudes, char **pipeCrecibe)
{
    if (argc != 7) // Verificar si se proporcionan todos los argumentos requeridos
    {
        fprintf(stderr, "Uso: %s -s nombre -a archivosolicitudes -p pipecrecibe\n",
                argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-s") == 0)
        {
            *nombre = argv[i + 1];
        }
        else if (strcmp(argv[i], "-a") == 0)
        {
            *archivoSolicitudes = argv[i + 1];
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            *pipeCrecibe = argv[i + 1];
        }
        else
        {
            fprintf(stderr, "Argumento desconocido: %s\n", argv[i]);
            exit(1);
        }
    }
}

// Función para recibir la respuesta del controlador
void receiveResponse(int pipeReceive, char *response)
{
    if (read(pipeReceive, response, BUFFER_SIZE) > 0)
    {
        printf("Respuesta recibida: %s\n", response);
    }
    else
    {
        perror("Error al leer respuesta del controlador");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    char *nombre, *archivoSolicitudes, *pipeCrecibe;
    parseArguments(argc, argv, &nombre, &archivoSolicitudes, &pipeCrecibe);

    // Abrir el pipe para enviar al controlador
    int pipeSend = open(pipeCrecibe, O_WRONLY);
    if (pipeSend == -1)
    {
        perror("Error al abrir el pipe de envío");
        return 1;
    }

    // Enviar el nombre al controlador
    ssize_t bytes_written = write(pipeSend, nombre, strlen(nombre) + 1);
    if (bytes_written < 0) {
        perror("Error al escribir en el pipe");
        close(pipeSend);
        return 1;
    }

    // Suponiendo que el pipe de respuesta se nombra agregando "_out" al nombre del
    // pipe de envío
    char pipeReceiveName[BUFFER_SIZE];
    snprintf(pipeReceiveName, BUFFER_SIZE, "%s_out", pipeCrecibe);
    mkfifo(pipeReceiveName, 0666); // Crear el pipe de respuesta
    int pipeReceive = open(pipeReceiveName, O_RDONLY);
    if (pipeReceive == -1)
    {
        perror("Error al abrir el pipe de recepción");
        close(pipeSend);
        return 1;
    }

    // Abrir el archivo de solicitudes
    FILE *file = fopen(archivoSolicitudes, "r");
    if (file == NULL)
    {
        perror("Error al abrir el archivo de solicitudes");
        close(pipeSend);
        close(pipeReceive);
        return 1;
    }

    char line[BUFFER_SIZE], response[BUFFER_SIZE];
    int currentSimulationHour =
        -1; // Marcador de la hora actual de simulación, debe ser establecido por la
            // primera respuesta del controlador

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = 0; // Eliminar el salto de línea al final de la cadena

        // Analizar la solicitud de reserva para obtener la hora solicitada
        int requestedHour;
        sscanf(line, "%*[^,],%d", &requestedHour);

        // Si el controlador ha establecido la hora actual de simulación, validar la
        // hora de reserva
        if (currentSimulationHour != -1 && requestedHour < currentSimulationHour)
        {
            printf("Solicitud rechazada: la hora de reserva es menor que la hora "
                   "actual de simulación.\n");
            continue; // Saltar esta reserva
        }

        // Enviar la solicitud al controlador
        ssize_t request_bytes_written = write(pipeSend, line, strlen(line) + 1);
        if (request_bytes_written < 0) {
            perror("Error al escribir en el pipe");
            close(pipeSend);
            close(pipeReceive);
            fclose(file);
            return 1;
        }

        // Recibir e imprimir la respuesta del controlador
        receiveResponse(pipeReceive, response);

        // Extraer la hora actual de simulación de la respuesta si no está establecida
        if (currentSimulationHour == -1)
        {
            sscanf(response, "%d", &currentSimulationHour);
        }

        sleep(2); // Esperar 2 segundos antes de la próxima solicitud
    }

    // Cerrar el archivo y los pipes
    fclose(file);
    close(pipeSend);
    close(pipeReceive);

    printf("Agente %s termina.\n", nombre);
    return 0;
}