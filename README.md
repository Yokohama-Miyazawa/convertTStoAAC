# convertTStoAAC  
mpeg2-tsファイル(.ts)をAAC(.aac)ファイルに変換するプログラム  

## 使い方 Usage  
コンパイル compiling  
`gcc -o converter converter.c`  

変換 conversion  
`sample.ts`というファイルを変換する場合 In a case of conversion a file named `sample.ts`.  
`./converter sample.ts`  
出力ファイルは`sample.aac`となる。 The output file will be `sample.aac`.  

デバッグコードを表示するには、オプション`--debug=1`を指定する To display debug codes, add the option `--debug=1`  
`./converter sample.ts --debug=1`  
