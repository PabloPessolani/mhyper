#include <lib.h>
#include <string.h>

PUBLIC int _mhdebug(text, code)
char *text;
int code;
{
  int status;
  char ltext[sizeof(message)+1];

  strncpy(&ltext[sizeof(int)], text, sizeof(message)-sizeof(int));
  status = _hdebug(ltext, code);
  return(status);
}