/**
 *  \file semSharedMemSmoker.c (implementation file)
 *
 *  \brief Problem name: SoccerGame
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the players:
 *     \li arrive
 *     \li playerConstituteTeam
 *     \li waitReferee
 *     \li playUntilEnd
 *
 *  \author Nuno Lau - December 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
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

/** \brief player takes some time to arrive */
static void arrive (int id);

/** \brief player constitutes team */
static int playerConstituteTeam (int id);

/** \brief player waits for referee to start match */
static void waitReferee(int id, int team);

/** \brief player waits for referee to end match */
static void playUntilEnd(int id, int team);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the player.
 */
int main (int argc, char *argv[])
{
    int key;                                            /*access key to shared memory and semaphore set */
    char *tinp;                                                       /* numerical parameters test flag */
    int n, team;

    /* validation of command line parameters */
    if (argc != 4) { 
        freopen ("error_PL", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    

    /* get goalie id - argv[1]*/
    n = (unsigned int) strtol (argv[1], &tinp, 0);
    if ((*tinp != '\0') || (n >= NUMPLAYERS )) { 
        fprintf (stderr, "Player process identification is wrong!\n");
        return EXIT_FAILURE;
    }

    /* get logfile name - argv[2]*/
    strcpy (nFic, argv[2]);

    /* redirect stderr to error file  - argv[3]*/
    freopen (argv[3], "w", stderr);
    setbuf(stderr,NULL);


    /* getting key value */
    if ((key = ftok (".", 'a')) == -1) {
        perror ("error on generating the key");
        exit (EXIT_FAILURE);
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


    /* simulation of the life cycle of the player */
    arrive(n);
    if((team = playerConstituteTeam(n))!=0) {
        waitReferee(n, team);
        playUntilEnd(n, team);
    }

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief player takes some time to arrive
 *
 *  Player updates state and takes some time to arrive
 *  The internal state should be saved.
 *
 */
static void arrive(int id)
{    
    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    sh->fSt.st.playerStat[id] = ARRIVING;
    saveState (nFic, &sh->fSt);                                                                    // update player state with arriving state
    
    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    usleep((200.0*random())/(RAND_MAX+1.0)+50.0);
}

/**
 *  \brief player constitutes team
 *
 *  If player is late, it updates state and leaves.
 *  If there are enough free players and free goalies to form a team, player forms team allowing 
 *  team members to proceed and waiting for them to acknowledge registration.
 *  Otherwise it updates state, waits for the forming teammate to "call" him, saves its team
 *  and acknowledges registration.
 *  The internal state should be saved.
 *
 *  \param id player id
 * 
 *  \return id of player team (0 for late goalies; 1 for team 1; 2 for team 2)
 *
 */
static int playerConstituteTeam (int id)
{
    int ret = 0;

    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    
    /* TODO: insert your code here */
    sh->fSt.playersFree++;                                                                          // increment number of free players
    sh->fSt.playersArrived++;                                                                    // increment number of arrived players
    if(sh->fSt.playersArrived > NUMTEAMPLAYERS*2){                                         // check if there are enough players to form a team
        sh->fSt.st.playerStat[id] = LATE;
        saveState (nFic, &sh->fSt);                                                               // if there are enough players update player state with late state
        sh->fSt.playersFree--;
    }
    else if(sh->fSt.playersFree >= NUMTEAMPLAYERS && sh->fSt.goaliesFree >= NUMTEAMGOALIES){    // check if there are enough players and goalies to form a team
        sh->fSt.st.playerStat[id] = FORMING_TEAM;                                        // update player state with forming team state (first player to form team)
        sh->fSt.playersFree -= NUMTEAMPLAYERS;                                             // decrement number of free players
        sh->fSt.goaliesFree -= NUMTEAMGOALIES;                                           // decrement number of free goalies

        for (int i = 0; i < 3; i++) {
            if (semUp(semgid, sh->playersWaitTeam) == -1) {                                         // notify the rest of players (3) to form team
                perror("error on the up operation for semaphore access (PL)");
                exit(EXIT_FAILURE);
            }
        }
        if (semUp(semgid, sh->goaliesWaitTeam) == -1) {                                        // notify one goalie to form team
            perror("error on the up operation for semaphore access (PL)");
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i < NUMTEAMPLAYERS; i++){
            if (semDown(semgid, sh->playerRegistered) == -1) {                                  // wait for players to acknowledge registration
                perror("error on the up operation for semaphore access (PL)");
            exit(EXIT_FAILURE);
        }
        }
        ret = sh->fSt.teamId++;                                                                
        saveState (nFic, &sh->fSt);                                                          // update player state with team id
    } else {
        sh->fSt.st.playerStat[id] = WAITING_TEAM;                                     
        saveState (nFic, &sh->fSt);                                                         // update player state with waiting team state
    }
      
    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    if(sh->fSt.st.playerStat[id] == WAITING_TEAM){                                         // wait for forming teammate to call him
        if (semDown (semgid, sh->playersWaitTeam) == -1)  {                                                     
            perror ("error on the up operation for semaphore access (PL)");
            exit (EXIT_FAILURE);
        }
        ret = sh->fSt.teamId;   
        if (semUp (semgid, sh->playerRegistered) == -1)  {                                  // notify forming teammate to acknowledge registration                                                   
            perror ("error on the up operation for semaphore access (PL)");
            exit (EXIT_FAILURE);
        }
    } else if (sh->fSt.st.playerStat[id] == FORMING_TEAM) {
        if (semUp (semgid, sh->refereeWaitTeams) == -1)  {                                           // notify referee that team is formed
            perror ("error on the up operation for semaphore access (PL)");
            exit (EXIT_FAILURE);
        }
    }
    return ret;
}

/**
 *  \brief player waits for referee to start match
 *
 *  The player updates its state and waits for referee to end match.  
 *  The internal state should be saved.
 *
 *  \param id   player id
 *  \param team player team
 */
static void waitReferee (int id, int team)
{
    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    sh->fSt.st.playerStat[id] = (team == 1) ? WAITING_START_1 : WAITING_START_2;                  // choose waiting start state based on team
    saveState (nFic, &sh->fSt);                                                                   // update player state with waiting start state

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    if (semDown (semgid, sh->playersWaitReferee) == -1)  {                                         // wait for referee to start match
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    
}

/**
 *  \brief player waits for referee to end match
 *
 *  The player updates its state and waits for referee to end match.  
 *  The internal state should be saved.
 *
 *  \param id   player id
 *  \param team player team
 */
static void playUntilEnd (int id, int team)
{
    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    sh->fSt.st.playerStat[id] = (team == 1) ? PLAYING_1 : PLAYING_2;                       // choose playing state based on team
    saveState (nFic, &sh->fSt);                                                          // update player state with playing state

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    if(semUp(semgid, sh->playing) == -1){                                                       // notify that players are playing
        perror("error on the up operation for semaphore access (PL)");
        exit(EXIT_FAILURE);
    }
    if (semDown (semgid, sh->playersWaitEnd) == -1)  {                                                   // wait for referee to end match
        perror ("error on the up operation for semaphore access (PL)");
        exit (EXIT_FAILURE);
    }

}



