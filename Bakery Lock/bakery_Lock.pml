int MAX_NUM_THREADS = 2;
byte choosingThread[MAX_NUM_THREADS];
byte ticketNumber[MAX_NUM_THREADS];

proctype customer(){
	byte id = _pid - 1; 
	
	int numIters;
	for(numIters : 0 .. 2){
		choosingThread[id]=1; // Threads request lock
		
		int i, max=0;	
		
		for(i : 0 .. (MAX_NUM_THREADS - 1)){ // loop across all threads that are active.
			if 			 // max is set to 0, if there is a ticketNumber that is greater than max, max = ticketNumber
			:: ticketNumber[i] > max -> max = ticketNumber[i];
			:: else;
			fi;
		}

		// Take a ticket
		ticketNumber[id] = max + 1;
		choosingThread[id] = 0;

		int j;
		for(j : 0 .. (MAX_NUM_THREADS - 1)){
			// Wait for our turn to come! 
			// if choosing[j] == 0, then we are on another thread and we must wait until it is our turn (choosingThread[j] == 1) threads turn to execute
			do
			:: (choosingThread[j] == 0) -> break;
			od;

			// dont continue UNLESS ticketNUmber[j] = 0 OR if (ticketNumber[j] >= ticketNumber[id] AND (ticketNumber[j] != ticketNumber[id]) OR (j >= id))
			((ticketNumber[j] == 0) || ((ticketNumber[j] >= ticketNumber[id]) && ((ticketNumber[j] != ticketNumber[id] || (j >= id))));
		}	

		// This is the critical section of the code! 
		// It will do some - call a function/ update a data stucture ETC
		
		// Release the lcok by clearing our ticket in ticketNumber
		ticketNumber[id] = 0;
	}
	printf("Process %d has competed %d iterations of this loop", id, numIters);
}

init {
	run customer();
	run customer();
	(_nr_pr == 1);
}
