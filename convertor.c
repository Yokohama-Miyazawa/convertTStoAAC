#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define TS_PACKET_SIZE 188
#define PAYLOAD_SIZE 184
#define PID_ARRAY_LEN 4

bool debug_mode = false;

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


void parsePMT(unsigned char *pat, pid_array *pidsOfAAC){
  int staticLengthOfPMT = 12;
  int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
  if(debug_mode){ printf("Section Length: %d\n", sectionLength); }
  int programInfoLength = ((pat[10] & 0x0F) << 8) | (pat[11] & 0xFF);
  if(debug_mode){ printf("Program Info Length: %d\n", programInfoLength); }

  int indexOfPids = pidsOfAAC->number;
  int cursor = staticLengthOfPMT + programInfoLength;
  while(cursor < sectionLength - 1){
    int streamType = pat[cursor] & 0xFF;
    int elementaryPID = ((pat[cursor+1] & 0x1F) << 8) | (pat[cursor+2] & 0xFF);
    if(debug_mode){ printf("Stream Type: 0x%02X(%d) Elementary PID: 0x%04X(%d)\n", streamType, streamType, elementaryPID, elementaryPID); }

    if(streamType == 0x0F || streamType == 0x11){
      if(debug_mode){ printf("AAC PID発見\n"); }
      pidsOfAAC->pids[indexOfPids++] = elementaryPID;
    }

    int esInfoLength = ((pat[cursor+3] & 0x0F) << 8) | (pat[cursor+4] & 0xFF);
    if(debug_mode){ printf("ES Info Length: 0x%04X(%d)\n", esInfoLength, esInfoLength); }
    cursor += 5 + esInfoLength;
  }
  pidsOfAAC->number = indexOfPids;
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


void parsePacket(unsigned char* packet, pid_array *pidsOfPMT, pid_array *pidsOfAAC, FILE* wfp){
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
  } else if(pidsOfAAC->number){
    for(int i = 0; i < pidsOfAAC->number; i++){
      if(pid == pidsOfAAC->pids[i]){
        if(debug_mode){ printf("AAC発見\n"); }
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
        parsePMT(&packet[payloadStart], pidsOfAAC);
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

  char *inputFileName, *outputFileName;
  for(int i = 1; i < argc; i++){
    if(strncmp(argv[i], "--debug=", 8) == 0){
      if(strncmp(&argv[i][8], "1", 1) == 0){
        printf("DEBUG MODE\n");
        debug_mode = true;
      }
    } else {
      const char *extension = strrchr(argv[i], '.');
      if(strcmp(".ts", extension) != 0){
        fputs("拡張子が違います。.tsファイルを指定してください。\n", stderr);
        return 1;
      }
      if(debug_mode){ printf("Input File Name: %s\n", argv[i]); }
      inputFileName = argv[i];
      int inputFileNameLength = (int)strlen(inputFileName);
      if(debug_mode){ printf("input file name length: %d\n", inputFileNameLength); }
      outputFileName = (char*)malloc(inputFileNameLength + 2);
      strncpy(outputFileName, inputFileName, inputFileNameLength-2);
      snprintf(outputFileName, inputFileNameLength + 2, "%s%s", outputFileName, "aac");
      if(debug_mode){ printf("Output File Name: %s\n", outputFileName); }
    }
  }

  FILE *fp = fopen(inputFileName, "rb");
  FILE *wfp = fopen(outputFileName, "wb");
  free(&outputFileName);
  if(fp == NULL || wfp == NULL){
    fputs("ファイルオープンに失敗しました。\n", stderr);
    exit(EXIT_FAILURE);
  }

  pid_array pidsOfPMT, pidsOfAAC;
  pidsOfPMT.number = 0;
  pidsOfAAC.number = 0;

  int packetCount = 0;

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
        parsePacket(packet, &pidsOfPMT, &pidsOfAAC, wfp);
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

  return 0;
}
