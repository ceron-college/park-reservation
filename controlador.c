#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define MAX_HOURS 24

// Variables globales para llevar un seguimiento de las estadísticas
int num_solicitudes_negadas = 0;
int num_solicitudes_aceptadas = 0;
int num_solicitudes_reprogramadas = 0;

typedef struct
{
    int capacidad_por_hora[MAX_HOURS];
} DisponibilidadParque;

typedef struct
{
    int hora_inicio;
    int hora_actual;
    int hora_fin;
    int segundos_por_hora;
    int capacidad_maxima;
    char pipe_nombre[100];
    DisponibilidadParque disponibilidad;
} EstadoParque;

typedef struct
{
    char familia[100];
    int hora;
    int num_personas;
} Reserva;

void inicializarEstado(EstadoParque *estado)
{
    estado->hora_inicio = 0;
    estado->hora_actual = 0;
    estado->hora_fin = 0;
    estado->segundos_por_hora = 0;
    estado->capacidad_maxima = 0;
    strcpy(estado->pipe_nombre, "aa");
}

void ProcesarReserva(EstadoParque *estado, Reserva *reserva, char *respuesta)
{
    // Verificar si la hora solicitada ya ha pasado
    if (reserva->hora < estado->hora_actual)
    {
        strcpy(respuesta, "Reserva negada por tarde.");
        num_solicitudes_negadas++;
    }
    // Verificar si la hora solicitada está más allá del tiempo de fin de simulación
    else if (reserva->hora >= estado->hora_fin)
    {
        strcpy(respuesta, "Reserva negada, debe volver otro día.");
        num_solicitudes_negadas++;
    }
    // Verificar si el número de personas supera la capacidad máxima del parque
    else if (reserva->num_personas > estado->capacidad_maxima)
    {
        strcpy(respuesta, "Reserva negada, capacidad máxima alcanzada.");
        num_solicitudes_negadas++;
    }
    // Si hay suficiente capacidad para la hora solicitada y la siguiente
    else if (estado->disponibilidad.capacidad_por_hora[reserva->hora] +
                 reserva->num_personas <=
             estado->capacidad_maxima &&
             estado->disponibilidad.capacidad_por_hora[reserva->hora + 1] +
                 reserva->num_personas <=
             estado->capacidad_maxima)
    {
        // Aceptar la reserva y actualizar la disponibilidad del parque
        estado->disponibilidad.capacidad_por_hora[reserva->hora] +=
            reserva->num_personas;
        estado->disponibilidad.capacidad_por_hora[reserva->hora + 1] +=
            reserva->num_personas;
        snprintf(respuesta, BUFFER_SIZE,
                 "Reserva ok para la familia %s a las %d con %d personas.",
                 reserva->familia, reserva->hora, reserva->num_personas);
        num_solicitudes_aceptadas++;
    }
    // Si no hay suficiente capacidad en la hora solicitada, intentar encontrar otra hora
    else
    {
        bool found_alternative = false;
        for (int i = estado->hora_actual; i < estado->hora_fin - 1; ++i)
        {
            if (estado->disponibilidad.capacidad_por_hora[i] +
                        reserva->num_personas <=
                    estado->capacidad_maxima &&
                estado->disponibilidad.capacidad_por_hora[i + 1] +
                        reserva->num_personas <=
                    estado->capacidad_maxima)
            {
                snprintf(respuesta, BUFFER_SIZE,
                         "Reserva garantizada para otras horas: %d.", i);
                estado->disponibilidad.capacidad_por_hora[i] +=
                    reserva->num_personas;
                estado->disponibilidad.capacidad_por_hora[i + 1] +=
                    reserva->num_personas;
                found_alternative = true;
                num_solicitudes_reprogramadas++;
                break;
            }
        }
        if (!found_alternative)
        {
            strcpy(respuesta, "Reserva negada, debe volver otro día.");
            num_solicitudes_negadas++;
        }
    }
    // Imprimir la solicitud y la respuesta para llevar un registro
    printf("Petición: Familia %s, Hora %d, Número de personas %d\n",
           reserva->familia, reserva->hora, reserva->num_personas);
    printf("Respuesta: %s\n", respuesta);
}

void *hiloAvanceTiempo(void *arg)
{
    EstadoParque *estado = (EstadoParque *)arg;

    while (estado->hora_actual < estado->hora_fin)
    {
        sleep(estado->segundos_por_hora);
        estado->hora_actual++;

        // TODO: Aquí iría la lógica para manejar las familias que salen y entran del parque
        printf("Hora actual de la simulación: %d\n", estado->hora_actual);
        // Imprimir información de las personas y familias que salen y entran, si es necesario
    }

    return NULL;
}

void *hiloConexionAgente(void *arg)
{
    EstadoParque *estado = (EstadoParque *)arg;
    int pipefd;

    mkfifo(estado->pipe_nombre, 0666);
    pipefd = open(estado->pipe_nombre, O_RDONLY);
    if (pipefd == -1)
    {
        perror("Error al abrir el pipe de comunicaciones");
        exit(1);
    }

    while (1)
    {
        char buffer[BUFFER_SIZE];
        char respuesta[BUFFER_SIZE];
        int num_bytes;

        num_bytes = read(pipefd, buffer, BUFFER_SIZE);
        if (num_bytes > 0)
        {
            buffer[num_bytes] = '\0';

            if (strstr(buffer, "nombre,"))
            {
                char agenteNombre[100];
                char agentePipe[100];
                sscanf(buffer, "nombre,%[^,],%s", agenteNombre, agentePipe);

                int agentePipeFd = open(agentePipe, O_WRONLY);
                if (agentePipeFd == -1)
                {
                    perror("Error al abrir el pipe del agente");
                    continue;
                }

                snprintf(respuesta, BUFFER_SIZE, "%d", estado->hora_actual);
                if (write(agentePipeFd, respuesta, strlen(respuesta) + 1) == -1)
                {
                    perror("Error al escribir en el pipe del agente");
                }

                close(agentePipeFd);
            }
            else
            {
                Reserva reserva;
                sscanf(buffer, "%[^,],%d,%d", reserva.familia, &reserva.hora,
                       &reserva.num_personas);

                ProcesarReserva(estado, &reserva, respuesta);

                int agentePipeFd = open(reserva.familia, O_WRONLY);
                if (agentePipeFd != -1)
                {
                    if (write(agentePipeFd, respuesta, strlen(respuesta) + 1) == -1)
                    {
                        perror("Error al escribir en el pipe del agente");
                    }
                    close(agentePipeFd);
                }
                else
                {
                    perror("Error al abrir el pipe del agente para enviar respuesta");
                }
            }
        }
    }

    close(pipefd);
    unlink(estado->pipe_nombre);
    return NULL;
}

void parseArguments(int argc, char *argv[], EstadoParque *estado)
{
    if (argc != 11)
    {
        fprintf(stderr,
                "Uso: %s -i horaInicio -f horaFinal -s segundosHora -t "
                "totalPersonas -p pipeRecibe\n",
                argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            estado->hora_inicio = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
            estado->hora_fin = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            estado->segundos_por_hora = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            estado->capacidad_maxima = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            strcpy(estado->pipe_nombre, argv[i + 1]);
        }
        else
        {
            fprintf(stderr, "Argumento desconocido: %s\n", argv[i]);
            exit(1);
        }
    }
}

void generarReporte(EstadoParque *estado)
{
    int peakHourCount = 0, offPeakHourCount = estado->capacidad_maxima;
    int peakHours[MAX_HOURS] = {0}, offPeakHours[MAX_HOURS] = {0};

    for (int i = estado->hora_inicio; i < estado->hora_fin; i++)
    {
        if (estado->disponibilidad.capacidad_por_hora[i] > peakHourCount)
        {
            peakHourCount = estado->disponibilidad.capacidad_por_hora[i];
        }
        if (estado->disponibilidad.capacidad_por_hora[i] < offPeakHourCount)
        {
            offPeakHourCount = estado->disponibilidad.capacidad_por_hora[i];
        }
    }

    printf("Reporte al finalizar la simulación:\n");
    printf("Horas pico (mayor número de personas): ");
    for (int i = estado->hora_inicio; i < estado->hora_fin; i++)
    {
        if (estado->disponibilidad.capacidad_por_hora[i] == peakHourCount)
        {
            printf("%d ", i);
        }
    }
    printf("\n");

    printf("Horas de menor afluencia (menor número de personas): ");
    for (int i = estado->hora_inicio; i < estado->hora_fin; i++)
    {
        if (estado->disponibilidad.capacidad_por_hora[i] == offPeakHourCount)
        {
            printf("%d ", i);
        }
    }
    printf("\n");

    printf("Número de solicitudes negadas: %d\n", num_solicitudes_negadas);
    printf("Número de solicitudes aceptadas: %d\n", num_solicitudes_aceptadas);
    printf("Número de solicitudes reprogramadas: %d\n",
           num_solicitudes_reprogramadas);
}

int main(int argc, char *argv[])
{
    EstadoParque estado;
    inicializarEstado(&estado);

    if (estado.hora_inicio < 7 || estado.hora_inicio > 18 ||
        estado.hora_fin <= estado.hora_inicio || estado.hora_fin > 19)
    {
        fprintf(stderr, "Horas de inicio y/o fin inválidas.\n");
        return 1;
    }

    estado.hora_actual = estado.hora_inicio;
    pthread_t hiloTiempo;
    if (pthread_create(&hiloTiempo, NULL, hiloAvanceTiempo, (void *)&estado) != 0)
    {
        perror("Error al crear el hilo de tiempo");
        return 1;
    }

    mkfifo(estado.pipe_nombre, 0666);
    int pipefd = open(estado.pipe_nombre, O_RDONLY);
    if (pipefd == -1)
    {
        perror("Error al abrir el pipe de comunicaciones");
        exit(1);
    }

    pthread_t hiloConexion;
    if (pthread_create(&hiloConexion, NULL, hiloConexionAgente, (void *)&estado) != 0)
    {
        perror("Error al crear el hilo de conexión de agentes");
        return 1;
    }

    generarReporte(&estado);

    close(pipefd);
    unlink(estado.pipe_nombre);

    pthread_join(hiloTiempo, NULL);
    pthread_join(hiloConexion, NULL);

    return 0;
}