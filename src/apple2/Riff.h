#pragma once

int RiffInitWriteFile(char *pszFile, uint32_t sample_rate, uint32_t NumChannels);

int RiffFinishWriteFile();

int RiffPutSamples(short *buf, uint32_t uSamples);
