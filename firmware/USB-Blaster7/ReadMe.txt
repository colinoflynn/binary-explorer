USB-Blasterもどき
マイクロチップのUSBファームウェアVer.2.8をベースに作っています。
再コンパイルする場合下記の環境が必要です。
・MPLAB IDE
・C18 Compiler
・Microchip USB Firmware Ver.2.8(同梱していますので用意する必要はありません)
上記ツールをインストールした上でプロジェクトファイルを開き、
Configure->SelectDevice画面でPIC18F14k50を選択してください。

SPIモジュールの利用と、バイト単位でのIOのため、IOピンが固定となっています。
「BlasterMacros.h」でのピン定義はあくまで命名のためです。
ここだけ変更してもピン配置の変更にはなりません。

・SPI周波数変更
　「BlasterMacros.h」ファイルの13-14行目で切り替え可能です。

・PIO/SPI切り替え
　JTAG通信を高速化するため、PICのMSSPモジュールをSPIモードで利用しています。
　「BlasterMacros.h」ファイル23行目の#define USE_SPIをコメントアウトすると
　SPIモジュールを使わず、プログラムでIOを行います。
　速度は遅くなるので、できればSPIを利用することをお勧めします。
