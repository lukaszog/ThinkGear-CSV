#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define THINKGEAR_SYNC 0xAA
#define THINKGEAR_MAX_DATA_SIZE 169
#define THINKGEAR_EXCODE 0x55
#define SINGLE_BYTE_THINKGEAR_CODE(code) (code <= 0x7F)
#define MULTI_BYTE_THINKGEAR_CODE(code) (code > 0x7F)

#define PAGE0_ENABLE_ATTENTION		0b00000001
#define PAGE0_ENABLE_MEDITATION		0b00000010
#define PAGE0_ENABLE_RAW_WAVE		0b00000100
#define PAGE0_ENABLE_HIGH_BAUD_RATE	0b00001000

int serial;
FILE * output;

void sigint_handler(int s)
{
	// Finish
	printf("\b\bOperation succeed.\n");
	close(serial);
	fclose(output);
	exit(0);
}

int main(int argc, char* argv[])
{
	 argv[1] = "COM3";
	 argv[2] = "output.csv";
//	if ((argc != 2) && (argc != 3))
//	{
//		printf("usage: %s <bluetooth_serial_port> [output_file]\n", argv[0]);
//		exit(4);
//	}

	signal (SIGINT,sigint_handler);

	// Open the port
	printf("Opening port.\n");
	serial = open(argv[1], O_RDWR);
	if (serial == -1)
	{
		printf("Cannot open the port:%s, errno = %i.\n", argv[1], errno);
		exit(1);
	}


    output = fopen(argv[2], "w+");


	if (!output)
	{
		printf("Cannot open the file:%s, errno = %i.\n", argv[2], errno);
		exit(2);
	}

	// Write csv header
	fprintf(output, "timestampMs,poorSignal,eegRawValue,eegRawValueVolts,attention,meditation,blinkStrength,delta,theta,alphaLow,alphaHigh,betaLow,betaHigh,gammaLow,gammaMid,tagEvent,location\n");

	// Send control byte
	printf("Sending control byte.\n");
	unsigned char command = 0x00;
	if (write(serial, &command, sizeof(command)) < sizeof(command))
	{
		printf("Cannot write the serial.\n");
		exit(3);
	}

	printf("Reading.\n");
	while (true)
	{
		// According to ThinkGear Serial Stream Guide the packet starts with the HEADER
		unsigned char HEADER = 0;

		// Read till the first SYNC
		if (read(serial, &HEADER, sizeof(HEADER)) < sizeof(HEADER))
		{
			printf("Cannot read the serial.\n");
			exit(3);
		}
		if (HEADER != THINKGEAR_SYNC) continue;

		//Check if the next byte is SYNC
		if (read(serial, &HEADER, sizeof(HEADER)) < sizeof(HEADER))
		{
			printf("Cannot read the serial.\n");
			exit(3);
		}
		if (HEADER != THINKGEAR_SYNC) continue;

		//Check if the next byte is PLENGTH
		while (HEADER == THINKGEAR_SYNC)
		{
			if (read(serial, &HEADER, sizeof(HEADER)) < sizeof(HEADER))
			{
				printf("Cannot read the serial.\n");
				exit(3);
			}
		}
		if (HEADER > THINKGEAR_SYNC) continue;

		// Read PAYLOAD
		unsigned char PAYLOAD[169];
		int checksum = 0;
		for (int i = 0; i < HEADER; i++)
		{
			if (read(serial, &PAYLOAD[i], sizeof(PAYLOAD[i])) < sizeof(PAYLOAD[i]))
			{
				printf("Cannot read the serial.\n");
				exit(3);
			}
			checksum += PAYLOAD[i];
		}
		checksum &= 0xFF;
		checksum = ~checksum & 0xFF;

		// Read CHKSUM
		unsigned char CHKSUM;
		if (read(serial, &CHKSUM, sizeof(CHKSUM)) < sizeof(CHKSUM))
		{
			printf("Cannot read the serial.\n");
			exit(3);
		}

		if (CHKSUM != checksum) continue;
		for (int i = 0; i < HEADER; i++)
		{
			if (PAYLOAD[i] == THINKGEAR_EXCODE)
			{
				printf("Unsupported code.\n");
				break;
			}
			else if (SINGLE_BYTE_THINKGEAR_CODE(PAYLOAD[i]))
			{
				switch (PAYLOAD[i++])
				{
					case 0x02:
						printf("POOR_SIGNAL: %i\n", PAYLOAD[i]);
						break;
					case 0x03:
						printf("HEART_RATE: %i\n", PAYLOAD[i]);
						break;
					case 0x04:
						printf("ATTENTION: %i\n", PAYLOAD[i]);
						break;
					case 0x05:
						printf("MEDITATION: %i\n", PAYLOAD[i]);
						break;
					case 0x06:
						printf("8BIT_RAW: %i\n", PAYLOAD[i]);
						break;
					case 0x07:
						printf("RAW_MARKER: %i\n", PAYLOAD[i]);
						break;
					default:
						printf("Unknown Code: %i\n", PAYLOAD[i]);
						break;
				}
			}
			else if (MULTI_BYTE_THINKGEAR_CODE(PAYLOAD[i]))
			{
				switch (PAYLOAD[i])
				{
					case 0x80:
						//printf("RAW Wave Value\n");
						break;
					case 0x81:
						printf("EEG_POWER\n");
						break;
					case 0x83:
						printf("ASIC_EEG_POWER\n");
						int temp;

						// timestamp
						time_t timestamp = time(NULL);
						fprintf(output, "%llu,", (unsigned long long)timestamp);

						// poorSignal, 0 for now
						fprintf(output, "0,");

						// eegRawValue, 0 for now
						fprintf(output, "0,");

						// eegRawValueVolts, 0 for now
						fprintf(output, "0,");

						// attention, 0 for now
						fprintf(output, "0,");

						// meditation, 0 for now
						fprintf(output, "0,");

						// blinkStrength, 0 for now
						fprintf(output, "0,");

						// delta
						temp = (PAYLOAD[i + 2] << 16) + (PAYLOAD[i + 3] << 8) + PAYLOAD[i + 4];
						fprintf(output, "%i,", temp);

						// theta
						temp = (PAYLOAD[i + 5] << 16) + (PAYLOAD[i + 6] << 8) + PAYLOAD[i + 7];
						fprintf(output, "%i,", temp);

						// alphaLow
						temp = (PAYLOAD[i + 8] << 16) + (PAYLOAD[i + 9] << 8) + PAYLOAD[i + 10];
						fprintf(output, "%i,", temp);

						// alphaHigh
						temp = (PAYLOAD[i + 11] << 16) + (PAYLOAD[i + 12] << 8) + PAYLOAD[i + 13];
						fprintf(output, "%i,", temp);

						// betaLow
						temp = (PAYLOAD[i + 14] << 16) + (PAYLOAD[i + 15] << 8) + PAYLOAD[i + 16];
						fprintf(output, "%i,", temp);

						// betaHigh
						temp = (PAYLOAD[i + 17] << 16) + (PAYLOAD[i + 18] << 8) + PAYLOAD[i + 19];
						fprintf(output, "%i,", temp);

						// gammaLow
						temp = (PAYLOAD[i + 20] << 16) + (PAYLOAD[i + 21] << 8) + PAYLOAD[i + 22];
						fprintf(output, "%i,", temp);

						// gammaMid
						temp = (PAYLOAD[i + 23] << 16) + (PAYLOAD[i + 24] << 8) + PAYLOAD[i + 25];
						fprintf(output, "%i,", temp);

						// tagEvent, Tag0 for now
						fprintf(output, "Tag0,");

						// location, unknown for now
						fprintf(output, "unknown\n");
						break;
					case 0x86:
						printf("RRINTERVAL\n");
						break;
					default:
						printf("Unknown Code: %i\n", PAYLOAD[i]);
						break;
				}
				i += PAYLOAD[++i];
			}
		}
	}
	return 0;
}
