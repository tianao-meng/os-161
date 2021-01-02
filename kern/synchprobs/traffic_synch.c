// #include <types.h>
// #include <lib.h>
// #include <synchprobs.h>
// #include <synch.h>
// #include <opt-A1.h>

// /* 
//  * This simple default synchronization mechanism allows only vehicle at a time
//  * into the intersection.   The intersectionSem is used as a a lock.
//  * We use a semaphore rather than a lock so that this code will work even
//  * before locks are implemented.
//  */

// /* 
//  * Replace this default synchronization mechanism with your own (better) mechanism
//  * needed for your solution.   Your mechanism may use any of the available synchronzation
//  * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
//  * declare other global variables if your solution requires them.
//  */

// /*
//  * replace this with declarations of any synchronization and other variables you need here
//  */
// //static struct semaphore *intersectionSem;

// static struct lock * intersectionLock;
// static struct cv * intersectionCV;
// int num_cars_in_intersection;
// Direction right[4] = {west, north, east, south};

// struct car {

//   Direction origin;
//   Direction destination;
//   struct car * next;

// };

// // wrapper for the car list in the intersection
// struct car * head;

// /* 
//  * The simulation driver will call this function once before starting
//  * the simulation
//  *
//  * You can use it to initialize synchronization and other variables.
//  * 
//  */
// void
// intersection_sync_init(void)
// {
//   /* replace this default implementation with your own implementation */

//   // intersectionSem = sem_create("intersectionSem",1);
//   // if (intersectionSem == NULL) {
//   //   panic("could not create intersection semaphore");
//   // }
//   intersectionLock = lock_create("intersectionLock");

//   if (intersectionLock == NULL) {
//     panic("could not create intersection lock");
//   }

//   intersectionCV = cv_create("intersectionCV");

//   if (intersectionCV == NULL) {
//     panic("could not create intersection cv");
//   }

//   head = NULL;
//   num_cars_in_intersection = 0;

//   return;
// }

// /* 
//  * The simulation driver will call this function once after
//  * the simulation has finished
//  *
//  * You can use it to clean up any synchronization and other variables.
//  *
//  */
// void
// intersection_sync_cleanup(void)
// {
//   /* replace this default implementation with your own implementation */
//   // KASSERT(intersectionSem != NULL);
//   // sem_destroy(intersectionSem);

//   KASSERT(intersectionLock != NULL);
//   KASSERT(intersectionCV != NULL);
//   KASSERT(num_cars_in_intersection == 0);
//   lock_destroy(intersectionLock);
//   cv_destroy(intersectionCV);
//   // while (head != NULL){
//   //   car * temp = head -> next;
//   //   kfree(head);
//   //   head = temp;
//   // }
// }


// /*
//  * The simulation driver will call this function each time a vehicle
//  * tries to enter the intersection, before it enters.
//  * This function should cause the calling simulation thread 
//  * to block until it is OK for the vehicle to enter the intersection.
//  *
//  * parameters:
//  *    * origin: the Direction from which the vehicle is arriving
//  *    * destination: the Direction in which the vehicle is trying to go
//  *
//  * return value: none
//  */

// void
// intersection_before_entry(Direction origin, Direction destination) 
// {
//   /* replace this default implementation with your own implementation */
//   KASSERT(intersectionLock != NULL);
//   KASSERT(intersectionCV != NULL);

//   struct car * new_in = kmalloc(sizeof(struct car));
//   new_in -> origin = origin;
//   new_in -> destination = destination;
//   new_in -> next = NULL;

//   //to change the critical section, i.e. the cars list, need to acquire first;

//   lock_acquire(intersectionLock);
//   //kprintf("Vehicle num in entry : %d \n", num_cars_in_intersection);
//   // when there are no cars in the intersection, just add a car to the list;
//   if (num_cars_in_intersection == 0){
//     head = new_in;
//     num_cars_in_intersection++;
//     //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
//     lock_release(intersectionLock);
//     return;
//   }

//   // when there exists cars in the intersection,
//   // if we want to add car to the list,
//   // we need to check if all pairs meet the requirement;

//   struct car * pair = head;
//   while (pair != NULL){

//     if (((pair -> origin) == origin) ||
//        (((pair -> origin) == destination) && ((pair -> destination) == origin)) || 
//        (((pair -> destination) != destination) && ((right[pair->origin] == pair -> destination) || (right[origin] == destination)))) {


//       if (pair -> next == NULL){

//         pair -> next = new_in;
//         num_cars_in_intersection++;
//         //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
//         lock_release(intersectionLock);
//         return;

//       }
//       pair = pair -> next;

//     } else {

//       cv_wait(intersectionCV, intersectionLock);
//       pair = head;
//       if (head == NULL){
//         head = new_in;
//         num_cars_in_intersection++;
//         //kprintf("Vehicle num after add : %d \n", num_cars_in_intersection);
//         lock_release(intersectionLock);
//         return;
//       }


//     }


//   }


// }


// /*
//  * The simulation driver will call this function each time a vehicle
//  * leaves the intersection.
//  *
//  * parameters:
//  *    * origin: the Direction from which the vehicle arrived
//  *    * destination: the Direction in which the vehicle is going
//  *
//  * return value: none
//  */

// void
// intersection_after_exit(Direction origin, Direction destination) 
// {
//   KASSERT(intersectionLock != NULL);
//   KASSERT(intersectionCV != NULL);
//   KASSERT(num_cars_in_intersection != 0);
  
//   /* replace this default implementation with your own implementation */


//   // lock_acquire(intersectionLock);
//   // if (num_cars_in_intersection == 1) {

//   //   kfree(head);
//   //   num_cars_in_intersection --;
//   //   head = NULL;
//   //   lock_release(intersectionLock);
//   //   return;

//   // }

//   lock_acquire(intersectionLock);
//   //kprintf("Vehicle num after entry : %d \n", num_cars_in_intersection);

//   if ((num_cars_in_intersection == 1) && ((origin != head -> origin) || (destination != head -> destination))){

//     panic("not exit the car that want to exit");

//   }

//   struct car * current = head;
//   struct car * next = head -> next;

//   //if the head exit;

//   if ((current -> origin == origin) && (current -> destination == destination)){

//     kfree(current);
//     head = next;
//     num_cars_in_intersection--;

//     //borad cast is faster and robust
//     //cv_signal(intersectionCV, intersectionLock);
//     cv_broadcast(intersectionCV, intersectionLock);
//     lock_release(intersectionLock);
//     return;

//   }

//   while (next != NULL){

//     if ((next -> origin == origin) && (next -> destination == destination)){

//        current -> next = next -> next;
//        kfree(next);
//        num_cars_in_intersection--;

//        //borad cast is faster and robust
//        //cv_signal(intersectionCV, intersectionLock);
//        cv_broadcast(intersectionCV, intersectionLock);
//        lock_release(intersectionLock);
//        return;

//     } else {

//       current = next;
//       next = current -> next;

//     }


//   }
//   //kprintf("Vehicle num in exit: %d \n", num_cars_in_intersection);
//   lock_release(intersectionLock);
//   panic("not exit the car that want to exit");

// }

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
// static struct semaphore *intersectionSem; // WE AINT GONNA USE NO SEMAPHORES IN THIS ONE 

static struct lock *mutex; 

// The RULES of the ROAD 
// If two vehicles are in the intersection simultaneously, 
// then at least ONE of the following must be true: 

// 1) V1.origin == V2.origin 
// 2) V1.origin == V2.destination && V1.destination == V2.origin 
// 3) V1.destination != V2.destination && ( V1 right turn || V2 right turn)

// To ensure these rules of the road are being followed, 
// we need the following boolean conditions: 
//int volatile occupied_destinations[4]; //N:0 E:1 S:2 W:3
//int volatile occupied_origins[4];      //N:0 E:1 S:2 W:3
//int volatile right_turns;

// New System: "One Direction"
// condition 1 is a strong independent condition who needs no other conditions to let traffic flow efficiently
int volatile waiting_from[4]; // N:0 E:1 S:2 W:3 
int volatile flow_direction;  // N:0 E:1 S:2 W:3

int volatile intersection_vehicles;

// NEW 
static struct cv *go[4]; //N:0 E:1 S:2 W:3

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

  //intersectionSem = sem_create("intersectionSem",1);
  //if (intersectionSem == NULL) {
  //  panic("could not create intersection semaphore");
  //}

  // LOCKS
  mutex = lock_create("mutex");

  // Conditional Variables 
  for (int i = 0; i < 4; i++)
  {
    go[i] = cv_create("go");
    if (go[i] == NULL)
      panic("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  }
  
  // Boolean setup 
  flow_direction = -1; 
  intersection_vehicles = 0;
  for (int i = 0; i < 4; i++)
  {
    waiting_from[i] = 0;
  }
  
}

int 
dir_index(Direction d)
{
  switch (d)
    {
    case north:
      return 0;
      break;
    case east:
      return 1;
      break;
    case south:
      return 2;
      break;
    case west:
      return 3;
      break;
    }
  // This should never happen 
  return 0;
}

// Haoda's DIRECTION CHECKING FUNCTIONS (USELESS IN THE ONE DIRECTION SYSTEM)
/*
bool 
all_false_except(Direction d, int volatile arr[])
{
  int exception = dir_index(d);
  for (int i = 0; i < 4; i++)
  {
    if (i != exception && arr[i] > 0)
      return false;
  }
  return true;
}
bool 
left_and_right_all_false(Direction d, int volatile arr[])
{
  int dir_i = dir_index(d);
  for (int i = 0; i < 3; i++)
  {
    //
    if (((dir_i - i != 0) || dir_i - i != 2 || dir_i - i != 2) // If direction is not parallel
      && arr[i] > 0) // And it's true (occupied)
      return false;
  }
  return true;
}
bool 
check_intersection(Direction origin, Direction destination)
{
  // Condition 1: Are all cars entering from the same direction?
  if (all_false_except(origin, occupied_origins))
    return true; 
  
  // Condition 1|2: Are all cars going in PARALLEL directions? 
  //               (i.e. origins and destinations are either same or opposites)
  if (left_and_right_all_false(origin, occupied_origins) && // origins are all parallel to current origin
    left_and_right_all_false(origin, occupied_destinations) && // dest are all parallel to current origin
    ((dir_index(destination) - dir_index(origin) == 2) || (dir_index(destination) - dir_index(origin) == -2)))
    return true; 
  
  // Condition 3: Do all cars have different destinations? 
  if (occupied_destinations[dir_index(destination)] == false
    // and is there a car making a right turn? 
   && (dir_index(origin) - dir_index(destination) == 1
   || dir_index(origin) - dir_index(destination) == -3
   //|| right_turns > 0))
   || intersection_vehicles - right_turns <= 0)) 
   // For one right turn to exist between every pair of vehicles, there cannot be 2 or more vehicles without a right turn
   // On a similar note, if you are about to enter an intersection, unless you are making a right turn, 
   // all vehicles in the intersection must be making a right turn
  {
    int diff = dir_index(origin) - dir_index(destination);
    bool isDiffDest = occupied_destinations[dir_index(destination)] == false;
    (void) diff; (void) isDiffDest;
    return true;
  }
   // If NONE of these conditions are satisfied, then you're done m8
   return false;
}
*/

void 
update_flow()
{
  int count = 0; // Prevent infinite loops
  do 
  {
    count++; // Prevent infinite loops
    flow_direction++; 
    if (flow_direction > 3) flow_direction = 0;
  } while (waiting_from[flow_direction] <= 0 && count <= 16);
  
  if (count >= 16)
    flow_direction = -1; // RESET the ROAD
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
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);
  
  for (int i = 0; i < 4; i++)
  {
    KASSERT(go[i] != NULL);
    cv_destroy(go[i]);
  }
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
  lock_acquire(mutex);
  int i_origin = dir_index(origin);
  bool is_waiting = false;
  
  if (flow_direction == -1) // This is the FIRST CAR to enter
  {
    flow_direction = i_origin; 
  }
  
  if (flow_direction != i_origin) // The flow direction is NOT in this car's favor
  {
    is_waiting = true;
    waiting_from[i_origin]++; 
    cv_wait(go[i_origin], mutex); 
  }
  
  // At this point, go has been signaled
  if (is_waiting)
    waiting_from[i_origin]--;
  intersection_vehicles++; 
  
  (void) destination; // Prevent stupid warnings 
  lock_release(mutex);
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
  lock_acquire(mutex);
  intersection_vehicles--; 
  if (intersection_vehicles == 0)
  {
    update_flow();
    if (flow_direction != -1)
      cv_broadcast(go[flow_direction], mutex);
  }
  
  (void) origin; // Prevent stupid warnings
  (void) destination;
  lock_release(mutex);
}
