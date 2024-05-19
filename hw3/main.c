#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// Constants for the capacity of the parking lot
#define PICKUP_CAPACITY 4
#define AUTOMOBILE_CAPACITY 8

#define MAX_CAR_OWNERS 40

// Semaphores for synchronization
sem_t newPickup;
sem_t inChargeforPickup;
sem_t newAutomobile;
sem_t inChargeforAutomobile;

// Counter variables for free parking spots
int mFree_pickup = PICKUP_CAPACITY;
int mFree_automobile = AUTOMOBILE_CAPACITY;

// Counter variables for total parked vehicles
int total_pickup = 0;
int total_automobile = 0;

// Mutexes to protect the counter variables
pthread_mutex_t pickupMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t automobileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int running = 1; // Global flag to control running state of attendants

// Function prototypes
void *carOwner(void *arg);
void *carAttendant(void *arg);
void check_termination_condition();

// Structure to represent a vehicle
typedef struct {
    int id;
    int type; // 0 for automobile, 1 for pickup
} Vehicle;


void check_termination_condition() {
    pthread_mutex_lock(&print_mutex);
    if (total_automobile == AUTOMOBILE_CAPACITY && total_pickup == PICKUP_CAPACITY) {
        printf("Parking lot is full. Signaling threads to terminate.\n");
        running = 0;
        sem_post(&newAutomobile); // Unblock any waiting attendants
        sem_post(&newPickup);     // Unblock any waiting attendants
    }
    pthread_mutex_unlock(&print_mutex);
}

void *carOwner(void *arg) {
    Vehicle *vehicle = (Vehicle *)arg;

    if (running) {
        if (vehicle->type == 0) { // Automobile
            pthread_mutex_lock(&automobileMutex);
            if (mFree_automobile > 0) {
                mFree_automobile--;
                printf("Automobile owner %d: parked temporarily. Remaining temp automobile spots: %d\n", vehicle->id, mFree_automobile);
                pthread_mutex_unlock(&automobileMutex);
                sem_post(&newAutomobile);
            } else {
                pthread_mutex_unlock(&automobileMutex);
                printf("Automobile owner %d: no temporary parking spot available. Leaving...\n", vehicle->id);
            }
        } else { // Pickup
            pthread_mutex_lock(&pickupMutex);
            if (mFree_pickup > 0) {
                mFree_pickup--;
                printf("Pickup owner %d: parked temporarily. Remaining temp pickup spots: %d\n", vehicle->id, mFree_pickup);
                pthread_mutex_unlock(&pickupMutex);
                sem_post(&newPickup);
            } else {
                pthread_mutex_unlock(&pickupMutex);
                printf("Pickup owner %d: no temporary parking spot available. Leaving...\n", vehicle->id);
            }
        }
    }

    free(vehicle);
    pthread_exit(NULL); // Exit the carOwner thread

    return NULL;
}

void *carAttendant(void *arg) {
    int type = *((int *)arg);
    free(arg);
    while (running) {
        if (type == 0) { // Automobile
            sem_wait(&newAutomobile);
            pthread_mutex_lock(&automobileMutex);
            if (total_automobile < AUTOMOBILE_CAPACITY) {
                mFree_automobile++;
                total_automobile++;
                printf("Car attendant: parked an automobile. Total parked automobiles: %d\n", total_automobile);
            } else {
                printf("Automobile cannot be parked. No empty parking space.\n");
            }
            pthread_mutex_unlock(&automobileMutex);
            sem_post(&inChargeforAutomobile);
        } else { // Pickup
            sem_wait(&newPickup);
            pthread_mutex_lock(&pickupMutex);
            if (total_pickup < PICKUP_CAPACITY) {
                mFree_pickup++;
                total_pickup++;
                printf("Pickup attendant: parked a pickup. Total parked pickups: %d\n", total_pickup);
            } else {
                printf("Pickup cannot be parked. No empty parking space.\n");
            }
            pthread_mutex_unlock(&pickupMutex);
            sem_post(&inChargeforPickup);
        }
        check_termination_condition();
        sleep(3); // Simulate the time taken to park a vehicle
    }
    return NULL;
}

int main() {
    // Initialize semaphores
    sem_init(&newPickup, 0, 0);
    sem_init(&inChargeforPickup, 0, 0);
    sem_init(&newAutomobile, 0, 0);
    sem_init(&inChargeforAutomobile, 0, 0);

    pthread_t carOwnerThreads[MAX_CAR_OWNERS];
    pthread_t carAttendantThreads[2];

    // Create car attendant threads
    for (int i = 0; i < 2; i++) {
        int *type = malloc(sizeof(int));
        *type = i; // 0 for automobile attendant, 1 for pickup attendant
        pthread_create(&carAttendantThreads[i], NULL, carAttendant, type);
    }
    int car_counter=0;
    // Create car owner threads
    for (int i = 0; i < MAX_CAR_OWNERS; i++) {
        if(running == 0) {
            car_counter=i;
            break; // Break if parking lot is full
        }
        Vehicle *vehicle = malloc(sizeof(Vehicle));
        vehicle->id = i;
        vehicle->type = rand() % 2; // Randomly assign type (0 for automobile, 1 for pickup)
        pthread_create(&carOwnerThreads[i], NULL, carOwner, vehicle);
        sleep(1); // Simulate the arrival of vehicles at different times
    }

    // Join car owner threads
    for (int i = 0; i < car_counter; i++) {
        pthread_join(carOwnerThreads[i], NULL);
    }

    // Ensure all car owner threads exit gracefully
    running = 0;
    sem_post(&newAutomobile); // Unblock any waiting attendants
    sem_post(&newPickup);     // Unblock any waiting attendants

    // Join car attendant threads
    for (int i = 0; i < 2; i++) {
        pthread_join(carAttendantThreads[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);

    return 0;
}
