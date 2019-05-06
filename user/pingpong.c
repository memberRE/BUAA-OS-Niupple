// Ping-pong a counter between two processes.
// Only need to start one of these -- splits into two with fork.

#include "lib.h"


void
umain(void)
{
	u_int who, i;

	int counter = 0;
	//syscall_panic("QAQ\n");
	if ((who = tfork()) != 0) {
		writef("father has returned\n");
		// get the ball rolling
		writef("\n@@@@@send 0 from %x to %x\n", syscall_getenvid(), who);
		ipc_send(who, 0, 0, 0);
		//user_panic("&&&&&&&&&&&&&&&&&&&&&&&&m");
	}
	writef("I am %d, who is %d\n", syscall_getenvid(), who);

	for (;;) {
		writef("%x am waiting.....\n", syscall_getenvid());
		i = ipc_recv(&who, 0, 0);

		writef("%x got %d from %x, counter = %d\n", syscall_getenvid(), i, who, ++counter);

		//user_panic("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&");
		if (i == 10) {
			return;
		}

		i++;
		writef("\n@@@@@send %d from %x to %x, counter = %d\n",i, syscall_getenvid(), who, ++counter);
		ipc_send(who, i, 0, 0);

		if (i == 10) {
			return;
		}
	}

}

