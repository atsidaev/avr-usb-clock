#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <stdio.h>
#include <string.h>

#define CHAR_WAIT_TIMEOUT 0x1FFFF

int fd;

int send(char ch)
{
	write(fd, &ch, 1);
	
	char buf[100];
	int n = -1;
	
	// Wait until char appear
	
	int counter = 0; /* Timeout (kinda) to prevent infinite loop */
	while (n <= 0 && counter++ < CHAR_WAIT_TIMEOUT)
		n = read(fd, buf, sizeof(buf));

	if (counter == CHAR_WAIT_TIMEOUT)
		return 0;

	// Wait until chars dissapear

	while (n > 0)
		n = read(fd, buf, sizeof(buf));
	
	if (ch != buf[0])
	{
		printf("Incorrect byte read");
		return 0;
	}
	return 1;
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("Usage: usbclock <device> <command>\n");
		return 1;
	}
	
	char* device = argv[1];
	char* command = argv[2];
	
	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	
	if (fd != -1)
	{
		if (flock(fd, LOCK_EX) == 0)
		{
			fcntl(fd, F_SETFL, 0);
			
			struct termios tio, options;

			tcgetattr(fd, &options);

			options.c_cflag     |= (CLOCAL | CREAD);
			options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
			options.c_oflag     &= ~OPOST;
			options.c_cc[VMIN]  = 0;
			options.c_cc[VTIME] = 0;

			cfsetospeed(&options, B9600);
			cfsetispeed(&options, B9600);

			tcsetattr(fd, TCSANOW, &options);

			int line_length = strlen(command);
			int i;
			
			for (i = 0; i < line_length; i++)
				if (!send(command[i]))
				{
					printf("Error!");
					return 1;
				}
			
			flock(fd, LOCK_UN);
		}
		else
		{
			perror("Device lock failed");
			close(fd);
			return 1;
		}

	}
	else
	{
		perror("Device open failed");
		return 1;
	}
	close(fd);
	
	return 0;
}
