
// Kilo: A text-editor written in <1000 lines of C
// as originally written by Antirez

#include <unistd.h>
#include <stdio.h>

// ECHO, ICANON, ISIG, IXON, IEXTEN
#include <termios.h>

// atexit
#include <stdlib.h>

// iscntrl
#include <ctype.h>

struct termios orig_termios;

void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
};

void enableRawMode(){

	tcgetattr(STDIN_FILENO, &orig_termios);

	// called after returning from main
	// comes from stdlib
	atexit(disableRawMode);
	
	struct termios raw = orig_termios;

	// c_lflag: miscellaneous flag bits
	// disable:
	// echoing (4th bit) and
	// canonical mode (2nd bit; waiting for return)
	// signalling (SIGINT, SIGSTP)
	// legacy ctrl-V funk (literal esc sequences)
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

	// likewise disable
	// ctrl-S = XOFF - stops data transmission
	// ctrl-Q = XON - opens transmission again
	// converting \r (13) to \n (10)
	raw.c_iflag &= ~(ICRNL | IXON);

	// disable
	// output conversion of \n to \r\n
	raw.c_oflag &=~(OPOST);

	// applies the new settings;
	// waits for pending output, discards input
	// (TCSAFLUSH)
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {

	enableRawMode();

	char c;
	//printf("%x\n", ICANON);

	// Note: STDIN_FILENO is a macro thatll evaluate to 0;
	// the FD/syscall value for stdin
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 's'){

		if(iscntrl(c)){
			printf("%d (ctrl)\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);
		}

	}

	printf("\n Exit (s)\n");
	return 0;
}