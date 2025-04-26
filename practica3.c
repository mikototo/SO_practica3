#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#define PAIS1_ID 1
#define PAIS2_ID 2
#define CAPACIDAD_CARGA 1000
#define UMBRAL_MEDIO 750
#define UMBRAL_BAJO 450

/* Parámetros de entrada */
double lim_alto, lim_medio, lim_bajo;
int N;

/* Estado del ecosistema y métricas de extracción */
double recursos_actuales = CAPACIDAD_CARGA;
double extraccion_acum_p1 = 0;
double extraccion_acum_p2 = 0;

/* Tuberías de comunicación (pipe1: padre→hijos, pipe2: hijos→padre) */
int to_children[2];
int to_parent[2];

/* Sincronización por señal */
volatile sig_atomic_t sigusr1 = 0;

/* Utilidades */

void sigusr1_handler(int sig)
{
    (void)sig;      /* parámetro sin usar */
    sigusr1 = 1;    /* marca recepción de señal */
}


/* Devuelve el incremento de recursos para el año en curso y muestra   */
/* los mensajes asociados al evento de recuperación                     */
double calcular_recuperacion(double recursos)
{
    double PB; /* porcentaje base */
    if (recursos >= UMBRAL_MEDIO)
        PB = 5;
    else if (recursos >= UMBRAL_BAJO)
        PB = 20;
    else
        PB = 5;

    /* Determinar evento aleatorio */
    int r = rand() % 100; /* 0‒99 */
    double PV, Ptotal;

    if (r < 75)
    {
        printf("[Evento] Este año se han producido condiciones normales de recuperación\n");
        PV = 0;
    }
    else if (r < 90)
    {
        printf("[Evento] Este año se han producido condiciones adversas de recuperación\n");
        PV = -15;
    }
    else
    {
        printf("[Evento] Este año se han producido condiciones favorables de recuperación\n");
        PV = 10;
    }

    Ptotal = PB + PV;
    if (Ptotal < 0) Ptotal = 0;

    printf("[Evento] El porcentaje de recuperación es del %f%%\n", Ptotal);
    return recursos * Ptotal / 100.0;
}

/* Proceso del pais */
void child_process(int id)
{
    /* Cada hijo sólo necesita: leer de to_children[0] y escribir en to_parent[1] */
    close(to_children[1]); /* cierra extremo de escritura de pipe padre→hijos */
    close(to_parent[0]);   /* cierra extremo de lectura de pipe hijos→padre */

    /* Semilla distinta para cada hijo */
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    for (int año = 0; año < N; ++año)
    {
        /* Esperar señal SIGUSR1 (busy wait) */
        while (!sigusr1) {}
        sigusr1 = 0; /* reinicia la marca */

        /* 1) Leer límite de extracción fijado por el coordinador */
        double limite;
        if (read(to_children[0], &limite, sizeof(double)) != sizeof(double))
        {
            perror("[Hijo] Error al leer límite");
            exit(EXIT_FAILURE);
        }

        /* 2) Generar aleatoriamente la demanda dentro del límite */
        double demanda = ((double)rand() / RAND_MAX) * limite;

        /* 3) Enviar demanda al coordinador */
        if (write(to_parent[1], &demanda, sizeof(double)) != sizeof(double))
        {
            perror("[Hijo] Error al escribir demanda");
            exit(EXIT_FAILURE);
        }
    }

    /* Cerrar tuberías y finalizar */
    close(to_children[0]);
    close(to_parent[1]);
    exit(EXIT_SUCCESS);
}

/* Proceso Coordinador */
void coordinator(pid_t pid1, pid_t pid2)
{
    /* El coordinador sólo escribe en to_children[1] y lee de to_parent[0] */
    close(to_children[0]); /* extremo de lectura no usado */
    close(to_parent[1]);   /* extremo de escritura no usado */

    srand((unsigned)time(NULL)); /* semilla eventos recuperación */

    for (int año = 0; año < N; ++año)
    {
        printf("* AÑO %d\n", año + 1);
        printf("[Coordinador] Los recursos disponibles para el año en curso son %f\n", recursos_actuales);

        /* 1) Calcular límite de extracción */
        double limite;
        if (recursos_actuales >= UMBRAL_MEDIO)
            limite = lim_alto;
        else if (recursos_actuales >= UMBRAL_BAJO)
            limite = lim_medio;
        else
            limite = lim_bajo;
        printf("[Coordinador] El límite de extracción para el año en curso es %f\n", limite);

        /**************   PAÍS 1   **************/
        kill(pid1, SIGUSR1);
        if (write(to_children[1], &limite, sizeof(double)) != sizeof(double))
        {
            perror("[Coordinador] Error write límite (p1)");
            exit(EXIT_FAILURE);
        }

        double demanda1;
        if (read(to_parent[0], &demanda1, sizeof(double)) != sizeof(double))
        {
            perror("[Coordinador] Error read demanda (p1)");
            exit(EXIT_FAILURE);
        }

        printf("[Coordinador] El pais 1 solicita extraer %f\n", demanda1);
        double aprobada1 = demanda1 > recursos_actuales ? recursos_actuales : demanda1;
        recursos_actuales -= aprobada1;
        printf("[Coordinador] La solicitud de extracción del país 1 se ha aprobado\n");
        extraccion_acum_p1 += aprobada1;

        /**************   PAÍS 2   **************/
        kill(pid2, SIGUSR1);
        if (write(to_children[1], &limite, sizeof(double)) != sizeof(double))
        {
            perror("[Coordinador] Error write límite (p2)");
            exit(EXIT_FAILURE);
        }

        double demanda2;
        if (read(to_parent[0], &demanda2, sizeof(double)) != sizeof(double))
        {
            perror("[Coordinador] Error read demanda (p2)");
            exit(EXIT_FAILURE);
        }

        printf("[Coordinador] El pais 2 solicita extraer %f\n", demanda2);
        double aprobada2 = demanda2 > recursos_actuales ? recursos_actuales : demanda2;
        recursos_actuales -= aprobada2;
        printf("[Coordinador] La solicitud de extracción del país 2 se ha aprobado\n");
        extraccion_acum_p2 += aprobada2;

        /* 2) Recuperación de recursos */
        double incremento = calcular_recuperacion(recursos_actuales);
        recursos_actuales += incremento;
        if (recursos_actuales > CAPACIDAD_CARGA) recursos_actuales = CAPACIDAD_CARGA;
        printf("[Coordinador] Los recursos se recuperaron en %f unidades\n\n", incremento);
    }


    /*FIN DE LA SIMULACIÓN  */
    printf("La simulación ha finalizado.\n");
    printf("Recursos extraídos por país 1: %f\n", extraccion_acum_p1);
    printf("Recursos extraídos por país 2: %f\n", extraccion_acum_p2);
    printf("Total recursos extraídos: %f\n", extraccion_acum_p1 + extraccion_acum_p2);
    printf("Recursos disponibles: %f\n", recursos_actuales);

    close(to_children[1]);
    close(to_parent[0]);

    /* Esperar a que terminen los hijos */
    wait(NULL);
    wait(NULL);
}


int main(int argc, char *argv[])
{
      // Leer parámetros
  if (argc != 5)
  {
    printf("%s <lim_alto> <lim_medio> <lim_bajo> <N>\n", argv[0]);
    exit(1);
  }

  lim_alto = atof(argv[1]);
  lim_medio = atof(argv[2]);
  lim_bajo = atof(argv[3]);
  N = atoi(argv[4]);

    if (N < 1)
    {
        fprintf(stderr, "N no puede ser cero o negativo\n");
        exit(EXIT_FAILURE);
    }

    /* Crear tuberías */
    if (pipe(to_children) == -1 || pipe(to_parent) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* Registrar manejador de SIGUSR1 */
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /******************  CREACIÓN DE PROCESOS HIJO  ******************/
    pid_t pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid1 == 0) /* Primer hijo */
        child_process(1);

    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid2 == 0) /* Segundo hijo */
        child_process(2);

    /***********************  PROCESO PADRE  *************************/
    coordinator(pid1, pid2);

    return 0;
}

