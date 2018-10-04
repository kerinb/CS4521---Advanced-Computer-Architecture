int MAX_NUM_THREADS = 2;
byte choosingThread[NUM_THREADS];
byte ticketNumber[NUM_THREADS];
int test_in_critical_section;

proctype customer(){
	byte id = _pid - 1; 
	int numIters;
	for(numIters : 0 .. 2){
		// Comment the line below in order to break safety/mutual exclusion property
		choosing[id] = 1; // Threads request lock
		
		int i, max = 0;	
		
		for(i : 0 .. (NUM_THREADS - 1)){ // loop across all threads that are active.
			if 			 // max is set to 0, if there is a ticketNumber that is greater than max, max = ticketNumber
			:: ticketNumber[i] > max -> max = ticketNumber[i];
			:: else;
			fi;
		}

		// Take a ticket
		ticketNumber[id] = max + 1;
		choosing[id] = 0;

		int j;
		for(j : 0 .. (NUM_THREADS - 1)){
			do
			:: (choosing[j] == 0) -> break;
			od;

			((ticketNumber[j] == 0) || ((ticketNumber[j] >= ticketNumber[id]) && ((ticketNumber[j] != ticketNumber[id] || (j >= id))));
		}	

		// This is the critical section of the code! 
		// It will do some - call a function/ update a data stucture ETC
		test_in_critical_section++;
		assert(test_in_critical_section);
		test_in_critical_section--;
		// Release the lcok by clearing our ticket in ticketNumber
		ticketNumber[id] = 0;
	}
	printf("Process %d has competed %d iterations of this loop", id, numIters);
}

init{
	run customer();
	run customer();
	(_nr_pr == 1);
}
