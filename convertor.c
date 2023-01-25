/*
  convertor.c
  Convertor from an mpeg2-ts file to an aac or mp3 file
  Copyright (C) 2021  Osamu Miyazawa

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define TS_PACKET_SIZE 188
#define PAYLOAD_SIZE 184
#define PID_ARRAY_LEN 4

bool debug_mode = false;

enum FileFormat {
  AAC,
  MP3
};

typedef struct {
  int number;
  int pids[PID_ARRAY_LEN];
} pid_array;


void parsePAT(unsigned char* pat, pid_array *pidsOfPMT){
  int startOfProgramNums = 8;
  int lengthOfPATValue = 4;
  int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
  if(debug_mode){ printf("Section Length: %d\n", sectionLength); }
  int program_number, program_map_PID;
  int indexOfPids = 0;
  for(int i = startOfProgramNums; i <= sectionLength; i += lengthOfPATValue){
    program_number = ((pat[i] & 0xFF) << 8) | (pat[i+1] & 0xFF);
    program_map_PID = ((pat[i+2] & 0x1F) << 8) | (pat[i+3] & 0xFF);
    if(debug_mode){ printf("Program Num: 0x%04X(%d) PMT PID: 0x%04X(%d)\n", program_number, program_number, program_map_PID, program_map_PID); }
    pidsOfPMT->pids[indexOfPids++] = program_map_PID;
  }
  pidsOfPMT->number = indexOfPids;
}


void parsePMT(unsigned char *pat, pid_array *pidsOfAudio,  enum FileFormat* format){
  int staticLengthOfPMT = 12;
  int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
  if(debug_mode){ printf("Section Length: %d\n", sectionLength); }
  int programInfoLength = ((pat[10] & 0x0F) << 8) | (pat[11] & 0xFF);
  if(debug_mode){ printf("Program Info Length: %d\n", programInfoLength); }

  int indexOfPids = pidsOfAudio->number;
  int cursor = staticLengthOfPMT + programInfoLength;
  while(cursor < sectionLength - 1){
    int streamType = pat[cursor] & 0xFF;
    int elementaryPID = ((pat[cursor+1] & 0x1F) << 8) | (pat[cursor+2] & 0xFF);
    if(debug_mode){ printf("Stream Type: 0x%02X(%d) Elementary PID: 0x%04X(%d)\n", streamType, streamType, elementaryPID, elementaryPID); }

    if(streamType == 0x04){
      if(debug_mode){ printf("MP3 PID発見\n"); }
      *format = MP3;
      pidsOfAudio->pids[indexOfPids++] = elementaryPID;
    }else if(streamType == 0x0F || streamType == 0x11){
      if(debug_mode){ printf("AAC PID発見\n"); }
      *format = AAC;
      pidsOfAudio->pids[indexOfPids++] = elementaryPID;
    }

    int esInfoLength = ((pat[cursor+3] & 0x0F) << 8) | (pat[cursor+4] & 0xFF);
    if(debug_mode){ printf("ES Info Length: 0x%04X(%d)\n", esInfoLength, esInfoLength); }
    cursor += 5 + esInfoLength;
  }
  pidsOfAudio->number = indexOfPids;
}


void parsePES(unsigned char *pat, int posOfPacketStart, FILE *wfp){
  int firstByte  = pat[0] & 0xFF;
  int secondByte = pat[1] & 0xFF;
  int thirdByte  = pat[2] & 0xFF;
  if(debug_mode){ printf("First 3 bytes: %02X %02X %02X\n", firstByte, secondByte, thirdByte); }
  if(firstByte == 0x00 && secondByte == 0x00 && thirdByte == 0x01){
    int PESRemainingPacketLength = ((pat[4] & 0xFF) << 8) | (pat[5] & 0xFF);
    if(debug_mode){ printf("PES Packet length: %d\n", PESRemainingPacketLength); }
    int posOfHeaderLength = 8;
    int PESRemainingHeaderLength = pat[posOfHeaderLength] & 0xFF;
    if(debug_mode){ printf("PES Header length: %d\n", PESRemainingHeaderLength); }
    int startOfData = posOfHeaderLength + PESRemainingHeaderLength + 1;
    if(debug_mode){ printf("First AAC data byte: %02X\n", pat[startOfData]); }
    fwrite(&pat[startOfData], 1, (TS_PACKET_SIZE - posOfPacketStart) - startOfData, wfp);
  }else{
    if(debug_mode){ printf("First AAC data byte: %02X\n", pat[0]); }
    fwrite(pat, 1, TS_PACKET_SIZE - posOfPacketStart, wfp);
  }
}


void parsePacket(unsigned char* packet, pid_array *pidsOfPMT, pid_array *pidsOfAudio, FILE* wfp, enum FileFormat* format){
  int pid = ((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF);
  if(debug_mode){ printf("PID: 0x%04X(%d)\n", pid, pid); }
  int payloadUnitStartIndicator = (packet[1] & 0x40) >> 6;
  if(debug_mode){ printf("Payload Unit Start Indicator: %d\n", payloadUnitStartIndicator); }
  int adaptionFieldControl = (packet[3] & 0x30) >> 4;
  if(debug_mode){ printf("Adaption Field Control: %d\n", adaptionFieldControl); }
  int remainingAdaptationFieldLength = -1;
  if((adaptionFieldControl & 0b10) == 0b10) {
    remainingAdaptationFieldLength = packet[4] & 0xFF;
    if(debug_mode){ printf("Adaptation Field Length: %d\n", remainingAdaptationFieldLength); }
  }

  int payloadStart = payloadUnitStartIndicator ? 5 : 4;

  if(pid == 0){
    if(debug_mode){ printf("PATをパース\n"); }
    parsePAT(&packet[payloadStart], pidsOfPMT);
  } else if(pidsOfAudio->number){
    for(int i = 0; i < pidsOfAudio->number; i++){
      if(pid == pidsOfAudio->pids[i]){
        if(debug_mode){
          if(*format == MP3){
            printf("MP3発見\n");
          }else if(*format == AAC){
            printf("AAC発見\n");
          }else{
            printf("フォーマット設定異常\n");
          }
        }
        int posOfPacketSrart = 4;
        if(remainingAdaptationFieldLength >= 0){
          posOfPacketSrart = 5+remainingAdaptationFieldLength;
          parsePES(&packet[posOfPacketSrart], posOfPacketSrart, wfp);
        }else{
          parsePES(&packet[posOfPacketSrart], posOfPacketSrart, wfp);
        }
      }
    }
  } else if(pidsOfPMT->number){
    for(int i = 0; i < pidsOfPMT->number; i++){
      if(pid == pidsOfPMT->pids[i]){ 
        if(debug_mode){ printf("PMT発見\n"); }
        parsePMT(&packet[payloadStart], pidsOfAudio, format);
      }
    }
  }   
  return;
}


int main(int argc, char *argv[]){
  if(argc <= 1){
    fputs("ファイル名が指定されていません。\n", stderr);
    return 1;
  }

  char *inputFileName, *temporalOutputFileName;
  int inputFileNameLength;
  for(int i = 1; i < argc; i++){
    if(!debug_mode && strncmp(argv[i], "--debug=", 8) == 0){
      if(strncmp(&argv[i][8], "1", 1) == 0){
        printf("Debug Mode\n");
        debug_mode = true;
      }
    } else {
      inputFileName = argv[i];
      inputFileNameLength = (int)strlen(inputFileName);
      temporalOutputFileName = (char *)malloc(inputFileNameLength + 2);
      strncpy(temporalOutputFileName, inputFileName, inputFileNameLength - 2);
      snprintf(temporalOutputFileName, inputFileNameLength + 2, "%s%s", temporalOutputFileName, "tmp");
    }
  }

  if(debug_mode){
    printf("Input File Name %s\n", inputFileName);
    printf("Temporal Output File Name: %s\n", temporalOutputFileName);
  }

  FILE *fp = fopen(inputFileName, "rb");
  FILE *wfp = fopen(temporalOutputFileName, "wb");
  if(fp == NULL || wfp == NULL){
    fputs("ファイルオープンに失敗しました。\n", stderr);
    exit(EXIT_FAILURE);
  }

  pid_array pidsOfPMT, pidsOfAudio;
  pidsOfPMT.number = 0;
  pidsOfAudio.number = 0;

  int packetCount = 0;

  enum FileFormat outputFileFormat;

  while(1){
    int c = fgetc(fp);
    unsigned char packet[TS_PACKET_SIZE];

    if(feof(fp)){ 
      break; 
    }else if(ferror(fp)){
      fputs("読み込み中にエラーが発生しました。\n", stderr);
      exit(EXIT_FAILURE);
    }else if(c == 0x47){
      if(debug_mode){ printf("TSパケット発見\n"); }
      packetCount++;
      fseek(fp, -1, SEEK_CUR);
      if(fread(packet, 1, TS_PACKET_SIZE, fp) < 1){
        fputs("パケット読み込み中にエラーが発生しました。\n", stderr);
        exit(EXIT_FAILURE);
      } else {
        parsePacket(packet, &pidsOfPMT, &pidsOfAudio, wfp, &outputFileFormat);
      }
    }else{
      // 何もしない
    }
  }

  if(fclose(fp) == EOF || fclose(wfp) == EOF){
    fputs("ファイルクローズに失敗しました。\n", stderr);
    exit(EXIT_FAILURE);
  }

  if(debug_mode){ printf("%d packets are found.\n", packetCount); }

  char *outputFileName, *finalOutputFileFormat;
  finalOutputFileFormat = (outputFileFormat == MP3) ? "mp3" : "aac";
  outputFileName = (char *)malloc(inputFileNameLength + 2);
  strncpy(outputFileName, inputFileName, inputFileNameLength - 2);
  snprintf(outputFileName, inputFileNameLength + 2, "%s%s", outputFileName, finalOutputFileFormat);
  if(debug_mode){
    printf("Output File Name: %s\n", outputFileName);
    printf("%s -> %s\n", temporalOutputFileName, outputFileName);
  }

  if (access(outputFileName, F_OK) != 0) {
    if (rename(temporalOutputFileName, outputFileName) != 0) {
      fputs("ファイルリネームに失敗しました。\n", stderr);
      exit(EXIT_FAILURE);
    }
  } else {
    fputs("同名の出力ファイルが既に存在しています。\n", stderr);
    exit(EXIT_FAILURE);
  }
  free(temporalOutputFileName);
  free(outputFileName);
  if(debug_mode){
    printf("Conversion done.\n");
  }

  return 0;
}
