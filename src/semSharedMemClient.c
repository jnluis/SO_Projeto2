/**
 *  \file semSharedMemClient.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the clients:
 *     \li waitFriends
 *     \li orderFood
 *     \li waitFood
 *     \li travel
 *     \li eat
 *     \li waitAndPay
 *
 *  \author Nuno Lau - December 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"

/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static bool waitFriends (int id);
static void orderFood (int id);
static void waitFood (int id);
static void travel (int id);
static void eat (int id);
static void waitAndPay (int id);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the client.
 */
int main (int argc, char *argv[])
{
    int key;                                         /*access key to shared memory and semaphore set */
    char *tinp;                                                    /* numerical parameters test flag */
    int n;

    /* validation of command line parameters */
    if (argc != 5) { 
        freopen ("error_CT", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
       freopen (argv[4], "w", stderr);
       setbuf(stderr,NULL);
    }

    n = (unsigned int) strtol (argv[1], &tinp, 0);
    if ((*tinp != '\0') || (n >= TABLESIZE)) { 
        fprintf (stderr, "Client process identification is wrong!\n");
        return EXIT_FAILURE;
    }
    strcpy (nFic, argv[2]);
    key = (unsigned int) strtol (argv[3], &tinp, 0);
    if (*tinp != '\0') { 
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */
    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

    /* initialize random generator */
    srandom ((unsigned int) getpid ());                                                 


    /* simulation of the life cycle of the client */
    travel(n);
    bool first = waitFriends(n);
    if (first) orderFood(n);
    waitFood(n);
    eat(n);
    waitAndPay(n);

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief client goes to restaurant
 *
 *  The client takes his time to get to restaurant.
 *
 *  \param id client id
 */
static void travel (int id)
{
    usleep((unsigned int) floor ((1000000 * random ()) / RAND_MAX + 1000));
}

/**
 *  \brief client eats
 *
 *  The client takes his time to eat a pleasant dinner.
 *
 *  \param id client id
 */
static void eat (int id)
{
    usleep((unsigned int) floor ((MAXEAT * random ()) / RAND_MAX + 1000));
}

/**
 *  \brief client waits until table is complete 
 *
 *  Client should udpate state, first and last clients should register their values in shared data,
 *  last client should, in addition, inform the others that the table is complete.
 *  Client must wait in this function until the table is complete.
 *  The internal state should be saved.
 *
 *  \param id client id
 *
 *  \return true if first client, false otherwise
 */
static bool waitFriends(int id)
{
    bool first = false;

    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* insert your code here */
    sh->fSt.tableClients++; // número de clientes soma + 1
    sh->fSt.st.clientStat[id] = WAIT_FOR_FRIENDS; // O estado é atualizado

    if(sh->fSt.tableClients==1){ // se for o primeiro cliente a chegar
        first=true;
        sh->fSt.tableFirst=id;
    }
    else if(sh->fSt.tableClients==TABLESIZE){ // se for o último cliente a chegar
        sh->fSt.tableLast=id; // já guarda o id do último
        sh->fSt.st.clientStat[id] = WAIT_FOR_FOOD; // o último cliente já pode esperar pelo pedido. Passa do estado 2 para o 4
        for(int i=1;i<=TABLESIZE;i++){
            if (semUp (semgid, sh->friendsArrived) == -1)         /* unlocks friends (the extra up is so they themselves don't block on the down) */
            { // este if é para verificar se está bem, mas não dá erro se tirar
                perror ("error on the up operation for semaphore access (CT)");
                exit (EXIT_FAILURE);
            }       
        }
    }

    saveState (nFic, &(sh->fSt)); // Mete-se o & porque é um ponteiro, senão dá erro de FULL_STAT ser diferente de FULL_STAT*
    
    if (semUp (semgid, sh->mutex) == -1)                                                      /* exit critical region */
    { perror ("error on the up operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* insert your code here */
    // FAZER SEMPRE o semDown fora da região crítica
    if (semDown (semgid, sh->friendsArrived) == -1) {                                                  /* wait for friends */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    return first;
}

/**
 *  \brief first client orders food.
 *
 *  This function is used only by the first client.
 *  The first client should update its state, request food to the waiter and 
 *  wait for the waiter to receive the request.
 *  
 *  The internal state should be saved.
 *
 *  \param id client id
 */
static void orderFood (int id)
{
    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    sh->fSt.st.clientStat[id] = FOOD_REQUEST; // O estado é atualizado para o estado 3
    sh->fSt.foodRequest=1;
    // O pedido é feito ao waiter
    if (semUp (semgid, sh->waiterRequest) == -1)      {    // dar up porque ele aqui está a esperar
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    saveState (nFic, &(sh->fSt));
    
    if (semUp (semgid, sh->mutex) == -1)                                                      /* exit critical region */
    { perror ("error on the up operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* insert your code here */
    if (semDown (semgid, sh->requestReceived) == -1) {                                          
       perror ("error on the down operation for semaphore access (CT)");
       exit (EXIT_FAILURE);
    } 
}

/**
 *  \brief client waits for food.
 *
 *  The client updates its state, and waits until food arrives. 
 *  It should also update state after food arrives.
 *  The internal state should be saved twice.
 *
 *  \param id client id
 */
static void waitFood (int id)
{
    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    sh->fSt.st.clientStat[id] = WAIT_FOR_FOOD; // O estado é atualizado 4
    saveState (nFic, &(sh->fSt));

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    if (semDown (semgid, sh->foodArrived) == -1)      { 
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    sh->fSt.st.clientStat[id] = EAT; // O estado é atualizado para 5
    saveState (nFic, &(sh->fSt));

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

}

/**
 *  \brief client waits for others to finish meal, last client to arrive pays the bill. 
 *
 *  The client updates state and waits for others to finish meal before leaving and update its state. 
 *  Last client to finish meal should inform others that everybody finished.
 *  Last client to arrive at table should pay the bill by contacting waiter and waiting for waiter to arrive.
 *  The internal state should be saved twice.
 *
 *  \param id client id
 */
static void waitAndPay(int id)
{
    bool last = false;

    if (semDown(semgid, sh->mutex) == -1)
    { /* enter critical region */
        perror("error on the down operation for semaphore access (CT)");
        exit(EXIT_FAILURE);
    }
    /* insert your code here */
    if (sh->fSt.tableLast == id)
    { // entra aqui se for o ultimo a chegar à mesa
        last = true;
    }

    sh->fSt.st.clientStat[id] = WAIT_FOR_OTHERS; // O estado é atualizado para 6
    sh->fSt.tableFinishEat++; // incrementa o numero de clientes que terminaram de comer
    saveState(nFic, &(sh->fSt));

    if (sh->fSt.tableFinishEat == TABLESIZE)
    { // quando acabarem todos de comer
        for (int i = 0; i < TABLESIZE; i++)
        {
            semUp(semgid, sh->allFinished);
        }
    }
    if (semUp(semgid, sh->mutex) == -1)
    { /* enter critical region */
        perror("error on the down operation for semaphore access (CT)");
        exit(EXIT_FAILURE);
    }

    /* insert your code here */
    semDown(semgid, sh->allFinished); // Para esperar que terminem de comer, ou seja que passem pelo menos, ao estado 6
    if (last)
    {
        if (semDown(semgid, sh->mutex) == -1)
        { /* enter critical region */
            perror("error on the down operation for semaphore access (CT)");
            exit(EXIT_FAILURE);
        }
        /* insert your code here */
        sh->fSt.st.clientStat[id] = WAIT_FOR_BILL; // O estado é atualizado para 7
        saveState(nFic, &(sh->fSt));
        sh->fSt.paymentRequest = 1; // flag que vai ser usada para chamar a função receivePayment() do waiter no switch case
        if (semUp(semgid, sh->waiterRequest) == -1)
        { // Para ir chamar a função do Wait que depois chama a função receivePayment() do waiter
            perror("error on the down operation for semaphore access (WT)");
            exit(EXIT_FAILURE);
        }

        if (semUp(semgid, sh->mutex) == -1)
        { /* exits critical region */
            perror("error on the down operation for semaphore access (CT)");
            exit(EXIT_FAILURE);
        }
        /* insert your code here */
        if (semDown(semgid, sh->requestReceived) == -1)
        {
            perror("error on the down operation for semaphore access (WT)");
            exit(EXIT_FAILURE);
        }
    }

    if (semDown(semgid, sh->mutex) == -1)
    { /* enter critical region */
        perror("error on the down operation for semaphore access (CT)");
        exit(EXIT_FAILURE);
    }

    /* insert your code here */
    sh->fSt.st.clientStat[id] = FINISHED; // O estado é atualizado para 8
    saveState(nFic, &(sh->fSt));

    if (semUp(semgid, sh->mutex) == -1)
    { /* enter critical region */
        perror("error on the down operation for semaphore access (CT)");
        exit(EXIT_FAILURE);
    }
}
