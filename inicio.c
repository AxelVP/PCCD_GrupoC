#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "zonaMemoria.h"

int senal;

void signal_handler (int senalRecibida) {

    senal = senalRecibida;
}


int main (int argc, char const *argv[]) {
    
    int i, j, k, cont, numMuertos = 0, nProcConPrio = 0;
    int nNodosAux, maxPasesSCAux, numPriosAux;
    long timerLong;

    struct timeval timerInicio, timerFin;

    char entrada [50], numeros [3], archivoOut [50], archivoStats [50];

    struct sigaction sig;
    sig.sa_handler = signal_handler;
    sig.sa_flags = SA_RESTART;

    sigaction (SIGCHLD, &sig, NULL);

    FILE * ficheroIn = fopen (argv [1], "r");


    archivoOut [strlen (archivoOut) - 1] = '\0';

    if (ficheroIn == NULL) {

        printf ("Error: Fichero de entrada no encontrado. Cerrando ...\n\n");
        return 0;
    }

    fgets (archivoOut, 50, ficheroIn);
    fgets (archivoStats, 50, ficheroIn);

    FILE * ficheroOut = fopen (archivoStats, "w");
    setbuf (ficheroOut, NULL);

    // Esta primera parte lee los valores de variables hasta que se encuentra una linea solo con un \n

    fgets (entrada, 50, ficheroIn);
    
    do {

        char variable [15] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};

        for (i = 0, cont = 0; entrada [i] != '='; i++) {

            variable [i] = entrada [i];
            cont ++;
        }

        memcpy (numeros, &entrada [cont + 1], 3);
        i = atoi (numeros);

        if (strcmp (variable, "nNodos") == 0) {

            nNodosAux = i;
            printf ("nNodos = %i\n", nNodosAux);

        } else if (strcmp (variable, "maxPasesSC") == 0) {

            maxPasesSCAux = i;
            printf ("maxPasesSC = %i\n",maxPasesSCAux);

        } else if (strcmp (variable, "numPrios") == 0) {

            numPriosAux = i;
            printf ("numPrios = %i\n", numPriosAux);
        }

        fgets (entrada, 50, ficheroIn);

    } while (entrada [0] != '\n');


    // Esta segunda parte inicializa toda la zona de memoria compartida de todos los nodos, sus respectivas colas de mensajes y su receptor

    int procReceptor[nNodosAux], zonaMemoria[nNodosAux];
    zonaMem * memoria [nNodosAux];

    for (i = 0; i < nNodosAux; i++) {

        int error = 0;
        key_t clave = ftok (PATH_CLAVES, i);

        zonaMemoria[i] = shmget (clave, sizeof (zonaMem), IPC_CREAT | 0777);

        if (zonaMemoria [i] == -1) {

            perror ("Error al crear la zona de memoria compartida. Cerrando programa ...\n\n");
            return 0;
        }

        printf ("\nCreada zona de memoria %i\n", zonaMemoria [i]);
        memoria [i] = shmat (zonaMemoria [i], NULL, 0);

        memoria [i]->miId = msgget (clave, IPC_CREAT | 0777);

        if (memoria [i]->miId == -1)
        {

            printf ("Error al crear la cola de mensajes. Cerrando programa ...\n\n");
            return 0;
        }

        printf ("Creada la cola de mensajes con ID %i\n", memoria [i]->miId);

        error += sem_init (&memoria [i]->disputaSC, 1, 0);

        for (j = 0; j < memoria [i]->numPrios; j++) {

            error += sem_init (&memoria [i]->entradaSC [j], 1, 0);
        }

        error += sem_init (&memoria [i]->maxProcesos, 1, 0);
        error += sem_init (&memoria [i]->mutexDatos, 1, 1);
        error += sem_init (&memoria [i]->sem_p_consultas, 1, 0);
        error += sem_init (&memoria [i]->finConsultas, 1, 0);

        if (error != 0) {

            printf ("Error al inicializar el semaforo de fin de consultas. Cerrando programa ...\n\n");
            return 0;
        }

        printf ("Semaforos inicializados\n");

        memoria [i]->nNodos = nNodosAux;
        memoria [i]->maxPasesSC = maxPasesSCAux;
        memoria [i]->numPrios = numPriosAux;

        memoria [i]->minTicket = 0;

        memoria [i]->miId = i;
        memoria [i]->pendientes = 0;
        memoria [i]->esperandoSC = 0;
        memoria [i]->enSC = 0;
        memoria [i]->pasesSC = 0;
        memoria [i]->numMensajes = 0;
        memoria [i]->esperandoMax = 0;
        memoria [i]->prioridadNodo = 0;
        memoria [i]->nConfirmaciones = 0;
        memoria [i]->nProcesosPrio [0] = 0;
        memoria [i]->nProcesosPrio [1] = 0;
        memoria [i]->nProcesosPrio [2] = 0;
        memoria [i]->nProcesosPrio [3] = 0;
        memoria [i]->esperaConsultas = 0;
        memoria [i]->prioridadExterna = 0;
        memoria [i]->bloqueoConsultas = 0;
        memoria [i]->numConsultas = 0;

        printf ("Fichero de salida del nodo %i = %s\n", i, archivoOut);

        for (j = 0; j < memoria [i]->nNodos; j++) {

            if (j < i) {

                memoria [i]->idNodos [j] = j;

            } else if (j > i) {

                memoria [i]->idNodos [j - 1] = j;

            }
        }

        printf ("Nodo %i inicializado correctamente\n", i);

        procReceptor [i] = fork ();

        if (procReceptor [i] == 0) {

            do {

                char iAux [2];
                sprintf (iAux, "%i", i);
                execl ("receptor", iAux, archivoOut, archivoStats, (char *) NULL);

            } while (error != 0);
        }

        printf ("Receptor del nodo %i creado (PID = %i)\n\n", i, procReceptor [i]);
    }

    sleep (1);

    // En esta tercera parte se crean todos los procesos que van a ir a los diferentes nodos

    entrada [0] = '0';
    fgets (entrada, 50, ficheroIn);

    gettimeofday (&timerInicio, NULL);

    for (i = 0; i < nNodosAux && !feof (ficheroIn); i++) {

        printf ("Entrada = %s", entrada);
        char * nProcesosPrio = strtok (entrada, " ");

        char iAux [3];
        sprintf (iAux, "%i", i);

        for (j = 1; j <= numPriosAux; j++) {

            char jAux [2];
            sprintf (jAux, "%i", j);

            for (k = 0; k < atoi (nProcesosPrio); k++) {
                
                char kAux [4];
                sprintf (kAux, "%i", k);

                int procHijo = fork ();

                if (procHijo == 0) {

                    //fprintf (ficheroOut, "Proc %i, prio %i, nodo %i (%s - %s)\n", k, j, i, iAux, jAux);
                    execl ("proceso_general", iAux, jAux, archivoOut, kAux, archivoStats, (char *) 0);
                    return 0;
                }

                nProcConPrio ++;
            }

            nProcesosPrio = strtok (NULL, " ");
        }

        for (j = 0; j < atoi (nProcesosPrio); j++) {
            
            char jAux [4];
            sprintf (jAux, "%i", j);

            int procHijo = fork ();

            if (procHijo == 0) {

                execl ("consulta", iAux, archivoOut, jAux, archivoStats, (char *) 0);
                return 0;
            }

            nProcConPrio ++;
        }

        printf ("Creados procesos del nodo %i\n", i);
        fgets (entrada, 50, ficheroIn);
    }

    fclose (ficheroIn);

    while (numMuertos < nProcConPrio) {

        wait (NULL);

        if (senal == SIGCHLD) {

            numMuertos ++;
        }
    }

    gettimeofday (&timerFin, NULL);
    timerLong = timerFin.tv_sec * (10 ^ 6) + timerFin.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
    fprintf (ficheroOut, "Tiempo total de ejecución de procesos : %ld\nNum procesos totales : %i\n", timerLong, nProcConPrio);
    
    printf("Num procesos totales : %i\n", nProcConPrio);


    
    for(i=0; i<nNodosAux; i++){

        //memoria [i] = shmat (zonaMemoria [i], NULL, 0);
        fprintf(ficheroOut, "Numero de mensajes totales enviados del nodo %i = %i\n", i, memoria[i]->numMensajes);
    }
    
    fclose (ficheroOut);
    return 0;
}