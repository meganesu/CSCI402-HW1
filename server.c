#include <assert.h>
/* FreeBSD */
#define _WITH_GETLINE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "window.h"
#include "db.h"
#include "words.h"
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

/* the encapsulation of a client thread, i.e., the thread that handles
 * commands from clients */
typedef struct Client {
        int id;
	pthread_t thread;
	window_t *win;
} client_t;

/* Interface with a client: get requests, carry them out and report results */
void *client_run(void *);
/* Interface to the db routines.  Pass a command, get a result */
int handle_command(char *, char *, int len);

// Declare global variables, mutexes, condition variables
int started = 0; // Number of threads that have been started. Use as Client ID.
int running = 0; // Number of threads that are currently running.
pthread_mutex_t running_mutex; // Lock for running.
pthread_cond_t running_cv; // Condition variable for running
client_t *chopping_block = NULL; // Next client to be killed by terminator()
pthread_mutex_t chopping_block_mutex; // Lock for chopping_block
pthread_cond_t chopping_block_cv; // Condition variable for chopping_block
int paused = 0; // Set to one when user enters 's'
pthread_mutex_t paused_mutex; // Lock for paused
pthread_cond_t paused_cv; // Condition variable for paused


/*
 * Create an interactive client - one with its own window.  This routine
 * creates the window (which starts the xterm and a process under it.  The
 * window is labelled with the ID passsed in.  On error, a NULL pointer is
 * returned and no process started.  The client data structure returned must be
 * destroyed using client_destroy()
 */
client_t *client_create(int ID) {
    client_t *new_Client = (client_t *) malloc(sizeof(client_t));
    char title[16];

    if (!new_Client) return NULL;

    sprintf(title, "Client %d", ID);

    new_Client->id = ID;

    /* Creates a window and set up a communication channel with it */
    if ((new_Client->win = window_create(title))) {
        pthread_mutex_lock(&running_mutex);
        running++;
        pthread_mutex_unlock(&running_mutex);
        return new_Client;
    }
    else {
	free(new_Client);
	return NULL;
    }
}

/*
 * Create a client that reads cmmands from a file and writes output to a file.
 * in and out are the filenames.  If out is NULL then /dev/stdout (the main
 * process's standard output) is used.  On error a NULL pointer is returned.
 * The returned client must be disposed of using client_destroy.
 */
client_t *client_create_no_window(char *in, char *out) {
    char *outf = (out) ? out : "/dev/stdout";
    client_t *new_Client = (client_t *) malloc(sizeof(client_t));

    if (!new_Client) return NULL;

    new_Client->id = started++;

    /* Creates a window and set up a communication channel with it */
    if( (new_Client->win = nowindow_create(in, outf))) {
        pthread_mutex_lock(&running_mutex);
        running++;
        pthread_mutex_unlock(&running_mutex);
        return new_Client;
    }
    else {
	free(new_Client);
	return NULL;
    }
}

/*
 * Destroy a client created with either client_create or
 * client_create_no_window.  The cient data structure, the underlying window
 * (if any) and process (if any) are all destroyed and freed, and any open
 * files are closed.  Do not access client after calling this function.
 */
void client_destroy(client_t *client) {
	/* Remove the window */
	window_destroy(client->win);
	free(client);
}

/* Code executed by the client */
void *client_run(void *arg)
{
	client_t *client = (client_t *) arg;

	/* main loop of the client: fetch commands from window, interpret
	 * and handle them, return results to window. */
	char *command = 0;
	size_t clen = 0;
	/* response must be empty for the first call to serve */
	char response[256] = { 0 };

	/* Serve until the other side closes the pipe */
	while (serve(client->win, response, &command, &clen) != -1) {
            // Check first to see if user has paused the system
            if (paused) {
                pthread_mutex_lock(&paused_mutex);
                // Double-check that system is actually still paused
                //   (since may have context switched and unpaused since initial check)
                if (paused) {
                    pthread_cond_wait(&paused_cv, &paused_mutex);
                }
                pthread_mutex_unlock(&paused_mutex);
            }
                  
	    handle_command(command, response, sizeof(response));
	}

        // If you get here, user has entered 'CTRL-D', so prepare to be terminated

        // Disable thread cancellation (for now)
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        // Acquire lock for chopping block
        pthread_mutex_lock(&chopping_block_mutex);

        //printf("I WANT TO DIE. -CLIENT %d\n", client->id);
        while (chopping_block != NULL) { // While someone else is waiting to die
            // Wait for a change to chopping block
            pthread_cond_wait(&chopping_block_cv, &chopping_block_mutex);
        }

        // If you get here, you have the lock and chopping block is free

        // Set client on chopping block to be killed next
        chopping_block = client;
        //printf("CHOPPING BLOCK SET TO CLIENT %d\n", client->id);

        // Let waiting threads know that chopping block state has changed
        pthread_cond_broadcast(&chopping_block_cv);
        //printf("WAITING THREADS ALERTED BY CLIENT\n");
        // Release the chopping block lock
        pthread_mutex_unlock(&chopping_block_mutex);
        //printf("CHOPPING BLOCK UNLOCKED FROM CLIENT\n");

        // Now that the lock has been released, it's safe to cancel the thread
        int s;
        s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if (s != 0) printf("ERROR SETTING CLIENT CANCEL STATE!\n");
        //printf("I CAN BE CANCELED NOW!\n");

        // Loop and wait to die. (RIP)
        while (1) { sleep(1000); }

	pthread_exit(NULL); // You should never get here
        //return 0;
}

/* Code executed by thread waiting to kill clients */
void *terminator()
{
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        while (1) {
            // Acquire the lock for the chopping block
            pthread_mutex_lock(&chopping_block_mutex);

            while (chopping_block == NULL) { // While there's no one to kill
                // Wait until there is a change to the chopping block
                pthread_cond_wait(&chopping_block_cv, &chopping_block_mutex);
            }

            // If you get here, you have the lock and someone wants to die


            //printf("WANT TO CANCEL THREAD %d\n", chopping_block->id);

            // Try to cancel client's thread
            //   This call simply says that cancel request has been queued,
            //   not that thread has actually been cancelled.
            pthread_cancel(chopping_block->thread);
            //printf("FINISHED CANCEL\n");

            void *res; // Set up location to store results of join()
            // Try to join with thread.
            //   Join will block until thread terminates.
            pthread_join(chopping_block->thread, &res);
            //printf("FINISHED JOIN\n");
            // Check result to see that thread was canceled.
            if (res == PTHREAD_CANCELED) {
                printf("Thread %d cancelled successfully!\n", chopping_block->id);
            }
            else printf("ERROR! THREAD %d DID NOT CANCEL...?\n", chopping_block->id);

            // Destroy client on chopping block
            client_destroy(chopping_block);

            // Adjust number of clients currently running
            pthread_mutex_lock(&running_mutex);
            running--;
            pthread_cond_signal(&running_cv);
            pthread_mutex_unlock(&running_mutex);

            // Set the chopping block to NULL so a new victim can be queued
            chopping_block = NULL;

            // Let waiting threads know that chopping_block state has changed
            pthread_cond_broadcast(&chopping_block_cv);

            // Release the chopping block lock
            pthread_mutex_unlock(&chopping_block_mutex);
            
        }
}

int handle_command(char *command, char *response, int len) {
    if (command[0] == EOF) {
	strncpy(response, "all done", len - 1);
	return 0;
    }
    interpret_command(command, response, len);
    return 1;
}

int main(int argc, char *argv[]) {

    if (argc != 1) {
	fprintf(stderr, "Usage: server\n");
	exit(1);
    }

    //client_t *c;	    /* A client to serve */
    //int started = 0;	    /* Number of clients started */

    // Initialize mutexes, condition variables
    pthread_mutex_init(&running_mutex, NULL);
    pthread_cond_init(&running_cv, NULL);
    pthread_mutex_init(&chopping_block_mutex, NULL);
    pthread_cond_init(&chopping_block_cv, NULL);
    pthread_mutex_init(&paused_mutex, NULL);
    pthread_cond_init(&paused_cv, NULL);

    // Set up thread attributes to make threads joinable explicitly
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Set up terminator thread
    pthread_t arnold;
    pthread_create(&arnold, &attr, terminator, NULL);

    //char *cmd = NULL; // Stores the command entered to the window by user
    //size_t cmd_len = 0;
    //ssize_t read;

    //char **parsed_cmd;

    int keepGoing = 1; // Set to 0 when you want to quit the program
    while(keepGoing) {
        // Get input from user
        char *cmd = NULL;
        size_t cmd_len = 0;
        ssize_t read = 0;
        read = getline(&cmd, &cmd_len, stdin); //getline returns num characters read from stdin
                                               //  (including '\n' character)
        // Basic error checking - make sure user typed something
        if (read == 1) {
          printf("YOU HAVE TO TYPE SOMETHING FIRST.\n");
          continue;
        }

        // Parse command from user
        //    char **split_words() returns a ref to an array of char * (words)
        //    char *cmd_words[] = split_words(cmd);
        char **parsed_cmd;
        parsed_cmd = split_words(cmd);
        int len = 0;
        while (parsed_cmd[len] != '\0') { len++; } // Determine num arguments in command

        // Do some basic error checking on command input
        if (parsed_cmd[0][1] != '\0') {
          // If first argument is more than one character
          printf("IMPROPER COMMAND FORMAT. CHECK FIRST ARGUMENT.\n");
          continue;
        }
        if (parsed_cmd[0][0] != 'E' && len > 1) {
          // If command has more than one argument (with exception of 'E' command)
          printf("IMPROPER COMMAND. TRY AGAIN.\n");
          continue;
        }
        if (parsed_cmd[0][0] == 'E' && (len < 2 || len > 3)) {
          // Check that 'E' command has input (and possibly output) argument
          printf("IMPROPER USAGE. TRY AGAIN. \'E input_file [output_file]\'\n");
          continue;
        }


        switch (parsed_cmd[0][0]) {

        case 'e': { // Create interactive client in a window

            client_t *c = NULL;

            if ((c = client_create(started++)) )  {
	        //client_run(c);
	        //client_destroy(c);
                pthread_create(&c->thread, &attr, client_run, (void *) c);
            }
        }
        break;

        case 'E': { // Create non-interactive client that reads commands from one file
                  //     and writes results to another
             client_t *c = NULL;

             if ((c = client_create_no_window(parsed_cmd[1], parsed_cmd[2]))) {
                 pthread_create(&c->thread, &attr, client_run, (void *) c);
             }
             else {
                 printf("INVALID INPUT FILE. TRY AGAIN.\n");
             }
        }
        break;

        case 's': // Stop processing command from clients
            /* 
                 Check "paused" variable. Acquire lock and set paused to true.

                 In clients running:
                     if paused, acquire paused_mutex.
                     if paused, enter wait on paused condition variable.
                     when unpaused, release lock and handle command (as usual)
            */
            pthread_mutex_lock(&paused_mutex);
            paused = 1;
            pthread_mutex_unlock(&paused_mutex);

            break;

        case 'g': // Continue processing command from clients

            /*
                  Acquire paused lock. Set paused to false.
                  Broadcast to waiting threads. Release lock.
            */
            pthread_mutex_lock(&paused_mutex);
            paused = 0;
            pthread_cond_broadcast(&paused_cv);
            pthread_mutex_unlock(&paused_mutex);

            break;

        case 'w': { // Stop processing server commands until currently running clients have terminated
            /*
                acquire lock on num_threads_running
                while num_threads_running is more than zero,
                    wait on running_threads_cv
                release lock on num_threads_running

                in terminator(), (thread that kills clients)
                    acquire running_threads_mutex after terminating a thread
                    update number of threads running (decrement by one)
                    signal anyone waiting on running_threads_cv
            */
            pthread_mutex_lock(&running_mutex);
            while (running) {
                pthread_cond_wait(&running_cv, &running_mutex);
            }
            pthread_mutex_unlock(&running_mutex);
            // Clear the stdin stream of any commands entered while system was paused
            //   (once stdin is empty, user nees to enter EOF, i.e. CTRL+D, manually
            //    to stop flushing input and resume listening to commands)
            //int ch;
            //printf("Flushing input. Enter EOF to continue.\n");
            //while ((ch = fgetc(stdin)) != EOF);// {
              //printf("still cleaning... %d\n", ch);
            //}
        }
        break;

        case 'x':
            keepGoing = 0;
        break;

        default:

            printf("ERROR. INVALID COMMAND.\n");

        }

        //printf("%zu\n", read);
        //if (read != -1) printf("\"hi, %s\"", cmd);

        // Free memory allocated by getline() and split_words()
        free(cmd);
        free_words(parsed_cmd);
    }

    // Clean-up time

    // Release/destroy everything you created
    //    destroy attributes, mutexes, conditions created
    pthread_mutex_destroy(&running_mutex);
    pthread_cond_destroy(&running_cv);
    pthread_mutex_destroy(&chopping_block_mutex);
    pthread_cond_destroy(&chopping_block_cv);
    pthread_mutex_destroy(&paused_mutex);
    pthread_cond_destroy(&paused_cv);
    pthread_attr_destroy(&attr);

    // Destroy terminator thread
    pthread_cancel(arnold); // Send cancel request
    void *res;
    pthread_join(arnold, &res); // Wait until cancellation is confirmed, thread terminated
    if (res == PTHREAD_CANCELED) {
        printf("Terminator thread canceled successfully... I'll be back.\n");
    }

    //if ((c = client_create(started++)) )  {
	//client_run(c);
	//client_destroy(c);
    //}
    fprintf(stderr, "Program terminating.\n");
    /* Clean up the window data */
    window_cleanup();

    pthread_exit(NULL);

    return 0;
}
