#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

static struct lock * intersectionLock;
static struct cv * intersectionCV;
int num_cars_in_intersection;
Direction right[4] = {west, north, east, south};

struct car {

  Direction origin;
  Direction destination;
  struct car * next;

};

// wrapper for the car list in the intersection
struct car * head;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  // intersectionSem = sem_create("intersectionSem",1);
  // if (intersectionSem == NULL) {
  //   panic("could not create intersection semaphore");
  // }
  intersectionLock = lock_create("intersectionLock");

  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }

  intersectionCV = cv_create("intersectionCV");

  if (intersectionCV == NULL) {
    panic("could not create intersection cv");
  }

  head = NULL;
  num_cars_in_intersection = 0;

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  // KASSERT(intersectionSem != NULL);
  // sem_destroy(intersectionSem);

  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);
  KASSERT(num_cars_in_intersection == 0);
  lock_destroy(intersectionLock);
  cv_destroy(intersectionCV);
  // while (head != NULL){
  //   car * temp = head -> next;
  //   kfree(head);
  //   head = temp;
  // }
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);

  struct car * new_in = kmalloc(sizeof(struct car));
  new_in -> origin = origin;
  new_in -> destination = destination;
  new_in -> next = NULL;

  //to change the critical section, i.e. the cars list, need to acquire first;

  lock_acquire(intersectionLock);
  //kprintf("Vehicle num in entry : %d \n", num_cars_in_intersection);
  // when there are no cars in the intersection, just add a car to the list;
  if (num_cars_in_intersection == 0){
    head = new_in;
    num_cars_in_intersection++;
    //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
    lock_release(intersectionLock);
    return;
  }

  // when there exists cars in the intersection,
  // if we want to add car to the list,
  // we need to check if all pairs meet the requirement;

  struct car * pair = head;
  while (pair != NULL){

    if (((pair -> origin) == origin) ||
       (((pair -> origin) == destination) && ((pair -> destination) == origin)) || 
       (((pair -> destination) != destination) && ((right[pair->origin] == pair -> destination) || (right[origin] == destination)))) {


      if (pair -> next == NULL){

        pair -> next = new_in;
        num_cars_in_intersection++;
        //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
        lock_release(intersectionLock);
        return;

      }
      pair = pair -> next;

    } else {

      cv_wait(intersectionCV, intersectionLock);
      pair = head;
      if (head == NULL){
        head = new_in;
        num_cars_in_intersection++;
        //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
        lock_release(intersectionLock);
        return;
      }


    }


  }


}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);
  KASSERT(num_cars_in_intersection != 0);
  
  /* replace this default implementation with your own implementation */


  // lock_acquire(intersectionLock);
  // if (num_cars_in_intersection == 1) {

  //   kfree(head);
  //   num_cars_in_intersection --;
  //   head = NULL;
  //   lock_release(intersectionLock);
  //   return;

  // }

  lock_acquire(intersectionLock);
  //kprintf("Vehicle num after entry : %d \n", num_cars_in_intersection);

  if ((num_cars_in_intersection == 1) && ((origin != head -> origin) || (destination != head -> destination))){

    panic("not exit the car that want to exit");

  }

  struct car * current = head;
  struct car * next = head -> next;

  //if the head exit;

  if ((current -> origin == origin) && (current -> destination == destination)){

    kfree(current);
    head = next;
    num_cars_in_intersection--;

    //borad cast is faster and robust
    //cv_signal(intersectionCV, intersectionLock);
    cv_broadcast(intersectionCV, intersectionLock);
    lock_release(intersectionLock);
    return;

  }

  while (next != NULL){

    if ((next -> origin == origin) && (next -> destination == destination)){

       current -> next = next -> next;
       kfree(next);
       num_cars_in_intersection--;

       //borad cast is faster and robust
       //cv_signal(intersectionCV, intersectionLock);
       cv_broadcast(intersectionCV, intersectionLock);
       lock_release(intersectionLock);
       return;

    } else {

      current = next;
      next = current -> next;

    }


  }
  //kprintf("Vehicle num in exit: %d \n", num_cars_in_intersection);
  lock_release(intersectionLock);
  panic("not exit the car that want to exit");

}
