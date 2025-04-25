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

sigset_t mask, oldmask;

int sigusr1 = 0;

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
    printf("N no pot ser zero o negatiu!\n");
    exit(1);
  }

      // TO-BE-DONE

  return 0;
}
