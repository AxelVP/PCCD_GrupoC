#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <signal.h>

#include "zonaMemoria.h"

float max(float n1, float n2)
{

    return (n1 > n2) ? n1 : n2;
}

int min(int n1, int n2)
{

    return (n1 < n2) ? n1 : n2;
}

int main(int argc, char const *argv[])
{

    int i,
        j,
        error = 0,
        zonaMemoria,
        prioridad = atoi(argv[1]),
        numProceso = atoi (argv [3]);

    long timerLong;

    struct timeval timerInicio, timerSC, timerFin;

    FILE * ficheroError = fopen ("salidaError.txt", "a");
    
    key_t clave = ftok (PATH_CLAVES, atoi(argv[0]));
    zonaMem *memoria = NULL;
    datos mensaje;

    char archOut [50];
//    sprintf (archOut, "%s%s.txt", argv [2], argv [0]);
    sprintf (archOut, "%s.txt", argv [2]);
    FILE * ficheroOut = fopen (archOut, "a");

    FILE * ficheroStats = fopen (argv[4], "a");
    setbuf (ficheroStats, NULL);

//    sprintf (archOut, "%s%s.txt", argv [4], argv [0]);
    sprintf (archOut, "%s.txt", argv[4]);
    //FILE * ficheroStats = fopen (archOut, "a");

    struct timeval timer;

    zonaMemoria = shmget (clave, sizeof (zonaMem), 0);

    if (zonaMemoria == -1) {

        fprintf (ficheroError, "[Nodo %s, numProc %s] - Error Zona Memoria\n", argv [0], argv [3]);
        fclose (ficheroError);
        return 0;
    }

    memoria = shmat (zonaMemoria, NULL, 0);

    setbuf (ficheroOut, NULL);

    gettimeofday (&timerInicio, NULL);


    //sleep ((float) (rand () % 51) / 1000.0f);

    sem_wait (&memoria->mutexDatos);

    if (memoria->bloqueoConsultas == 0) {

        memoria->bloqueoConsultas = 1;
    }

    if (memoria->pasesSC >= memoria->maxPasesSC) {

        gettimeofday (&timer, NULL);
        timerLong = timer.tv_sec * (10 ^ 6) + timer.tv_usec;

        fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - max (sem = %p)\n", memoria->miId, prioridad, numProceso, &memoria->maxProcesos);

        memoria->esperandoMax++;
        sem_post (&memoria->mutexDatos);

        sem_wait (&memoria->maxProcesos);

        gettimeofday (&timer, NULL);
        timerLong = timer.tv_sec * (10 ^ 6) + timer.tv_usec;

        fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - saleMax t = %ld\n", memoria->miId, prioridad, numProceso, timerLong);
        sem_wait (&memoria->mutexDatos);
        memoria->esperandoMax--;

        if (memoria->esperandoMax > 0 && memoria->pasesSC < memoria->maxPasesSC) {

            gettimeofday (&timer, NULL);
            timerLong = timer.tv_sec * (10 ^ 6) + timer.tv_usec;

            sem_post (&memoria->maxProcesos);
            fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - Libera (sem = %p)\n", memoria->miId, prioridad, numProceso, &memoria->maxProcesos);
        }
    }

    gettimeofday (&timer, NULL);
    timerLong = timer.tv_sec * (10 ^ 6) + timer.tv_usec;

//    fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - pase t = %ld\n", memoria->miId, prioridad, numProceso, timerLong);

    memoria->esperandoSC++;
    memoria->pasesSC++;
    memoria->nProcesosPrio [prioridad - 1]++;


    if (memoria->enSC == 0 && (prioridad < memoria->prioridadNodo || memoria->prioridadNodo == 0)) {

        fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - Disputa\n", memoria->miId, prioridad, numProceso);

        memoria->prioridadNodo = prioridad;
        sem_post (&memoria->mutexDatos);

        srand ((unsigned) time (NULL));

        //fprintf(ficheroOut, "[N%i, pR%i]Esperando al semaforo mutex\n", memoria->miId, numProceso);
        sem_wait (&memoria->mutexDatos);
        //fprintf(ficheroOut, "P[N%i, pR%i]Saliendo del semaforo mutex\n", memoria->miId, numProceso);
        memoria->ticket = memoria->minTicket + (float) ((rand () % 1000) / 1000.0f);

        memoria->minTicket = max (memoria->minTicket, memoria->ticket);
        memoria->nConfirmaciones = 0;

        mensaje.mtype = 1;
        mensaje.idOrigen = memoria->miId;
        mensaje.ticket = memoria->ticket;
        mensaje.prioridad = prioridad;
        
        sem_post (&memoria->mutexDatos);

        for (i = 0; i < memoria->nNodos - 1; i++) {

            do {

                error = msgsnd (memoria->idNodos [i], &mensaje, sizeof (datos) - sizeof (long), 0);


            } while (error != 0);
           
            memoria->numMensajes++;
           
        }

    } else {

        sem_post (&memoria->mutexDatos);
    }

    sem_wait (&memoria->entradaSC [prioridad - 1]);


    sem_wait (&memoria->mutexDatos);
    memoria->prioridadExterna = memoria->prioridadNodo;

    memoria->enSC++;

    if (memoria->prioridadExterna != 0) {

        memoria->prioridadExterna = memoria->prioridadNodo;
    }

    sem_post (&memoria->mutexDatos);

    


//    fprintf (ficheroOut, "[N%i, Pr%i, nP%i] -      In t = %ld\n", memoria->miId, prioridad, numProceso, timerLong);
    gettimeofday (&timerSC, NULL);
    sleep ((float) (rand () % 51) / 1000.0f);
//    fprintf (ficheroOut, "[N%i, Pr%i, nP%i] -      Out t = %ld\n", memoria->miId, prioridad, numProceso, timerLong);

    sem_wait (&memoria->mutexDatos);

    memoria->esperandoSC--;
    memoria->enSC--;
    memoria->nProcesosPrio [prioridad - 1]--;

    if (memoria->pendientes == 0) {

        memoria->prioridadExterna = 0;
    }

    if (memoria->esperandoSC == 0) {

        memoria->prioridadNodo = 0;

        if (memoria->pendientes == 0 && memoria->esperandoMax != 0) {

            memoria->pasesSC = 0;
            sem_post (&memoria->maxProcesos);
            fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - Libera (sem = %p)\n", memoria->miId, prioridad, numProceso, &memoria->maxProcesos);

        } else if (memoria->pendientes == 0 && memoria->esperandoMax == 0) {

            memoria->pasesSC = 0;

            if (memoria->esperaConsultas > 0) {

                sem_post (&memoria->sem_p_consultas);
            }

            sem_post (&memoria->mutexDatos);

            mensaje.mtype = 3;
            mensaje.idOrigen = memoria->miId;
            mensaje.ticket = 0;
            mensaje.prioridad = 0;

            for (i = 0; i < (memoria->nNodos - 1); i++) {

                do {

                    error = msgsnd (memoria->idNodos [i], &mensaje, sizeof (datos) - sizeof (long), 0);

                } while (error != 0);
                
                memoria->numMensajes++;
                
            }

            sem_wait (&memoria->mutexDatos);
            memoria->bloqueoConsultas = 0;

        } else {

            mensaje.mtype = 2;
            mensaje.idOrigen = memoria->miId;

            for (i = 0; i < memoria->pendientes; i++) {

                mensaje.prioridad = memoria->colaNodos [i] [1];
                mensaje.ticket = memoria->ticketPendientes [i];

                do {

                    error = msgsnd (memoria->colaNodos [i] [0], &mensaje, sizeof (datos) - sizeof (long), 0);

                } while (error != 0);

                
                memoria->numMensajes++;
                
            }

            memoria->pendientes = 0;
            memoria->pasesSC = 0;
            sem_post (&memoria->maxProcesos);
            fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - Libera (sem = %p)\n", memoria->miId, prioridad, numProceso, &memoria->maxProcesos);
        }

    } else {
        
        for (i = 0; i < memoria->numPrios; i++) {

            if (memoria->nProcesosPrio[i] > 0) {

                memoria->prioridadNodo = i + 1;
                memoria->prioridadExterna = 10;

                for (j = 0; j < memoria->pendientes; j++) {

                    memoria->prioridadExterna = min (memoria->prioridadExterna, memoria->colaNodos [j] [1]);
                }

                if (memoria->prioridadExterna == 10) {

                    memoria->prioridadExterna = 0;
                }

                if (memoria->prioridadNodo > memoria->prioridadExterna && memoria->prioridadExterna != 0) {

                    mensaje.mtype = 2;
                    mensaje.idOrigen = memoria->miId;
                    mensaje.ticket = 0;

                    srand ((unsigned) time (NULL));
                    memoria->ticket = memoria->minTicket + (float) ((rand () % 1000) / 1000.0f);

                    memoria->minTicket = max (memoria->minTicket, memoria->ticket);

                    int colaNodosAux [memoria->pendientes] [2], pendientesAux = 0;
                    float ticketPendientesAux [memoria->pendientes];

                    for (j = 0; j < memoria->pendientes; j++) {

                        mensaje.prioridad = memoria->colaNodos [j] [1];
                        mensaje.ticket = memoria->ticketPendientes [j];

                        if (memoria->prioridadNodo > mensaje.prioridad ||
                            (memoria->prioridadNodo == mensaje.prioridad && (memoria->ticket > mensaje.ticket ||
                                                                             (memoria->ticket == mensaje.ticket && memoria->miId > memoria->colaNodos[j][0])))) {

                            do {

                                error = msgsnd (memoria->colaNodos [j] [0], &mensaje, sizeof (datos) - sizeof (long), 0);

                            } while (error != 0);

                            
                            memoria->numMensajes++;
                            

                        } else {

                            colaNodosAux [pendientesAux] [0] = memoria->colaNodos [j] [0];
                            colaNodosAux [pendientesAux] [1] = memoria->colaNodos [j] [1];
                            ticketPendientesAux [pendientesAux] = memoria->ticketPendientes [j];
                            pendientesAux++;
                        }
                    }

                    for (memoria->pendientes = 0; memoria->pendientes < pendientesAux; memoria->pendientes++) {

                        memoria->colaNodos [memoria->pendientes] [0] = colaNodosAux [memoria->pendientes] [0];
                        memoria->colaNodos [memoria->pendientes] [1] = colaNodosAux [memoria->pendientes] [1];
                        memoria->ticketPendientes [memoria->pendientes] = ticketPendientesAux [memoria->pendientes];
                    }

                    memoria->nConfirmaciones = 0;

                    mensaje.mtype = 1;
                    mensaje.idOrigen = memoria->miId;
                    mensaje.ticket = memoria->ticket;
                    mensaje.prioridad = memoria->prioridadNodo;

                    sem_post (&memoria->mutexDatos);

                    for (j = 0; j < memoria->nNodos - 1; j++) {

                        do {

                            error = msgsnd (memoria->idNodos[j], &mensaje, sizeof (datos) - sizeof (long), 0);

                        } while (error != 0);

                        
                        memoria->numMensajes++;
                        
                    }

                    sem_wait (&memoria->mutexDatos);
                    memoria->pasesSC = memoria->esperandoSC;

                    if (memoria->esperandoMax > 0 && memoria->pasesSC < memoria->maxPasesSC) {

                        sem_post (&memoria->maxProcesos);
                        fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - Libera (sem = %p)\n", memoria->miId, prioridad, numProceso, &memoria->maxProcesos);
                    }
                } else {

                    sem_post (&memoria->entradaSC[i]);
                }

                break;
            }
        
        }
    }
    sem_post (&memoria->mutexDatos);
    gettimeofday (&timerFin, NULL);

    /*if(prioridad==4){
        //fprintf(ficheroStats, "\nMantenimiento Tiempo antes de SC:");
        timerLong = timerFin.tv_sec * (10 ^ 6) + timerFin.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
        fprintf (ficheroStats, "%ld+",timerLong);
    }*/
    /*
    if(prioridad==2){
        //fprintf(ficheroStats, "\nAnulaciones Tiempo antes de SC:");
        timerLong = timerSC.tv_sec * (10 ^ 6) + timerSC.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
        fprintf (ficheroStats, "%ld+",timerLong);
    }*/

    /*if(prioridad==3){
        timerLong = timerSC.tv_sec * (10 ^ 6) + timerSC.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
        fprintf (ficheroStats, "%ld+",timerLong);
    }
*/
   /* if(prioridad==4){
        timerLong = timerSC.tv_sec * (10 ^ 6) + timerSC.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
        fprintf (ficheroStats, "%ld+",timerLong);
    }*/

    //timerLong = timerFin.tv_sec * (10 ^ 6) + timerFin.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
    //fprintf (ficheroStats, "[N%i, nP%i] - Tiempo total proceso: %ld\n", memoria->miId, numProceso, timerLong);

    fprintf (ficheroOut, "[N%i, Pr%i, nP%i] - END\n", memoria->miId, prioridad, numProceso);
    fclose (ficheroOut);
    fclose (ficheroStats);
    fclose (ficheroError);

    kill (getpid (), SIGCHLD);
    return 0;
}