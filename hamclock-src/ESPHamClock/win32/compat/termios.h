#ifndef HC_TERMIOS_H
#define HC_TERMIOS_H
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int tcflag_t;
typedef unsigned int speed_t;
struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    unsigned char c_cc[32];
    speed_t c_ispeed;
    speed_t c_ospeed;
};
#define IGNBRK  0000001
#define IGNPAR  0000004
#define ISTRIP  0000040
#define IGNCR   0000200
#define CS8     0000060
#define CREAD   0000200
#define CLOCAL  0004000
#define ICANON  0000002
#define B4800   4800
#define B9600   9600
#define B38400  38400
#define B1000000 1000000
#define TCSANOW 0
#define TCSAFLUSH 1
#define TCIOFLUSH 2
#define VMIN 6
#define VTIME 5
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int actions, const struct termios *t);
int tcflush(int fd, int queue);
int cfsetspeed(struct termios *t, speed_t speed);
int cfsetispeed(struct termios *t, speed_t speed);
int cfsetospeed(struct termios *t, speed_t speed);
int cfmakeraw(struct termios *t);
int tcdrain(int fd);
#ifdef __cplusplus
}
#endif
#endif
