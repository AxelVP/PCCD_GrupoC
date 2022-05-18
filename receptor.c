#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string.h>
#include <sys/time.h>

#include "zonaMemoria.h"


float max (float n1, float n2) {

    return (n1 > n2) ? n1 : n2;
}

int min (int n1, int n2) {

    return (n1 < n2) ? n1 : n2;
}



int main (int argc, char const *argv[]) {

    int i = 0,
        error = 0,
        zonaMemoria;

    long timerLong;

    zonaMem * memoria = NULL;
    key_t clave = ftok (PATH_CLAVES, atoi (argv[0]));
    datos mensajeIn, mensajeOut;

    struct timeval timerInicio, timerFin;

    zonaMemoria = shmget (clave, sizeof (zonaMem), 0);

    if (zonaMemoria == -1) {

        return 0;
    }

    char archOut [50];
    sprintf (archOut, "%sReceptor.txt", argv [1]);

    memoria = shmat (zonaMemoria, NULL, 0);

    FILE * ficheroOut = fopen (archOut, "a");
    setbuf (ficheroOut, NULL);

    

    if (ficheroOut == NULL) {

        return 0;
    }

    if (memoria->miId == -1) {

        return 0;
    }

    while (1) {

        do {
            error = msgrcv (memoria->miId, &mensajeIn, sizeof (datos) - sizeof (long), 0, 0);

        } while (error == -1);

        sem_wait(&memoria->mutexDatos);

        if (memoria->bloqueoConsultas == 0) {
            
            memoria->bloqueoConsultas = 1;

            if (memoria->numConsultas > 0) {

                memoria->esperaFinConsultas ++;
                
                sem_post(&memoria->mutexDatos);

                FILE * ficheroStats = fopen (argv[2], "a");
                setbuf (ficheroStats, NULL);
                gettimeofday (&timerInicio, NULL);
                sem_wait (&memoria->finConsultas);
                gettimeofday (&timerFin, NULL);
                timerLong = timerFin.tv_sec * (10 ^ 6) + timerFin.tv_usec - (timerInicio.tv_sec * (10 ^ 6) + timerInicio.tv_usec);
                fprintf (ficheroOut, "%ld+",timerLong);
                fclose(ficheroStats);

                sem_wait(&memoria->mutexDatos);
                memoria->esperaFinConsultas --;

            }
        }

        switch (mensajeIn.mtype) {

            case 1:

                if (memoria->prioridadNodo > mensajeIn.prioridad && memoria->enSC == 0) {

                    srand ((unsigned) time (NULL));
                    memoria->ticket = memoria->minTicket + (float) ((rand () % 1000) / 1000.0f);

                    memoria->minTicket = max (memoria->minTicket, memoria->ticket);
                    memoria->nConfirmaciones = 0;

                    mensajeOut.idOrigen = memoria->miId;
                    mensajeOut.mtype = 1;
                    mensajeOut.ticket = memoria->ticket;
                    mensajeOut.prioridad = memoria->prioridadNodo;

                    sem_post(&memoria->mutexDatos);

                    for (i = 0; i < memoria->nNodos - 1; i++) {

                        do {
                            error = msgsnd (memoria->idNodos [i], &mensajeOut, sizeof (datos) - sizeof (long), 0);

                        } while (error != 0);

                      
                        memoria->numMensajes++;
                       
                    }
                    sem_wait(&memoria->mutexDatos);
                }

                if (memoria->prioridadExterna == 0) {

                    memoria->prioridadExterna = mensajeIn.prioridad;

                } else {

                    memoria->prioridadExterna = min (memoria->prioridadExterna, mensajeIn.prioridad);
                }

                if (memoria->esperandoSC == 0 ||
                   (memoria->enSC == 0 && (memoria->prioridadNodo > mensajeIn.prioridad ||
                                          (memoria->prioridadNodo == mensajeIn.prioridad && (memoria->ticket > mensajeIn.ticket ||
                                                                                            (memoria->ticket == mensajeIn.ticket && memoria->miId > mensajeIn.idOrigen)))))) {
                
                    memoria->minTicket = max (memoria->minTicket, mensajeIn.ticket);

                    mensajeOut.mtype = 2;
                    mensajeOut.idOrigen = memoria->miId;
                    mensajeOut.ticket = mensajeIn.ticket;
                    mensajeOut.prioridad = mensajeIn.prioridad;

                    sem_post(&memoria->mutexDatos);

                    do {
                        error = msgsnd (mensajeIn.idOrigen, &mensajeOut, sizeof (datos) - sizeof (long), 0);

                    } while (error != 0);

                    
                    memoria->numMensajes++;
                    

                } else {

                    memoria->colaNodos [memoria->pendientes] [0] = mensajeIn.idOrigen;
                    memoria->colaNodos [memoria->pendientes] [1] = mensajeIn.prioridad;
                    memoria->ticketPendientes [memoria->pendientes] = mensajeIn.ticket;

                    memoria->pendientes += 1;
                    sem_post(&memoria->mutexDatos);

                }

                break;

            case 2:

                if (mensajeIn.prioridad == memoria->prioridadNodo && mensajeIn.ticket == memoria->ticket) {

                    memoria->nConfirmaciones ++;

                    if (memoria->nConfirmaciones == (memoria->nNodos - 1)) {

                        sem_post (&memoria->entradaSC [memoria->prioridadNodo - 1]);
                        memoria->nConfirmaciones = 0;
                    }
                }
                sem_post(&memoria->mutexDatos);

                break;

            case 3:

                if (memoria->esperandoSC == 0) {

                    memoria->bloqueoConsultas = 0;

                    if (memoria->esperaConsultas > 0) {

                        sem_post (&memoria->sem_p_consultas);
                    }
                }
                sem_post(&memoria->mutexDatos);

                break;
        }
    }
}