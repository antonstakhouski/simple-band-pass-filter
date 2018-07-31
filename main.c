#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

/*TODO:*/
/*4. codestyle fixes(global vars, qualifiers)*/

/* 5ms interval */
#define BLOCK_SIZE 8 * 2 * 5
#define SAMPLE_RATE 8000

#define NZEROS 12
#define NPOLES 12
#define GAIN   3.868375833e+02

static float xv[NZEROS+1], yv[NPOLES+1];

struct wave_file_header {
	char ckID[4];
	uint32_t cksize;
	char WAVEID[4];
};

struct fmt_header {
	char ckID[4];
	uint32_t cksize;
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
};

struct data_chunk_header {
	char ckID[4];
	uint32_t cksize;
};

/**
 * Butterworth 6th order band filter with center in 1000 Hz
 * (cuttof frequencies are 400 Hz and 1600 Hz)
 */
static void filterloop(const int16_t *src, int16_t *dest, int size)
{
	for (int i = 0; i < size; i++) {
		xv[0] = xv[1];
		xv[1] = xv[2];
		xv[2] = xv[3];
		xv[3] = xv[4];
		xv[4] = xv[5];
		xv[5] = xv[6];
		xv[6] = xv[7];
		xv[7] = xv[8];
		xv[8] = xv[9];
		xv[9] = xv[10];
		xv[10] = xv[11];
		xv[11] = xv[12];
		xv[12] = ((double)src[i])/ GAIN;
		yv[0] = yv[1];
		yv[1] = yv[2];
		yv[2] = yv[3];
		yv[3] = yv[4];
		yv[4] = yv[5];
		yv[5] = yv[6];
		yv[6] = yv[7];
		yv[7] = yv[8];
		yv[8] = yv[9];
		yv[9] = yv[10];
		yv[10] = yv[11];
		yv[11] = yv[12];
		yv[12] = (xv[0] + xv[12]) - 6 * (xv[2] + xv[10])
			+ 15 * (xv[4] + xv[8]) - 20 * xv[6]
			+ ( -0.0218315740 * yv[0]) + (  0.2705039756 * yv[1])
			+ ( -1.6299093485 * yv[2]) + (  6.2757857499 * yv[3])
			+ (-17.1578265840 * yv[4]) + ( 35.0570401030 * yv[5])
			+ (-54.8756557230 * yv[6]) + ( 66.3406171160 * yv[7])
			+ (-61.5547217780 * yv[8]) + ( 42.7987607680 * yv[9])
			+ (-21.1536778800 * yv[10]) + ( 6.6501842739 * yv[11]);
		dest[i] = (int16_t)yv[12];
	}
}

void show_data_chunk_info(const struct data_chunk_header *data)
{
	printf("Chunk ID: %s\n", data->ckID);
	printf("Chunk size: %" PRIu32 "\n", data->cksize);
};

void show_wave_info(const struct wave_file_header *wave_header)
{
	printf("Chunk ID: %s\n", wave_header->ckID);
	printf("Chunk size: %" PRIu32 "\n", wave_header->cksize);
	printf("WAVE ID: %s\n", wave_header->WAVEID);
}

void show_fmt_info(const struct fmt_header *fmt)
{
	printf("Chunk ID: %s\n", fmt->ckID);
	printf("Chunk size: %" PRIu32 "\n", fmt->cksize);
	printf("Format code: %" PRIu16 "\n", fmt->wFormatTag);
	printf("Number of interleaved channels: %" PRIu16 "\n", fmt->nChannels);
	printf("Sampling rate (blocks per second): %" PRIu32 "\n", fmt->nSamplesPerSec);
	printf("Data rate: %" PRIu32 "\n", fmt->nAvgBytesPerSec);
	printf("Data block size (bytes): %" PRIu16 "\n", fmt->nBlockAlign);
	printf("Bits per sample: %" PRIu16 "\n", fmt->wBitsPerSample);
}

int read_wav(char *in_filename, char *out_filename)
{
	FILE *fin, *fout;
	fin = fopen(in_filename, "rb");
	if (!fin) {
		perror("Error opening input file");
		goto fin_open_error;
	}

	fout = fopen(out_filename, "wb");
	if (!fout) {
		perror("Error opening output file");
		goto fout_open_error;
	}

	struct wave_file_header wave_header;
	fread(&wave_header, sizeof(wave_header), 1, fin);
	show_wave_info(&wave_header);

	struct fmt_header fmt;
	fread(&fmt, sizeof(fmt), 1, fin);
	show_fmt_info(&fmt);

	struct data_chunk_header data;
	fread(&data, sizeof(data), 1, fin);
	show_data_chunk_info(&data);

	fwrite(&wave_header, sizeof(wave_header), 1, fout);
	fwrite(&fmt, sizeof(fmt), 1, fout);
	fwrite(&data, sizeof(data), 1, fout);

	int16_t *block;
	int16_t *filered_block;
	block = (int16_t*)malloc(BLOCK_SIZE);
	if (!block) {
		perror("error allocating memory for block pointer");
		goto block_error;
	}
	filered_block = (int16_t*)malloc(BLOCK_SIZE);
	if (!filered_block) {
		perror("error allocating memory for filered_block pointer");
		goto filtered_block_error;
	}


	uint32_t n;
	n = 0;
	for(uint32_t i = 0; i < data.cksize; i+= n) {
		n = fread(block, 1, BLOCK_SIZE, fin);
		filterloop(block, filered_block,
				(n / sizeof(block[0])));
		fwrite(filered_block, 1, n, fout);
	}

	while (!feof(fin)) {
		n = fread(&block, 1, sizeof(block), fin);
		fwrite(&block, 1, n, fout);
	}

	fclose(fin);
	fclose(fout);
	free(filered_block);
	free(block);
	return 0;

filtered_block_error:
	free(block);

block_error:
	fclose(fout);

fout_open_error:
	fclose(fin);

fin_open_error:
	return -errno;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s <in_file> <out_file>",  argv[0]);
		exit(0);
	}

	read_wav(argv[1], argv[2]);
	return 0;
}
