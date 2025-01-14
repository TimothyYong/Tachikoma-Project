#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include "serial.h"

#define INPUT_DIR "/dev/"
static const char *PREFIXES[] = {
  "ttyACM",
  NULL
};

static int _serial_setattr(serial_t *connection);
static void _serial_update(serial_t *connection);
static char tempbuf[SWREADMAX + 1];

/** Connect to a serial device.
 *  @param connection
 *    a pointer to the serial struct
 *  @param port
 *    a portname; if NULL, will open a random port
 *  @param baudrate
 *    the bits per second of information to transmit/receive
 *  @return 0 on success, -1 on failure
 */
int serial_connect(serial_t *connection, char *port, int baudrate) {
  connection->connected = 0;
  if (port) {
    connection->port = (char *)malloc((strlen(port) + 1) * sizeof(char));
    strcpy(connection->port, port);
    if ((connection->fd = open(connection->port, O_RDWR)) == -1) {
      goto error;
    }
  } else {
    DIR *dp;
    struct dirent *ent;
    char hasPossibleSerial;
    if (!(dp = opendir(INPUT_DIR))) {
      fprintf(stderr, "[SERIAL] Cannot find directory %s to open serial connection\n", INPUT_DIR);
      return -1;
    }
    while ((ent = readdir(dp))) {
      const char *prefix;
      int i;
      hasPossibleSerial = 0;
      for (prefix = PREFIXES[(i = 0)]; prefix != NULL; prefix = PREFIXES[++i]) {
        if (strstr(ent->d_name, prefix)) {
          connection->port = (char *)malloc((strlen(INPUT_DIR) + strlen(ent->d_name) + 1) * sizeof(char));
          sprintf(connection->port, "%s%s", INPUT_DIR, ent->d_name);
          if ((connection->fd = open(connection->port, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
            free(connection->port);
            connection->port = NULL;
          } else {
            hasPossibleSerial = 1;
            break;
          }
        }
      }
      if (hasPossibleSerial) {
        break;
      }
    }
    if (!hasPossibleSerial) {
      fprintf(stderr, "[SERIAL] Cannot find a serial device to open\n");
      return -1;
    }
  }

  /* set connection attributes */
  connection->baudrate = baudrate;
  connection->parity = 0;
  if (_serial_setattr(connection) == -1) {
    goto error; /* possible bad behavior */
  }
  tcflush(connection->fd, TCIFLUSH);
  tcflush(connection->fd, TCOFLUSH);
  connection->connected = 1;
  memset(connection->buffer, 0, SWBUFMAX + 1);
  memset(connection->readbuf, 0, SWREADMAX + 1);
  connection->readAvailable = 0;

  printf("[SERIAL] Connected to %s\n", connection->port);

  return 0;

error:
  fprintf(stderr, "[SERIAL] Cannot connect to the device on %s\n", connection->port);
  connection->connected = 0;
  if (connection->fd != -1) {
    close(connection->fd);
  }
  connection->fd = -1;
  if (connection->port) {
    free(connection->port);
  }
  connection->port = NULL;
  return -1;
}

/** Helper method to set the attributes of a serial connection,
 *  particularly for the arduino or similar device.
 *  @param connection
 *    the serial port to connect to
 *  @return 0 on success, -1 on failure
 */
static int _serial_setattr(serial_t *connection) {
  struct termios tty;
  if (tcgetattr(connection->fd, &tty) == -1) {
    return -1;
  }

  // get rid of old
  memset(&tty, 0, sizeof(struct termios));
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;
  tcsetattr(connection->fd, TCSANOW, &tty);
  tcsetattr(connection->fd, TCSAFLUSH, &tty);
  fcntl(connection->fd, F_SETFL, O_NONBLOCK);

  // set new attributes
  memset(&tty, 0, sizeof(struct termios));
  tty.c_cflag = CS8 | CREAD | CLOCAL;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 5;
  cfsetospeed(&tty, connection->baudrate);
  cfsetispeed(&tty, connection->baudrate);
  if (tcsetattr(connection->fd, TCSANOW, &tty) == -1) {
    return -1;
  }
  return 0;
}

/** Method to update the readbuf of the serial communication,
 *  as well as the connection itself.
 *  @param connection
 *    the serial struct
 *  @note
 *    the packets will be read in the following format:
 *    data\n
 */
static void _serial_update(serial_t *connection) {
  int bytesRead;
  int bytesStored;
  unsigned char analyzeBuffer;

  /* dynamically reconnect the device */
  if (access(connection->port, F_OK) == -1) {
    if (connection->connected) {
      connection->connected = 0;
      connection->fd = -1;
    }
  } else {
    if (!connection->connected) {
      if ((connection->fd = open(connection->port, O_RDWR | O_NOCTTY | O_NDELAY)) != -1) {
        if (_serial_setattr(connection) == 0) {
          connection->connected = 1;
        } else {
          close(connection->fd);
          connection->fd = -1;
        }
      }
    }
  }
  if (!connection->connected) {
    return;
  }

  /* update buffer constantly (be careful of overflow!) */
  analyzeBuffer = 0;
  while ((bytesRead = read(connection->fd, tempbuf, SWREADMAX)) > 0) {
    if (bytesRead > 0) {
      analyzeBuffer = 1; /* turn on buffer analysis signal */
    }
    tempbuf[bytesRead] = '\0';
    bytesStored = strlen(connection->buffer); /* no \0 */
    while (bytesStored + bytesRead >= SWBUFMAX) {
      /* shorten it by only half of the readmax value */
      bytesStored -= SWREADMAX / 2;
      memmove(connection->buffer, &connection->buffer[SWREADMAX / 2],
          (bytesStored + 1) * sizeof(char));
    }
    strcat(connection->buffer, tempbuf);
  }

  if (analyzeBuffer) {
    char *start_index, *end_index;
    size_t nbytes;
    if ((end_index = strrchr(connection->buffer, '\n'))) {
      end_index[0] = '\0';
      end_index = &end_index[1];
      start_index = strrchr(connection->buffer, '\n');
      start_index = start_index ? &start_index[1] : connection->buffer;
      nbytes = (size_t)end_index - (size_t)start_index;
      memcpy(connection->readbuf, start_index, nbytes * sizeof(char));
      connection->readbuf[nbytes + 1] = '\n'; /* put the \n back */
      connection->readbuf[nbytes + 2] = '\0';
      memmove(connection->buffer, end_index,
          (strlen(end_index) + 1) * sizeof(char));
      connection->readAvailable = 1;
    }
  }
}

/** Read a string from the serial communication link.
 *  @param connection
 *    the serial connection to read a message from
 *  @return the readbuf if a message exists, else NULL
 */
char *serial_read(serial_t *connection) {
  _serial_update(connection);
  if (connection->readAvailable) {
    connection->readAvailable = 0;
    return connection->readbuf;
  } else {
    return NULL;
  }
}

/** Write a message to the serial communication link.
 *  @param connection
 *    the serial communication link to write to
 *  @param message
 *    the message to send over to the other side
 *  @note
 *    be sure the message has a '\n' chararacter
 */
void serial_write(serial_t *connection, char *message) {
  if (connection->fd != -1) {
    if (write(connection->fd, message, strlen(message)) != -1) {
//      printf("[SERIAL] write [%s]: %s\n", connection->port, message);
    }
  }
}

/** Disconnect from the USB Serial port.
 *  @param connection
 *    A pointer to the serial struct.
 */
void serial_disconnect(serial_t *connection) {
  /* clean up */
  if (!connection->connected) {
    return;
  }
  if (connection->fd != -1) {
    close(connection->fd);
  }
  if (connection->port != NULL) {
    free(connection->port);
  }
  memset(connection, 0, sizeof(serial_t));
  connection->fd = -1;
}
