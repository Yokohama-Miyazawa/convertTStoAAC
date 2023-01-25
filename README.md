# convertTStoAAC  
mpeg2-tsファイル(.ts)をAAC(.aac)もしくはMP3(.mp3)ファイルに変換するプログラム  
Convertor from an mpeg2-ts file to an aac or mp3 file  

## 使い方 Usage  
### コンパイル Compiling  
`gcc -o convertor convertor.c`  

### 変換 Conversion  
`sample.ts`というファイルを変換する場合  
To convert a file named `sample.ts`  
`./convertor sample.ts`  
  
出力ファイルはフォーマットに応じて`sample.aac`もしくは`sample.mp3`となる  
In this case, the output file will be `sample.aac` or `sample.mp3` depending on the coding format.  
  
### オプション Options
デバッグコードを表示するには、オプション`--debug=1`を指定する  
To display debug codes, add the option `--debug=1`  
`./convertor sample.ts --debug=1`  
