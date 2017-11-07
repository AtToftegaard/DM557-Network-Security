#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "rdt.h"
#include "subnetsupport.h"
#include "subnet.h"
#include "fifoqueue.h"
#include "debug.h"
#include "eventDefinitions.h"

FifoQueue from_transport_layer_queue;
FifoQueue for_transport_layer_queue;
mlock_t *transport_layer_lock;

long int events_we_handle;

void* give_me_message(int i){    
      char *msg;
      msg = (char *) malloc(10*sizeof(char));
      sprintf(msg,"%d" , i);
      return (void*) msg;
}

void fakeTransportLayer(){

   int Receiver = 3;
   char *buffer;
   int i;
   packet *p;

   initialize_locks_and_queues();

    printf("%s\n", "setting up messages");
    // Setup some messages
    for( i = 0; i < 1; i++ ) {
      p = (packet *) malloc( sizeof(packet) );
      buffer = malloc(sizeof(char *) * (SIZE_OF_SEGMENT));
      p->globalDestination = Receiver;//
      p->globalSender = ThisStation; 

      sprintf( buffer, "D: %d", i );
      strcpy(p->data, buffer);
      EnqueueFQ( NewFQE( (void *) p ), from_transport_layer_queue );
      Signal(transport_layer_ready, give_me_message(3) );
    }
    if (i == 1) {
      sleep(10);
      logLine(succes, "Stopping - Sent %d messages to station %d\n", i, Receiver );
      /* A small break, so all stations can be ready */
      Stop();
    }
}

void initialize_locks_and_queues(){
   
   transport_layer_lock = malloc(sizeof(mlock_t));
   Init_lock(transport_layer_lock);

   from_transport_layer_queue = InitializeFQ();
   for_transport_layer_queue = InitializeFQ();
}

int forward(int toAddress){
   return ((ThisStation%4)+1);
}

void network_layer_main_loop(){

//int globalDestination;
packet *p;
FifoQueueEntry e;
//The main loop of the network layer would likely look something along the lines of: (pseudo code)
event_t event;
events_we_handle = transport_layer_ready | network_layer_allowed_to_send | data_for_network_layer;

p = (packet *) malloc(sizeof(packet));

   while( true ){
   	// Wait until we are allowed to transmit
   	Wait(&event, events_we_handle);
   	switch(event.type) {
   		case network_layer_allowed_to_send:
            // Layer below says it is ok to send
            // Lets send if there is something to send to that neighbour
            Lock( network_layer_lock );
            if ((EmptyFQ(from_network_layer_queue)) == 0){ // 1 = tom, 0 = ikke tom
               // Signal element is ready
               Signal(network_layer_ready, NULL);
            }
            Unlock( network_layer_lock );

   			break;
   		case data_for_network_layer:
			// Layer below has provided data for us - lets process
			// Either it should go up or be shipped on
         // Retrieve from link_layer
            printf("%s %d\n", "data_for_network_layer", __LINE__);
            Lock(network_layer_lock);
            e = DequeueFQ(for_network_layer_queue);
            Unlock(network_layer_lock);

            if(!e) {
               logLine(error, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (packet *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
             
               if (p->globalDestination == ThisStation){ // This is final destination
                  Lock(transport_layer_lock);
                  EnqueueFQ( (void *) p, for_transport_layer_queue);
                  Unlock(transport_layer_lock);
               } else {                                  // forward it
                  Lock(network_layer_lock);
                  EnqueueFQ( (void *) p, from_network_layer_queue);
                  Signal(network_layer_ready, give_me_message(forward(0)));
                  Unlock(network_layer_lock);
               }
            }
   			break;
   		case transport_layer_ready:
   		    // Data arriving from above - do something with it
            printf("%s\n", "BEGIN transport_layer_ready");
            Lock(transport_layer_lock);
            e = DequeueFQ(from_transport_layer_queue);
            Unlock(transport_layer_lock);
            if(!e) {
               logLine(succes, "ERROR: We did not receive anything from the queue, like we should have\n");
            } else {
               memcpy(p, (char *)ValueOfFQE( e ), sizeof(packet));
               free( (void *)ValueOfFQE( e ) );
               DeleteFQE( e );
            }                                 
            p->globalDestination = (int) event.msg; //nothing more than int in message, ever.
            p->globalSender = ThisStation;
            p->kind = DATAGRAM;
            Lock(network_layer_lock);
            EnqueueFQ( NewFQE( (void *) p ), from_network_layer_queue );
            Signal(network_layer_ready, give_me_message(forward(0)));
            Unlock(network_layer_lock);
            printf("%s\n","END transport_layer_ready" );
            break;
            }
   }
}
void signal_link_layer_if_allowed(int address){

}

