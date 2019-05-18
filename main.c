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
int room_capacity = 2;
int my_room = -1;
    // n - kobiety w ntej szatni (np room_av[0], room_av[3], room_av[6])
    // n + 1 - mężczyźni w ntej szatni (np room_av[1], room_av[4], room_av[7])
    // n + 2 - liczba zajętych szafek w ntej szatni (np room_av[2], room_av[5], room_av[8])

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
        MPI_Status status;
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender = status.MPI_SOURCE;
        int received_message_state = msg[0];
        int received_time;
        int r_timer;
        int r_previous_state;

        switch (state) {
        case 0:
            // sekcja lokalna
            switch(received_message_state){
                // wiadomości z liczbą ostatnią cyfrą == 1 (np 21) -> wiadomość pytanie
                // wiadomości z liczbą ostatnią cyfrą == 0 (np 20) -> wiadomość odpowiedź
                case 0:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
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
                break;
                case 11:
                    msg[0] = 10;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;
                case 21:
                    msg[0] = 20;
                    msg[1] = -1;
                    msg[2] = -1;
                    msg[3] = male;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;
            }
            break;
        case 1:
            // P1
            switch(received_message_state){
                case 1:
                    msg[0] = 0;
                    msg[1] = timer;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                    break;
                case 11:
                    r_timer = msg[1];
                    r_previous_state = msg[2];

                    if(better_priority(sender, r_timer, r_previous_state)){
                        // kolejkujemy odebraną wiadomość do późniejszego odesłania
                        mes_queue[mes_queue_indx] = sender;
                        mes_queue_indx++;
                    } else {
                        msg[0] = 10;
                        MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                    }
                    break;
                case 10:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        received_messages = 0;
                    }
                    break;
                case 21:
                    msg[0] = 20;
                    msg[1] = -1;
                    msg[2] = -1;
                    msg[3] = male;
                    MPI_Send(msg, MSG_SIZE, MPI_INT, sender, MSG_HELLO, MPI_COMM_WORLD );
                break;  
            }
            break;
        case 2:
            // P2
            case 20:
            received_messages++;
            increment_rooms();

            if(received_messages == proc_num - 1){
                pthread_cond_signal(&cond0);
                received_messages = 0;
            }
            break;
        case 3:
            // szatnia
            break;
        case 4:
            //basen
            break;
        default:
            printf("warning\n");
            break;
        }
    }
}

void increment_rooms(){
    if(msg[1] >= 0){// czy jest w szatni
        if(msg[3] == 0){ // czy jest kobietą
            room_av[msg[2]]++;
        } else { //jest mężczyzną
            room_av[msg[2] + 1]++;
        }
    } else if(msg[2] >= 0) { // jeśli jest poza szatnią, ale ma zajętą szafke
        room_av[msg[2] + 2]++;
    }
}

void init(int rank){
    timer = rank;
    male = rand() % 2;
}

void other_stuff(){
    int sleep_time = rand() % 10;
    sleep(sleep_time);
}

void send_to_all(){
    for(int i = 0; i < NUM_PROC; i++) {
        if(i == rank) continue;
        MPI_Send( msg, MSG_SIZE, MPI_INT, i, 100, MPI_COMM_WORLD );
    }
}

void reset_global_variables(){
    //reset msg
    for(int i = 0; i < MSG_SIZE; i++){
        msg[i] = -1;
    }
    received_messages = 0;
    expected_messages = NUM_PROC - 1;
    max_time = -1;

    for(int i = 0; i < 3 * 3; i++){
        room_av[i] = 0;
    }
}

void change_state(int new_state){
    printf("%d: Zmieniam stan z %d na %d, [szatnia: %d, płeć: %d]\n", rank, state, new_state, my_room, male);
    previous_state = state;
    state = new_state;
    reset_global_variables();
}

int available_room() {
    for(int i = 0; i < 3; i++){ // dla każdej sztani
        if(room_av[3*i + 2] < room_capacity && room_av[3*i + 1 - male] == 0) return i;
        // sprawdzam czy jest jakaś wolna szafka
        // oraz czy w danej szatni jest aktualnie osoba przeciwnej płci
        // jeśli tak to zwracam numer tej szatni
    }
    // jeśli nie znajdzie szatni to zwracam -1
    return -1;
}

int main(int argc, char **argv)
{
    srand(time(NULL));
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    printf("%d: Zaczynam od stanu %d, [szatnia: %d, płeć: %d]\n", rank, state, my_room, male);
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
        case 0: //sekcja lokalna
            other_stuff();
            msg[0] = 1;
            msg[1] = timer;
            send_to_all(); // wysyłamy wiadomość do wszystkich
            pthread_cond_wait(&cond0, &lock0);
            timer = max_time + 1;
            change_state(1);
            break;
        case 1: // P1
            msg[0] = 11;
            msg[1] = timer;
            msg[2] = previous_state;
            send_to_all();
            pthread_cond_wait(&cond0, &lock0);
            change_state(2);
            break;
        case 2: // P2
            msg[0] = 21;
            send_to_all();
            pthread_cond_wait(&cond0, &lock0);
            my_room = available_room();
            if(my_room > -1){ 
                change_state(3);
            } else {
                change_state(1);
            }
            break;
        case 3: // szatnia
            sleep(1000);
            break;
        case 4: // basen
            break;
        default:
            break;
        }
    }

    pthread_kill(threads[0], NULL);
	MPI_Finalize();
}