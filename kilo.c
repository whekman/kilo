
// Kilo: A text-editor written in <1000 lines of C
// as originally written by Antirez

#include <unistd.h>
#include <stdio.h>
#include <termios.h>

void enableRawMode() {
	struct termios raw;

	tcgetattr(STDIN_FILENO, &raw);

	// bitwise AND with the bitwise NOT of ECHO
	// sets the bit of echo to 0 (4th bit)
	raw.c_lflag &= ~(ECHO);

	// setting with TCSAFLUSH
	// waits for pending output, discards input
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {

	enableRawMode();

	char c;

	// Note: STDIN_FILENO is a macro thatll evaluate to 0;
	// the FD/syscall value for stdin
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');

	printf("\nI got:  %c\n", c);
	return 0;
}