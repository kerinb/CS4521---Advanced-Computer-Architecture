//
// Peterson lock
//

// bool flag[2];
byte last;

active[2] proctype acquire(int id){
	byte i = _pid;
	byte j = 1 - i;

again:

	// flag[i] = 1;
	last = i;

	// (flag[i] == 1 
	( last == j); // wait until true

	// flag[i] = 0;

	goto again
}

