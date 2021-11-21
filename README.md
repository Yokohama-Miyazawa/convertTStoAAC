# convertTStoAAC  
mpeg2-tsファイル(.ts)をAAC(.aac)ファイルに変換するプログラム  

## 使い方 Usage  
### コンパイル compiling  
`gcc -o convertor convertor.c`  

### 変換 Conversion  
`sample.ts`というファイルを変換する場合  
To convert a file named `sample.ts`  
`./convertor sample.ts`  
  
出力ファイルは`sample.aac`となる  
In this case, the output file will be `sample.aac`.  
  
### オプション Options
デバッグコードを表示するには、オプション`--debug=1`を指定する
  To display debug codes, add the option `--debug=1`  
`./convertor sample.ts --debug=1`  
