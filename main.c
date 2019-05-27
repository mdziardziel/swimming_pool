#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#define NUM_PROC 8
#define MSG_HELLO 100

int male = -1;
int state = 0;
int previous_state;
int room = -1;
int timer;
int received_messages = 0;
int expected_messages = NUM_PROC - 1;
int rank;
int proc_num = NUM_PROC;
int max_time = -1;
int mes_queue[NUM_PROC] = {-1};
int mes_queue_indx = 0;

int mes_queue_pool[NUM_PROC] = {-1};
int mes_queue_indx_pool = 0;

int room_av[9] = {0};
int room_capacity = 1;
int my_room = -1;
    // n - kobiety w ntej szatni (np room_av[0], room_av[3], room_av[6])
    // n + 1 - mężczyźni w ntej szatni (np room_av[1], room_av[4], room_av[7])
    // n + 2 - liczba zajętych szafek w ntej szatni (np room_av[2], room_av[5], room_av[8])
int visited_pool_num = 0;
int was_on_pool = -1;
int messages_sent[NUM_PROC] = {-1};
int additional_messages = 0;

pthread_mutex_t	lock0 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond0 = PTHREAD_COND_INITIALIZER; 

pthread_mutex_t	lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 

pthread_mutex_t	lock2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER; 

pthread_mutex_t	lock3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond3 = PTHREAD_COND_INITIALIZER; 

// pthread_mutex_t	lock_status = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t cond_status = PTHREAD_COND_INITIALIZER; 

pthread_mutex_t lock_status;

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
        if(get_previous_state() == 4){
            if(r_timer == timer){
                if(r_rank < rank) return 0;
            } else {
                if(r_timer < timer) return 0;
            }
        } else {
            return 0;
        }
    } else {
        if(get_previous_state() != 4){
            if(r_timer == timer){
                if(r_rank < rank) return 0;
            } else {
                if(r_timer < timer) return 0;
            }
        }
    }
    //return 1- mamy lepszy priorytet, 0 - mamy gorszy
    return 1;

    // if(r_prev_state == 4 && get_previous_state() != 4){
    //     return 0;
    // }else if(r_prev_state != 4 && get_previous_state() == 4){ 
    //     return 1;
    // }
    // else if(r_rank > rank) {
    //     return 0;
    // } else if(r_rank < rank){
    //     return 1;
    // }else {
    //     exit_with_error("RANKING");
    // }
}

void *wait_for_message(void *arguments){
    int msg[4] = {-1};
    while(1){
        sleep(0.01);
        MPI_Status status;
        MPI_Recv(msg, 4, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int sender = status.MPI_SOURCE;
        // printf("   %d Received: sender %d, msg0 %d, msg1 %d, msg2 %d, msg3 %d, state %d\n", rank, sender, msg[0], msg[1], msg[2], msg[3], state);
        int received_message_state = msg[0];
        int received_time;
        int r_timer;
        int r_previous_state;

        switch (get_state()) {
        case 0:
            // sekcja lokalna
            switch(received_message_state){
                // wiadomości z liczbą ostatnią cyfrą == 1 (np 21) -> wiadomość pytanie
                // wiadomości z liczbą ostatnią cyfrą == 0 (np 20) -> wiadomość odpowiedź
                case 0:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    // printf("%d: received %d messages\n", rank, received_messages);
                    received_time = msg[1];
                    if(received_time > max_time) max_time = received_time;
                    if(received_messages == proc_num - 1){
                        received_messages = 0;
                        pthread_cond_signal(&cond0);
                    }
                break;
                case 1:
                    send_case_1(sender);
                    break;
                case 11:
                    // printf("%d\n", msg[0]);
                    send_case_11(sender);
                break;
                case 21:
                    send_case_21(sender);
                break;
                case 30:
                break;
                default:
                    printf("%d: MSG STATE %d\n", rank, received_message_state);
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

                    // printf("%d: Sender %d\n", rank, sender);
                    //przysyłający ma lepszy priorytet i potwierdził nam, że mozemy wejść do P2
                    if(!better_priority(sender, r_timer, r_previous_state) && messages_sent[sender] == 0){ // jeśli ma lepszy priorytet i wysłaliśmmy mu już wiadomość
                        additional_messages++; //zwiększamy ilość oczekiwanych wiadomości o 1
                        // printf("sdfsdfsfds\n");
                        // printf("%d: ------- %d, timer: %d\n", rank, additional_messages, timer);
                        printf("    %d odsyłam jeszcze raz %d\n", rank, sender);
                        send_msg(sender,11, timer, get_previous_state(), -1); // wysyłamy kolejną wiadomość
                    } 
                        if(better_priority(sender, r_timer, r_previous_state)){ //mamy lepszy priorytet lub proces już nam pozowolił wejść
                            // kolejkujemy odebraną wiadomość do późniejszego odesłania
                            mes_queue[mes_queue_indx] = sender;
                            printf("    %d kolejkuje %d\n", rank, sender);
                            mes_queue_indx++;
                        } else {
                            // printf("%d: Lepsze %d\n", rank, sender);
                            send_case_11(sender);
                        }
                    

                    break;
                case 10:
                    received_messages++; //zwiększamy liczbę otrzymanych wiadomości
                    messages_sent[sender] = 0; // odznaczamy, że dostaliśmy wiadomość od tego procesu
                    printf("    %d sender %d additional_messages: %d, received_messages: \n", rank, sender, additional_messages, received_messages);
                    if(received_messages == proc_num + additional_messages - 1){

                                            // printf("%d: ++++++++ %d\n", rank, additional_messages);
                        received_messages = 0;
                        additional_messages = 0;
                        pthread_cond_signal(&cond1);
                    }
                    break;
                case 21:
                    send_case_21(sender);
                break;
                case 30:
                break;
                default:
                    printf("%d: MSG STATE %d\n", rank, received_message_state);
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
                    // if(msg[2] == 4 && previous_state != 4) { // jeśli ktoś przychodzi z basenu to nie będziemy go blokować skoro nie możemy wejść
                    //     send_msg(sender,11, timer, previous_state, -1); // wysyłamy kolejną wiadomość
                    //     additional_messages++; //zwiększamy ilość oczekiwanych wiadomości o 1
                    //     pthread_cond_signal(&cond1); //zwalniamy blokade
                    // } else {
                        if(msg[2] == 4){ //jeśli poprzedni stan nadawcy to basen
                            // printf("%d: basen %d\n", rank, sender);
                            
                            mes_queue_pool[mes_queue_indx_pool] = sender;
                        printf("%d kolejkuje pool %d\n", rank, sender);
                            mes_queue_indx_pool++;
                            pthread_cond_signal(&cond3);
                        } else {
                            mes_queue[mes_queue_indx] = sender;
                            // printf("%d kolejkuje %d\n", rank, sender);
                            mes_queue_indx++;
                        }
 
                    // }
                    break;
                case 20:
                    received_messages++;
                    // printf("%d Received: sender %d, msg0 %d, w szatni %d, szatnia %d, plec %d\n", rank, sender, msg[0], msg[1], msg[2], msg[3]);
                    increment_rooms(msg[1], msg[2], msg[3]);
    
                    if(received_messages == proc_num - 1){
                        received_messages = 0;
                        pthread_cond_signal(&cond2);
                    }
                    break;
                case 21:
                    // printf("%d: sender %d\n", rank, sender);
                    exit_with_error("Wątek będąc w stanie 2 odebrał wiadomość od stanu 2\n");
                    break;  
                case 30:
                    pthread_cond_signal(&cond3);
                break;
                default:
                    printf("%d: MSG STATE %d\n", rank, received_message_state);
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
                case 30:
                break;
                default:
                    printf("%d: MSG STATE %d\n", rank, received_message_state);
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
                case 30:
                break;
                default:
                    printf("%d: MSG STATE %d\n", rank, received_message_state);
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
    if(get_state() == 3){
        send_msg(send_to, 20, 1, my_room, male);
    } else {
        send_msg(send_to, 20, 0, my_room, male);
    }
}

void send_msg(int send_to, int m0, int m1, int m2, int m3){
    int send_msg[] = {m0, m1, m2, m3};

    printf("    %d Send: to %d msg0 %d\n", rank, send_to, m0);
    MPI_Send(send_msg, 4, MPI_INT, send_to, MSG_HELLO, MPI_COMM_WORLD);
}

void send_case_1(int send_to){
    send_msg(send_to, 0, timer, -1, -1);
}

void send_case_11(int send_to){
    send_msg(send_to, 10, -1, -1, -1);
}

void increment_rooms(int m1, int m2, int m3){
    // m1 - czy jest w szatni
    // m2 - numer szatni
    // m3 - płeć
    if(m1 > 0){// czy jest w szatni
        room_av[3 * m2 + m3]++; // zwiększamy licznik danej płci w szatni
        room_av[3 * m2 + 2]++; // zajmuje szafke
    } else if(m2 >= 0) { // jeśli jest poza szatnią, ale ma zajętą szafke
        room_av[3 * m2 + 2]++;
    }
}

// void decrement_rooms(int m1, int m2, int m3){
//     if(m1 == 1){// był na basenie, to znaczy, że może zwolnić szafke
//         room_av[3 * m2 + m3]--; // zmniejszamy licznik danej płci w szatni
//         room_av[3 * m2 + 2]--; // zwalnia szafke
//     } else if(m2 >= 0) { // nie idzie na basen to nie zwalnia szafki, tylko licznik danej płci w szatni
//         room_av[3 * m2 + m3]--; // zmniejszamy licznik danej płci w szatni
//     }
// }

void init(int rank){
    timer = rank;
    // male = own_rand(0, 2);
    male = rank % 2;
}

void other_stuff(){
    // sleep(own_rand(0,10));
    sleep(1);
}

void send_to_all(int m0, int m1, int m2, int m3){
    for(int i = 0; i < NUM_PROC; i++) {
        if(i == rank) continue;
        // messages_sent[i] = 1;
        send_msg(i, m0, m1, m2, m3);
    }
}

void reset_global_variables(){
    //reset msg
    received_messages = 0;

    for(int i = 0; i < 9; i++){
        room_av[i] = 0;
    }

    for(int i = 0; i < NUM_PROC; i++){
        messages_sent[i] = -1;
    }
}

void change_state(int new_state){
    pthread_mutex_lock(&lock_status);
    printf("%d: Zmieniam stan z %d na %d, [szatnia: %d, płeć: %d, timer: %d]\n", rank, state, new_state, my_room, male, timer);
    previous_state = state;
    state = new_state;
    reset_global_variables();
    pthread_mutex_unlock(&lock_status);
}

int get_state(){
    int st;
    pthread_mutex_lock(&lock_status);
    st = state;
    pthread_mutex_unlock(&lock_status);
    return st;
}

int get_previous_state(){
    int st;
    pthread_mutex_lock(&lock_status);
    st = previous_state;
    pthread_mutex_unlock(&lock_status);
    return st;
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
        // printf("%d resend %d\n", rank, mes_queue[i]);
        send_msg(mes_queue[i], 10, -1, -1, -1);
        // printf("%d: ods %d\n", rank, mes_queue[i]);
        mes_queue[i] = -1;
    }
    mes_queue_indx = 0;
}

void resend_queued_messages_pool(){
    for(int i = 0; i < mes_queue_indx_pool; i++ ){ // odsyłam każdemu komu nie odpowiedziałem
        // printf("%d resend %d\n", rank, mes_queue[i]);
        send_msg(mes_queue_pool[i], 10, -1, -1, -1);
        // printf("%d: ods %d\n", rank, mes_queue[i]);
        mes_queue_pool[i] = -1;
    }
    mes_queue_indx_pool = 0;
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
    int first_time = 1;
    int tmp_room = -1;

    thread_args[0] = 0;
    result_code = pthread_create(&threads[0], NULL, wait_for_message, &thread_args[0]);
    assert(!result_code);
// #####
    int first_visit = 1;
    printf("%d: Zaczynam od stanu %d, [szatnia: %d, płeć: %d]\n", rank, get_state(), my_room, male);
    sleep(1);
    while(1){
        // printf("rrr\n");
        sleep(0.01);
        // sleep(1);
        switch (get_state()) {
            case 0: //sekcja lokalna
                sleep(timer%4);
                // printf("%d Send\n", rank);

                // printf("%d : max time: %d\n", rank, max_time);
                if(first_visit == 1){
                    timer = rank;
                    first_visit = -1;
                } else {
                    send_to_all(1, timer, -1, -1); // wysyłamy wiadomość do wszystkich
                    pthread_cond_wait(&cond0, &lock0);
                    timer = max_time + 1;
                }

                change_state(1);
                break;
            case 1: // P1
                // sleep(1);
                // printf("%d wysyłam ALL\n", rank);
                send_to_all(11, timer, get_previous_state(), -1);
                // sleep(1);
                pthread_cond_wait(&cond1, &lock1);
                change_state(2);
                break;
            case 2: // P2
                send_to_all(21, -1, -1, -1); //pytamy o to kto w jakiej szatni
                // while(tmp_room == -1) {
                //     pthread_cond_wait(&cond0, &lock0); // czekamy na wszystkie odpowiedzi
                //     tmp_room = available_room();
                //     if(tmp_room == -1){ // jeśli nie mamy wolnej szatni
                //         pthread_cond_wait(&cond1, &lock1); // blokujemy i czekamy aż się zwolni miejsce 
                //         tmp_room = available_room(); // powinno być większe od -1, chyba, że przyszedł ktoś z basenu
                //     }
                // }
                // my_room = tmp_room;
                // tmp_room = -1;
                pthread_cond_wait(&cond2, &lock2); // czekamy na wszystkie odpowiedzi
                my_room = available_room();
                if(my_room == -1){
                    // timer--;
                    pthread_cond_wait(&cond3, &lock3); // czekamy na wszystkie odpowiedzi
                    change_state(1);
                    // sleep(1);
                    resend_queued_messages_pool();
                } else {
                    change_state(3);
                }
                break;
            case 3: // szatnia
                resend_queued_messages_pool();
                resend_queued_messages();
                sleep(timer%4 + 1);
                if(was_on_pool == -1){
                    // send_to_all(30, 0, my_room, male); // 32, czy był na basenie, nr szatni, plec
                    change_state(4);
                    send_to_all(30, -1, -1, -1); 
                } else {
                    // send_to_all(30, 1, my_room, male); // 32, czy był na basenie, nr szatni, plec
                    my_room = -1;
                    was_on_pool = -1;
                    change_state(0);
                    send_to_all(30, -1, -1, -1); 
                }
                break;
            case 4: // basen
                sleep(timer%4 + 1);
                visited_pool_num++;
                was_on_pool = 1;
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