
// Kilo: A text-editor written in <1000 lines of C
// as originally written by Antirez

#include <unistd.h>

// perror
#include <stdio.h>

// ECHO, ICANON, ISIG, IXON, IEXTEN
#include <termios.h>

// atexit, exit
#include <stdlib.h>

// iscntrl
#include <ctype.h>

// errno, EAGAIN
#include <errno.h>

struct termios orig_termios;

void die(const char *s){
	// looks up global errno variable
	// converts to descriptive error message
	perror(s);
	exit(1);
}

void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
};

void enableRawMode(){

	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1){
		die("tcgetattr");
	}

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
	// IXON: ctrl-S = XOFF - stops data transmission
	// 		 ctrl-Q = XON - opens transmission again
	// ICRNL: converting \r (13) to \n (10)
	// and legacy support
	// BRKINT: BREAK support (breaks a telegraph circuit)
	// INPCK: parity checking
	// ISTRIP: strip off 8th bit
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	
	// to carry on a tradition of turning off certain flags:
	// disable CS8: sets the character size to 8 bits/byte
	raw.c_cflag &= ~(CS8); // 1011 1111 --> 1000 1111

	// disable
	// output conversion of \n to \r\n
	raw.c_oflag &=~(OPOST); // 0000 0101 --> 0000 0100
	
	// set time out that returns if no input
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	// applies the new settings;
	// waits for pending output, discards input
	// (TCSAFLUSH)
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
		die("tcsetattr");
	}
}

int main() {

	enableRawMode();

	char c = '\0';

	while(1) {

		// Note: STDIN_FILENO is a macro thatll evaluate to 0;
		// the FD/syscall value for stdin		
		
		if (read(STDIN_FILENO, &c, 1)  == -1 && errno != EAGAIN){
			die ("read");
		}

		if(iscntrl(c)){
			printf("%d (ctrl)\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);
		}

		// exit
		if (c == 's') break;

	}

	printf("\n Exit (s)\n");
	return 0;
}