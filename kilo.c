
// Kilo: A text-editor written in <1000 lines of C
// Originally written by Antirez
// Made into a neat tutorial by Paige Rutten.
// This is my version (willemhekman.nl)
// which involves lots of comments to help and understand
// the program.

/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

/*** defines ***/

#define KILO_VERSION "0.0.1 by W. Hekman - Exit = Ctrl + K"

#define CTRL_KEY(k) ((k) & 0x1f)
// gets the control value of a character
// by setting the top 3 bits to 0
// e.g a = 97 = 0100 0001 --> ctrl+a = 01 = 000000001

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

/*** data ***/


// stores a row's characters
typedef struct erow {
  int size;
  char *chars;
} erow;

// stores the editor's state
struct editorConfig {
  int cx, cy; // cursor position
  int screenrows;
  int screencols;
  int numrows;
  erow row;
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
int editorReadKey() {
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

  // handle length 3 esc-seqs
  // arrow-keys eg esc[A
  // page-up/down ia esc[5~, esc[6~

  if (c == '\x1b') {
    char seq[3];

    // read in the start of the sequence
    // if user presses esc key and we time-out
    // just return ESC
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    // set-up aliassing
    if (seq[0] == '[') {

      // [0 -- [9
      if (seq[1] >= '0' && seq[1] <= '9') {

        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

        // [0~ -- [9~
        if (seq[2] ==  '~') {
          switch (seq[1]) {
          case '1': return HOME_KEY;
          case '3': return DEL_KEY;
          case '4': return END_KEY;
          case '5': return PAGE_UP;
          case '6': return PAGE_DOWN;
          case '7': return HOME_KEY;
          case '8': return END_KEY;
          }
        }

      } else {

      // [A -- [F
      switch (seq[1]) {
      case 'A': return ARROW_UP;
      case 'B': return ARROW_DOWN;
      case 'C': return ARROW_RIGHT;
      case 'D': return ARROW_LEFT;
      case 'H': return HOME_KEY;
      case 'F': return END_KEY;

      }

    }

    // [OH ; [OF
    } else if (seq[0] == 'O') {
      switch (seq[1]) {

      case 'H': return HOME_KEY;
      case 'F': return END_KEY;        
      
      }


    
    // if we do not recognize the sequence we just return ESC
    return '\x1b';

    } else {
      return c;
    }
  }
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

/*** fileio ***/

// open a file
void editorOpen(char *filename) {

  FILE *fp = fopen(filename, "r");
  if (!fp) die ("fopen");

  // buffer - setting both to 0 makes getline set them appropriately
  char *line = NULL;
  ssize_t linecap = 0;
  ssize_t linelen;

  // get first line
  linelen = getline(&line, &linecap, fp);

  // getline() reads an entire line from stream, storing the address
  // of the buffer containing the text into *lineptr.  The buffer is
  // null-terminated and includes the newline character, if one was
  // found.

  // If *lineptr is set to NULL before the call, then getline() will
  // allocate a buffer for storing the line.  This buffer should be
  // freed by the user program even if getline() failed.

  // On success, getline() and getdelim() return the number of
  // characters read, including the delimiter character, but not
  // including the terminating null byte ('\0').  This value can be
  // used to handle embedded null bytes in the line read.

  if (linelen != -1) {

    // decrease linelen till we hit eol
    while (linelen > 0 && (line[linelen -1] == '\n') || (line[linelen -1] == '\r') ) {
      linelen--;
    }

    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1;
  }

  free(line);
  fclose(fp);

}

/*** append buffer ***/

// buffer that holds the output to write to the screen
struct abuf {
  char *b; // points to the start of the buffer
  int len; // the length of the buffer
};

// acts as a constructor for abuf
#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len) {

  // new points to the start of the memory block
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

void editorDrawRows(struct abuf *ab) {

  int y;
  
  for (y = 0; y < E.screenrows; y++) {
  
    // after the end of the text buffer 
    if (y >= E.numrows) {
  
      // welcome message at 1/3 if no file
      if (E.numrows == 0 && y == E.screenrows / 3) {

        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);

      // not at 1/3
      } else {
        abAppend(ab, "~", 1);
      }

    // part of the text buffer
    } else {

      // truncate erow to fit width of screen
      int len = E.row.size;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, E.row.chars, len);
    }

    // erase till end of line
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

// VT100 stuff
// clears screen and sets cursor top left
void editorRefreshScreen() {

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // hide cursor
  abAppend(&ab, "\x1b[H", 3); // top-left


  // redraw using the cursor

  editorDrawRows(&ab); // tildes + rows

  // place the cursor
  char buf[32];
  
  // esc-seq to place the cursor;
  // +1 to convert to 1 indexing
  
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // show cursor

  write(STDOUT_FILENO, ab.b, ab.len);

  abFree(&ab);

}

/*** input ***/

void editorMoveCursor(int key) {

  // NOTE:
  // moving right is x++
  // moving down is y++

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;  
    }
    break;
  case ARROW_RIGHT:
    if (E.cx != E.screencols - 1) {
      E.cx++;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy != E.screenrows - 1) {
      E.cy++;
    }
    break;
  }
}


void editorProcessKeypress() {

  int c = editorReadKey();

  switch (c) {

    // exit
    case CTRL_KEY('k'):
      write(STDOUT_FILENO, "\x1b[2J", 4); // flush
      write(STDOUT_FILENO, "\x1b[H", 3); // move top-left
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      E.cx = E.screencols - 1;
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      { // little hack to reuse effect of arrow keys
        int times = E.screenrows;
        while (times-- >= 0)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP: ARROW_DOWN);
      }

    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

  }
}

/*** init ***/

void initEditor() {

  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  if (argc >= 2) {
    editorOpen(argv[1]);  
  }
  
  while (1) {
    editorRefreshScreen();

    // will hang in the following routine
    // untill we get a key-press
    editorProcessKeypress();
  }

  return 0;
}
