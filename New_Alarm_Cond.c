/*
* alarm_cond.c
*
* This is an enhancement to the alarm_mutex.c program, which
* used only a mutex to synchronize access to the shared alarm
* list. This version adds a condition variable. The alarm
* thread waits on this condition variable, with a timeout that
* corresponds to the earliest timer request. If the main thread
* enters an earlier timeout, it signals the condition variable
* so that the alarm thread will wake up and process the earlier
* timeout first, requeueing the later request.
*/

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <pthread.h>

#include <time.h>
#include "errors.h"

#include<stdbool.h>
#include <semaphore.h>

#define MAXLENGTH 128
/*
* The "alarm" structure now contains the time_t (time since the
* Epoch, in seconds) for each alarm, so that they can be
* sorted. Storing the requested number of seconds would not be
* enough, since the "alarm thread" cannot tell how long it has
* been on the list.
*/

#define DEBUG true
typedef struct alarm_tag{

    struct alarm_tag *link;
    int id;
    int group_id;
    int seconds;
    time_t time;
    char message[128];
    bool flag;

} alarm_t;
// display threads initialization

typedef struct display_tag{
    int group_id;
    pthread_t tid;
    struct display_tag *link;
    struct alarm_tag *link_alarm;
// struct display_tag	  	*link_big;
} display_t;

// initializatio of the  mutex, semaphores, conditions
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t display_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;

sem_t sem;
sem_t d_sem;

//#define MAXLENGTH 128


alarm_t *alarm_list = NULL;
alarm_t *presentAlarm = NULL;


display_t *display_thread = NULL;



void alarm_insert(alarm_t *alarm) {
//   int status;
    alarm_t **last, *next;

    /*
    * LOCKING PROTOCOL:
    * 
    * This routine requires that the caller have locked the
    * alarm_mutex!
    */
    last = &alarm_list;
    next = *last;

    while(next != NULL){
        if(next->id >= alarm->id){
            alarm->link = next;
            *last = alarm;
            break;
        }

        last = &next->link;
        next = next->link;
        
    }
    /*
    * If we reached the end of the list, insert the new alarm
    * there.  ("next" is NULL, and "last" points to the link
    * field of the last item, or to the list header.)
    */


    if(next == NULL){
        *last = alarm;
        alarm->link = NULL;
        alarm->flag = false;
    }
    
    alarm->time = time(NULL) + alarm->seconds;

}


void update_alarm(alarm_t *update) {
    
    alarm_t *alarm = alarm_list;

    while(alarm != NULL){
        if(alarm->id == update->id){
            strcpy(alarm->message, update->message);
            alarm->seconds = update->seconds;
            alarm->group_id = update->group_id;
            update->time = time(NULL) + update->seconds;
            alarm->time = update->time;
            alarm->flag = true;
            printf("Alarm(%d) Changed at %ld: Group(%d) %ld %s\n",
                alarm->id, time(NULL), alarm->group_id, alarm->time, alarm->message);
            return;
        }
        alarm = alarm->link;
    }
    printf("No alarm with id: %d", update->id);
}

//for debugging purposes
void output_alarm(){
    printf("Alarms:\n");
    alarm_t *present = alarm_list;
    while(present != NULL){
        printf("Alarm(%d): Group(%d) %d %s\n",
        present->id, present->group_id, present->seconds, present->message);
        present = present->link;
    }
}

//returns the thread with matching group id
display_t *display_tag_thread(int group_id){
    display_t* present_thread = display_thread;
    while(present_thread != NULL && present_thread->group_id != group_id){
        present_thread = present_thread->link;
    }
    return present_thread;
}

//assigns an alarm to a display thread
void show_display(alarm_t* alarm, display_t* display){

    if(display->link_alarm == NULL){
        display->link_alarm = alarm;
    }else{
        alarm_t *presentAlarm = display->link_alarm;
        while(presentAlarm->link != NULL){
            presentAlarm = presentAlarm->link;
        }
    }

}

//removes an alarm from the alarm list
void alarm_remove(alarm_t* alarm){
    alarm_t *previous = NULL;
    alarm_t *present = alarm_list;

    while(present != NULL && present->id != alarm->id){
        previous = present;
        present = present->link;
    }

    if(previous != NULL){
        previous->link = present->link;
    }else{
        if(alarm->link == NULL){
            alarm_list = NULL;
        }
        else{
            alarm_list = alarm_list->link;
        }
    }

    return;
}

bool alarm_queue(alarm_t* alarm){
    bool flag;
    alarm_t* present = alarm_list;
    if(present == NULL){
        flag = false;
    }else{
        while(present != NULL){
            if(present->id == alarm->id){
                flag = true;
            }
            present = present->link;
        }
    }
    return flag;
} 




// display for alarms with particular group ids
void *alarm_display(void *arg){

    int status, sleep_time;
    display_t *self_display = arg;
    alarm_t *alarm = self_display->link_alarm;
    bool group_first = true;
    bool message_first = true;

    while(alarm != NULL){


        //alarm exist in global alarm list and alarm belongs to group
        if(alarm_queue(alarm) == true && alarm->group_id == self_display->group_id && alarm->flag == false){
            printf("Alarm(%d) Printed By Alarm Display Thread %d at %ld: Group(%d) %d %s\n",
                alarm->id, pthread_self(), time(NULL), alarm->group_id, alarm->time, alarm->message);
            sleep(5);
        }else{
            //alarm not in list anymore
            if(alarm_queue(alarm) == false){
                printf("Display Thread %d Has Stopped Printing Message of Alarm(%d) at %ld: Group(%d) %ld %s\n",
                    pthread_self(), alarm->id, time(NULL), alarm->group_id, alarm->time, alarm->message);
            }
            
            //alarm still in list but group id has been changed

            //previous group
            if(alarm_queue(alarm)==true && alarm->flag == true && alarm->group_id != self_display->group_id){
                printf("Display Thread %d Has Stopped Printing Message of Alarm(%d) at %ld: Changed Group(%d) %ld %s\n",
                    pthread_self(), alarm->id, time(NULL), alarm->group_id, alarm->time, alarm->message);
            }

            //alarm group
            if(alarm_queue(alarm)==true && alarm->flag == true && alarm->group_id == self_display->group_id){

                if(group_first == true){
                    printf("Display Thread %d Has Taken Over Printing Message of Alarm(%d) at %ld: Changed Group(%d) %ld %s\n",
                        pthread_self(), alarm->id, time(NULL), alarm->group_id, alarm->time, alarm->message); 
                    group_first = false;
                }
                printf("Alarm(%d) Printed By Alarm Display Thread %d at %ld: Group(%d) %d %s\n",
                    alarm->id, pthread_self(), time(NULL), alarm->group_id, alarm->time, alarm->message);
                sleep(5); 
            }

            //alarm still in list with same group id but changed message
            if(alarm_queue(alarm) == true && alarm->flag == true && alarm->group_id == self_display->group_id){ //add changed message flag in condition
                if(message_first== true){
                    printf("Display Thread %d Starts to Print Changed Message Alarm(%d) at %ld: Group(%d) %ld %s\n",
                        pthread_self(), alarm->id, time(NULL), alarm->group_id, alarm->time, alarm->message);
                    message_first = false;
                }
                printf("Alarm(%d) Printed By Alarm Display Thread %d at %ld: Group(%d) %d %s\n",
                    alarm->id, pthread_self(), time(NULL), alarm->group_id, alarm->time, alarm->message);
                sleep(5); 
            }


            if(alarm_queue(alarm)==false){
                //move to next alarm
                alarm = alarm->link;
            }

        }

    }
    //no more alarms assigned to display so terminate
    if(alarm == NULL){
        printf("No More Alarms in Group(%d): Display Thread %d exiting at %ld\n",
            self_display->group_id, pthread_self(), time(NULL)); 
        pthread_exit((void *)100);
    }

    return NULL;
}

// creates display threads and assigns alarms
void *alarm_thread(void *arg){

    int status, sleep_time;
    display_t *alarm_thread = (display_t*)malloc(sizeof(display_t));
    display_t* present_thread;
 

/*
    * Loop forever, processing commands. The alarm thread will
    * be disintegrated when the process exits. Lock the mutex
    * at the start -- it will be unlocked during condition
    * waits, so the main thread can insert alarms.
    */
    while(true){


        // Lock semaphore
        status = sem_wait(&sem);
        if(status != 0) err_abort(status, "Lock semaphore");


        /*
        * If the alarm list is empty, wait until an alarm is
        * added. Setting presentAlarm to 0 informs the insert
        * routine that the thread is not busy.
        */

        status = pthread_cond_wait(&alarm_cond, &alarm_mutex);
        if(status != 0) err_abort(status, "Wait on Cond");

        if(presentAlarm == NULL){
            sleep_time = 1;
        }else{

            
            
            display_t* display = display_tag_thread(presentAlarm->group_id);

        
            if(display != NULL && display->group_id == presentAlarm->group_id){
                show_display(presentAlarm,display);
                printf("Alarm Thread Display Alarm Thread %d Assigned to Display Alarm(%d) at %ld: Group(%d) %ld %s\n",
                    display->tid, presentAlarm->id, time(NULL), presentAlarm->group_id, presentAlarm->time, presentAlarm->message);
            }else{
            
                alarm_thread->group_id = presentAlarm->group_id;
                alarm_thread->link = NULL;
                alarm_thread->link_alarm = presentAlarm;
                pthread_create(&alarm_thread->tid, NULL, alarm_display, (void*)alarm_thread);

                // Lock display semaphore
                status = sem_wait(&d_sem);
                if(status != 0) err_abort(status, "Lock semaphore");
                

                if(display_thread == NULL){
                    display_thread = alarm_thread;
                    present_thread = display_thread;
                }
                else{
                    present_thread = display_thread;
                    while(present_thread->link != NULL){
                        present_thread = present_thread->link;
                    }
                    present_thread->link = alarm_thread;
                }

                printf("Alarm Group Display Thread Created alarm Display Alarm Thread %d For Alarm(%d) at %ld: Group(%d) %ld %s\n",
                    present_thread->tid, presentAlarm->id, time(NULL), presentAlarm->group_id, presentAlarm->time, presentAlarm->message);
                
                // Unlock display semaphore
                status = sem_post(&d_sem);
                if(status != 0) err_abort(status, "Unlock semaphore");



            }

        }
        // Unlock semaphore
        status = sem_post(&sem);
        if(status != 0) err_abort(status, "Unlock semaphore");

        sleep(sleep_time);

    }

    return NULL;
}

// removes the expired alarms .
void *alarm_dequeue(void *arg){

    int status, sleep_time;
    alarm_t *present;


    while(true){

        // Lock semaphore
        status = sem_wait(&sem);
        if(status != 0) err_abort(status, "Unlock semaphore");

        if(alarm_list == NULL){
            sleep_time = 1;
        }else{

            present = alarm_list;

            while(present != NULL){
                if(time(NULL) >= present->time){
                    alarm_remove(present);
                    printf("Alarm Removal Thread Has Removed Alarm(%d) at %ld: Group(%d) %ld %s\n",
                        present->id, time(NULL), present->group_id, present->time, present->message);
                    break;
                }
                present = present->link;
            }
#ifdef DEBUG
            output_alarm();
#endif 
        }

        // Unlock semaphore
        status = sem_post(&sem);
        if(status != 0) err_abort(status, "Unlock semaphore");

        sleep(sleep_time);
    }

    return NULL;
}




int main(int argc, char *argv[]) {

    pthread_t thread_alarm;
    pthread_t thread_removal;
    int status;
    alarm_t *alarm;
    char line[128];
    int alarm_id, group_id, seconds;
    char message[MAXLENGTH+1];

    //Initializing the threads and semaphores
    status = sem_init(&sem, 0, 2);
    if(status != 0) err_abort(status, "Semaphore Init error");

    status = sem_init(&d_sem, 0, 1);
    if(status != 0) err_abort(status, "Semaphore Init error");

    status = pthread_create(&thread_alarm, NULL, alarm_thread, NULL);
    if(status != 0) err_abort(status, "Create Alarm Group Display");

    status = pthread_create(&thread_removal, NULL, alarm_dequeue, NULL);
    if(status != 0) err_abort(status, "Create Alarm Removal");

    while (true) {
        printf("Alarm> ");

        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        
        alarm = (alarm_t *)malloc(sizeof(alarm_t));
        if(alarm == NULL) errno_abort("Allocate Alarm");

        if (sscanf(line, "Start_Alarm(%d): Group(%d) %d %64[^\n]",
                &alarm->id, &alarm->group_id, &alarm->seconds, alarm->message) == 4) {

            // Lock semaphore
            status = sem_wait(&sem);
            if(status != 0) err_abort(status, "Lock semaphore");



            alarm_insert(alarm);
            printf("Alarm(%d) Inserted by Main Thread %d Into Alarm List at %ld: Group(%d) %ld %s\n",
                alarm->id, pthread_self(), time(NULL), alarm->group_id, alarm->time, alarm->message);
            
            
            presentAlarm = alarm;
            status = pthread_cond_signal(&alarm_cond);
            if(status != 0) err_abort(status, "Signal Condition");
            
            // Unlock semaphore
            status = sem_post(&sem);
            if(status != 0) err_abort(status, "Unlock semaphore");



            
        } else if (sscanf(line, "Change_Alarm(%d): Group(%d) %d %64[^\n]",
                        &alarm->id, &alarm->group_id, &alarm->seconds, alarm->message) == 4) {
            // Lock semaphore
            status = sem_wait(&sem);
            if(status != 0) err_abort(status, "Lock semaphore");



            update_alarm(alarm);
            /*
            * Insert the new alarm into the list of alarms,
            * sorted by expiration time.
            */
            // Unlock semaphore
            status = sem_post(&sem);
            if(status != 0) err_abort(status, "Unlock semaphore");


        } else {
            printf("Invalid alarm request\n");
            free(alarm);
        }
    }

    return 0;
}