
// Kilo: A text-editor written in <1000 lines of C
// as originally written by Antirez

// STDIN, STDOUT_FILENO
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

// ioctl..
#include <sys/ioctl.h>

// the value of a char k
// with the top 3 bits set to 0
// is the ctrl value of the char
// eg ascii a = 97 = 0100 0001
// and ctrl+a = 01 = 000000001
#define CTRL_KEY(k) ((k) & 0x1f)

/*** LL Terminal Stuff ***/

struct editorConfig {
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

////----------

void die(const char *s){

	// see refresh screen
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	// looks up global errno variable
	// converts to descriptive error message
	perror(s);
	exit(1);
}


int getWindowSize(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
		ws.ws_col == 0){
		return -1;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

void initEditor(){
	if(getWindowSize(&E.screenrows, &E.screencols) == -1){
		die("getWindowSize");
	}
}

void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
};

void enableRawMode(){

	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){
		die("tcgetattr");
	}

	// called after returning from main
	// comes from stdlib
	atexit(disableRawMode);
	
	struct termios raw = E.orig_termios;

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
	// and other 1960s stuff:
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
	
	// set time out for returns of read() if no input
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	// flush  and apply the new settings
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
		die("tcsetattr");
	}
}

// wait for a single keypress and return it
char editorReadKey(){
	int nread;
	char c;

	// Try read stdio in till valid, return c
	// 		Note: STDIN_FILENO is a macro thatll evaluate to 0;
	// 		the FD/syscall value for stdin		
	while((nread = read(STDIN_FILENO, &c, 1))  != -1) {

		if (nread == -1 && errno != EAGAIN) die ("read");
	}

	return c;
	
}

/*** input ***/

void editorProcessKeypress(){

	char c = editorReadKey();

	switch(c) {
		case CTRL_KEY('s'):
			// see refresh screen
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** output ***/


// some leading tildes
void editorDrawRows(){
	int y;
	for (y = 0; y < E.screenrows; y++){
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

// VT100 stuff
// clears screen and sets cursor top left
void editorRefreshScreen() {
	
	// \x1b[ = escape
	// J2 = erase whole screen
	write(STDOUT_FILENO, "\x1b[2J", 4);
	
	// move cursor top left 1;1H = default
	// generally y;xH sets cursor at x,y
	write(STDOUT_FILENO, "\x1b[H", 3);

	// writes tildes in the side
	editorDrawRows();

	// moves back the cursor top left
	write(STDOUT_FILENO, "\x1b[H", 3);


}


int main(){

	enableRawMode();

	while(1){

		editorRefreshScreen();
		editorProcessKeypress();
	
	}

	printf("\n Exit (s)\n");
	return 0;
}