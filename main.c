#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#define MSG_SIZE 4
#define NUM_PROC 4
#define MSG_HELLO 100

int msg[MSG_SIZE];
int male;
int state = 0;
int previous_state;
int room = -1;
int timer;
int received_messages = 0;
int expected_messages = NUM_PROC - 1;
int rank;
int proc_num = 4;
int max_time = -1;
int mes_queue[NUM_PROC];
int mes_queue_indx = 0;
int room_av[9] = {0};


pthread_mutex_t	lock0 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond0 = PTHREAD_COND_INITIALIZER; 

int better_priority(int r_rank, int r_timer, int r_prev_state){
    if(r_prev_state == 4){ // 4 - stan basen
        if(previous_state == 4){
            if(r_timer == timer){
                if(r_rank < rank) return 0;
            } else {
                if(r_timer < timer) return 0;
            }
        } else {
            return 0;
        }
    } else {
        if(previous_state != 4){
            if(r_timer == timer){
                if(r_rank < rank) return 0;
            } else {
                if(r_timer < timer) return 0;
            }
        }
    }
    
    return 1;
}

void *wait_for_message(void *arguments){
    while(1){
        sleep(1);
        MPI_Status status;
        // printf("receive\n");
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender = status.MPI_SOURCE;
        int received_message_state = msg[0];
        printf("%d: ODBIERAM od %d [SEKCJA_LOKALNA?, CZAS] %d, %d\n", rank, sender, msg[0], msg[1]);
        //printf("%d: Otrzymalem token: %d, %d od %d\n", rank, msg[0], msg[1], status.MPI_SOURCE);
        
        int received_time;
        int r_timer;
        int r_previous_state;
        
        switch (state) {
        case 0: //sekcja 
            switch(received_message_state){
                case 0:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        printf("%d: ODBLOKOWAŁEM P0 \n", rank);
                        received_messages = 0;
                    }
                break;
                case 1:
                    received_time = msg[1];
                    if(received_time > max_time)
                        max_time = received_time;

                    msg[0] = 0;
                    msg[1] = timer;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                    printf("%d: Wysylam POTWIERDZENIE do %d ->> %d, %d\n", rank, sender, msg[0], msg[1]);

                break;
                case 11:
                    msg[0] = 10;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;
                case 21:
                    msg[0] = 20;
                    msg[1] = -1;
                    msg[2] = 0;
                    msg[3] = male;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;
            }
            break;
        case 1:
            printf("P1\n");//P1
            switch(received_message_state){
                case 1:
                    msg[0] = 0;
                    msg[1] = timer;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                    break;
                case 11:
                    // received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    r_timer = msg[1];
                    r_previous_state = msg[2];

                    if(better_priority(sender, r_timer, r_previous_state)){
                        // kolejkujemy odebraną wiadomość do późniejszego odesłania
                        mes_queue[mes_queue_indx] = sender;
                        mes_queue_indx++;
                        printf("%d: LEPSZY OD %d\n", rank, sender);
                    } else {
                        msg[0] = 10;
                        printf("%d: GORSZY OD %d\n", rank, sender);
                        MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                    }
                    break;
                case 10:
                    printf("%d: POWIEKSZYLEM RANK OD %d\n", rank, sender);
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        printf("%d: ODBLOKOWAŁEM P1 \n", rank);
                        received_messages = 0;
                    }
                    break;
                case 21:
                    msg[0] = 20;
                    msg[1] = -1;
                    msg[2] = 0;
                    msg[3] = male;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;  
            }
            break;
        case 2:
            case 20:
            received_messages++;
            if(msg[1] != -1){//czy jes t w szatni
                if(msg[3] == 0){
                
                } else {

                }
            }

            if(received_messages == proc_num - 1){
                pthread_cond_signal(&cond0);
                printf("%d: ODBLOKOWAŁEM P2 \n", rank);
                received_messages = 0;
            }
            // printf("P2 %d\n", rank);//P2
            break;
        case 3:
            printf("Szatnia\n");//szatnia
            break;
        case 4:
            printf("basen\n");//basen
            break;
        default:
            printf("warning\n");
            break;
        }
        //MPI_Send( msg, MSG_SIZE, MPI_INT, receiver, MSG_HELLO, MPI_COMM_WORLD );
    }
}

// void config_state(int )

void init(int rank){
    timer = rank;
    male = rand() % 2;
    printf("male: %d, time: %d\n", male, timer);
}

void other_stuff(){
    int sleep_time = rand() % 10;
    sleep(sleep_time);
}

void send_to_all(){
    for(int i = 0; i < NUM_PROC; i++) {
        if(i == rank) continue;
        MPI_Send( msg, MSG_SIZE, MPI_INT, i, 100, MPI_COMM_WORLD );
        printf("%d: Wysylam do %d ->> %d, %d, %d\n", rank, i, msg[0], msg[1], msg[2]);
    }
}

void change_state(int new_state){
    previous_state = state;
    state = new_state;
}

int is_room_available() {

}

int main(int argc, char **argv)
{
    srand(time(NULL));
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    printf("start\n");
    init(rank);


    int receiver;

// ##### THREADS
    pthread_t threads[1];
    int thread_args[1];
    int result_code;

    thread_args[0] = 0;
    result_code = pthread_create(&threads[0], NULL, wait_for_message, &thread_args[0]);
    assert(!result_code);
// #####

    while(1){
    switch (state) {
        case 0: //sekcja 
            printf("sekcja lokalna\n");
            other_stuff();
            msg[0] = 1;
            msg[1] = timer;
            send_to_all(); // wysyłamy wiadomość do wszystkich
            pthread_cond_wait(&cond0, &lock0);
            timer = max_time + 1;
            change_state(1);
            break;
        case 1:
            printf("P1 %d\n", rank);//P1
            msg[0] = 11;
            msg[1] = timer;
            msg[2] = previous_state;
            send_to_all();
            pthread_cond_wait(&cond0, &lock0);
            printf("%d: ODBLOKOWANY P1 \n", rank);
            change_state(2);
            break;
        case 2:
            printf("P2 %d\n", rank);//P2
            msg[0] = 21;
            send_to_all();
            pthread_cond_wait(&cond0, &lock0);
            printf("%d: ODBLOKOWANY P2\n", rank);
            if(is_room_available()){
                change_state(3);
            } else {
                change_state(1);
            }
            break;
        case 3:
            printf("Szatnia %d\n", rank);//szatnia
            break;
        case 4:
            printf("basen\n");//basen
            break;
        default:
            printf("warning\n");
            break;
        }
        //MPI_Send( msg, MSG_SIZE, MPI_INT, receiver, MSG_HELLO, MPI_COMM_WORLD );
    }

    pthread_kill(threads[0], NULL);
	MPI_Finalize();
}