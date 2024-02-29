
// Kilo: A text-editor written in <1000 lines of C
// Originally written by Antirez
// Made into a neat tutorial by Paige Rutten.
// This is my version (willemhekman.nl)
// which involves lots of comments to help and understand
// the program.

/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)
// gets the control value of a character
// by setting the top 3 bits to 0
// e.g a = 97 = 0100 0001 --> ctrl+a = 01 = 000000001

/*** data ***/

struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {

    // see refresh screen
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

    // looks up global errno variable
    // converts to descriptive error message
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {

  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  // DISABLE:
  // ECHO: echoing (4th bit)
  // ICANON: canonical mode = waiting for return (2nd bit)
  // ISIG: signalling (SIGINT, SIGSTP)
  // IEXTEN: legacy ctrl-V stuff (literal esc sequences)
  // IXON: ctrl-S = XOFF - stops data transmission
  //       ctrl-Q = XON - opens transmission again
  // ICRNL: converting \r (13) to \n (10)
  // BRKINT: BREAK support (breaks a telegraph circuit)
  // INPCK: parity checking
  // ISTRIP: strip off 8th bit
  // CS8: sets the character size to 8 bits/byte
  // OPOST: output conversion of \n to \r\n
  // set time out for returns of read() if no input

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // flush and apply the new settings
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// wait for a single keypress and return it
char editorReadKey() {
  int nread;
  char c;

  // Repeatedly read from stdin
  // till you get 1 character (at first returns 0)
  //          Note: STDIN_FILENO is a macro
  //          that'll evaluate to 0 = stdin syscall      
  // If somehow first nread == 1 but then == -1
  // stop the program.
  //          Note: I had a silly bug here that cost
  //          me an hour by having != -1 instead of 1
  //          causing an infinite while loop.

  while((nread = read(STDIN_FILENO, &c, 1))  != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {

  // buffer to hold the returned stdin
  char buf[32];
  unsigned int i = 0;

  // this escape sequence puts the cursor position
  // into stdin as: \x1b[24;80R
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  
  // zero terminate the string
  buf[i] = '\0';

  // assert that we got back a control sequence
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;

  // scan in the value
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  printf("\r\n rows, cols: '%d','%d'\r\n", *rows, *cols);

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

// buffer that holds the state of the screen
struct abuf {
  char *b;
  int len;

}

// acts as a constructor for abuf
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  // copy in the string into the memory
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

// draw some tildes
void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {

    write(STDOUT_FILENO, "~", 1);

    if (y < E.screenrows -1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }

  }
}

// VT100 stuff
// clears screen and sets cursor top left
void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {

  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
