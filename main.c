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
int male = -1;
int state = 0;
int previous_state;
int room = -1;
int timer;
int received_messages = 0;
int expected_messages = NUM_PROC - 1;
int rank;
int proc_num = 4;
int max_time = -1;
int mes_queue[NUM_PROC] = {-1};
int mes_queue_indx = 0;
int room_av[9] = {0};
int room_capacity = 2;
int my_room = -1;
    // n - kobiety w ntej szatni (np room_av[0], room_av[3], room_av[6])
    // n + 1 - mężczyźni w ntej szatni (np room_av[1], room_av[4], room_av[7])
    // n + 2 - liczba zajętych szafek w ntej szatni (np room_av[2], room_av[5], room_av[8])
int visited_pool_num = 0;

pthread_mutex_t	lock0 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond0 = PTHREAD_COND_INITIALIZER; 

int own_rand(int start, int end){
    int rnd = rand();
    int range = end-start;
    return start + (rnd%range);
}

void exit_with_error(char* err){
    printf(err);
    printf("Liczba wizyt na basenie: %d\n", visited_pool_num);
    exit(1234);
}

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
        // sleep(0.001);
        MPI_Status status;
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender = status.MPI_SOURCE;
        // printf("%d Received: sender %d, msg0 %d, state %d\n", rank, sender, msg[0], state);
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
                    // printf("%d: received %d messages\n", rank, received_messages);
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        received_messages = 0;
                    }
                break;
                case 1:
                    received_time = msg[1];
                    if(received_time > max_time)
                        max_time = received_time;

                    send_case_1(sender);
                    break;
                case 11:
                    // printf("%d\n", msg[0]);
                    send_case_11(sender);
                break;
                case 21:
                    send_case_21(sender);
                break;
                default:
                    exit_with_error("ERROR R case 0\n");
    
                break;
            }
            break;
        case 1:
            // P1
            switch(received_message_state){
                case 1:
                    send_case_1(sender);
                    break;
                case 11:
                    r_timer = msg[1];
                    r_previous_state = msg[2];

                    if(better_priority(sender, r_timer, r_previous_state)){
                        // kolejkujemy odebraną wiadomość do późniejszego odesłania
                        mes_queue[mes_queue_indx] = sender;
                        // printf("%d kolejkuje %d\n", rank, sender);
                        mes_queue_indx++;
                    } else {
                        send_case_11(sender);
                    }
                    break;
                case 10:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    // printf("%d dostaje %d\n", rank, sender);
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        received_messages = 0;
                    }
                    break;
                case 21:
                    send_case_21(sender);
                break;  
                default:
                    exit_with_error("ERROR R case 1\n");
                break;
            }
            break;
        case 2:
            // P2
            switch(received_message_state){
                case 1:
                    send_case_1(sender);
                    break;
                case 11:
                    mes_queue[mes_queue_indx] = sender;
                    // printf("%d kolejkuje %d\n", rank, sender);
                    mes_queue_indx++;
                    break;
                case 20:
                    received_messages++;
                    printf("%d Received: sender %d, msg0 %d, w szatni %d, szatnia %d, plec %d\n", rank, sender, msg[0], msg[1], msg[2], msg[3]);
                    increment_rooms();
    
                    if(received_messages == proc_num - 1){
                        pthread_cond_signal(&cond0);
                        received_messages = 0;
                    }
                    break;
                case 21:
                    exit_with_error("ERROR! Wątek będąc w stanie 2 odebrał wiadomość od stanu 2\n");
                break;  
                default:
                    exit_with_error("ERROR R case 2\n");
    
                break;
            }
            break;
        case 3:
            // szatnia
            switch(received_message_state){
                case 1:
                    send_case_1(sender);
                    break;
                case 11:
                    send_case_11(sender);
                    break;
                case 21:
                    send_case_21(sender);
                break;  
                default:
                    exit_with_error("ERROR R case 3\n");
    
                break;
            }
            break;
        case 4:
            //basen
            switch(received_message_state){
                case 1:
                    send_case_1(sender);
                    break;
                case 11:
                    send_case_11(sender);
                    break;
                case 21:
                    send_case_21(sender);
                break;  
                default:
                    exit_with_error("ERROR R case 4\n");
    
                break;
            }
            break;
        default:
            exit_with_error("ERROR Receive\n");
            break;
        }
    }
}

void send_case_21(int send_to){
    msg[0] = 20;
    if(state == 3){
        msg[1] = 1;
    } else {
        msg[1] = 0;
    }
    msg[2] = my_room;
    msg[3] = male;
    send_msg(send_to);
}

void send_msg(int send_to){
    // printf("%d Send: to %d msg0 %d\n", rank, send_to, msg[0]);
    MPI_Send(msg, MSG_SIZE, MPI_INT, send_to, MSG_HELLO, MPI_COMM_WORLD);
}

void send_case_1(int send_to){
    msg[0] = 0;
    msg[1] = timer;
    send_msg(send_to);
}

void send_case_11(int send_to){
    msg[0] = 10;
    send_msg(send_to);
}

void increment_rooms(){
    if(msg[1] > 0){// czy jest w szatni
        room_av[msg[2] + male]++; // zwiększamy licznik danej płci w szatni
        room_av[msg[2] + 2]++; // zajmuje szafke
    } else if(msg[2] >= 0) { // jeśli jest poza szatnią, ale ma zajętą szafke
        room_av[msg[2] + 2]++;
    }
}

void init(int rank){
    timer = rank;
    // male = own_rand(0, 2);
    male = rank % 2;
}

void other_stuff(){
    // sleep(own_rand(0,10));
    sleep(1);
}

void send_to_all(){
    for(int i = 0; i < NUM_PROC; i++) {
        if(i == rank) continue;
        send_msg(i);
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

    for(int i = 0; i < 9; i++){
        room_av[i] = 0;
    }
}

void change_state(int new_state){
    printf("%d: Zmieniam stan z %d na %d, [szatnia: %d, płeć: %d, timer: %d]\n", rank, state, new_state, my_room, male, timer);
    previous_state = state;
    state = new_state;
    reset_global_variables();
}

int available_room() {
    for(int i = 0; i < 3; i++){ // dla każdej sztani
        // sprawdzam czy jest jakaś wolna szafka
        // oraz czy w danej szatni jest aktualnie osoba przeciwnej płci
        // jeśli tak to zwracam numer tej szatni
        if(room_av[3*i + 2] < room_capacity && room_av[3*i + 1 - male] == 0){
            return i;
        } else if(room_av[3*i + 2] > room_capacity){
            exit_with_error("ERROR więcej zajętych szafek niż dostępnych!\n");
        } else if(room_av[3*i + 1] > 0 && room_av[3*i] > 0) {
            exit_with_error("ERROR kobieta i mężczyzna w jednej szatni!\n");
        }
    }
    // jeśli nie znajdzie szatni to zwracam -1
    return -1;
}

void resend_queued_messages(){
    for(int i = 0; i < mes_queue_indx; i++ ){ // odsyłam każdemu komu nie odpowiedziałem
        msg[0] = 10;
        // printf("%d resend %d\n", rank, mes_queue[i]);
        send_msg(mes_queue[i]);
        // printf("%d: ods %d\n", rank, mes_queue[i]);
        mes_queue[i] = -1;
    }
    mes_queue_indx = 0;
}

int main(int argc, char **argv)
{
    srand(rank);
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

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

    printf("%d: Zaczynam od stanu %d, [szatnia: %d, płeć: %d]\n", rank, state, my_room, male);
    sleep(1);
    while(1){
    switch (state) {
        case 0: //sekcja lokalna
            sleep(timer%4);
            msg[0] = 1;
            msg[1] = timer;
            // printf("%d Send\n", rank);
            send_to_all(); // wysyłamy wiadomość do wszystkich
            pthread_cond_wait(&cond0, &lock0);
            timer = max_time + 1;
            change_state(1);
            break;
        case 1: // P1
            // sleep(1);
            msg[0] = 11;
            msg[1] = timer;
            msg[2] = previous_state;
            // printf("%d wysyłam ALL\n", rank);
            send_to_all();
            // sleep(1);
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
            resend_queued_messages();
            sleep(timer%4 + 1);
            if(previous_state == 2){
                change_state(4);
            } else {
                my_room = -1;
                change_state(0);
            }
            break;
        case 4: // basen
            sleep(timer%4 + 1);
            visited_pool_num++;
            change_state(1);
            break;
        default:
            exit_with_error("ERROR Send\n");
        break;
        }
    }

    pthread_kill(threads[0], NULL);
	MPI_Finalize();
}