# 低消費電力円形トラックパッド


IQS7211Eを使用した直径30mmのトラックパッドです。低消費電力(~1.5mA)なので無線自作キーボードでの使用に適しています。2点マルチタッチに対応しています。

|![](img/top.JPG)|![](img/bottom.JPG)|
|-|-|

**[BOOTH](https://nogikes.booth.pm/items/7254791)にて販売中**

## 組立

* 両面テープは基板表面に隙間なく敷き詰めてください。
* アクリルプレートの片面の保護シートをはがし、基板に貼り付けてください。
* もう一面の保護シートは剥がさないほうが操作感が良いです。

## ピン配置

0.5mmピッチ、6ピンのFPCで接続します。 同一電極面のFPCを使う場合、対向する側のコネクタでピン順が反転するので注意してください。

Auto-KDKやtorabo-tsuki LPのコネクタと同一電極面のFPCで接続できます。

|番号|機能|
|-|-|
|1|VCC(1.8~3.3V)|
|2|RDY|
|3|GND|
|4|SCL|
|5|SDA|
|6|GND|

## サンプルプログラム

- [zmk-driver-iqs7211e](https://github.com/sekigon-gonnoc/zmk-driver-iqs7211e)
  - [torabo-tsuki LPに取り付けるサンプル](https://github.com/sekigon-gonnoc/zmk-keyboard-torabo-tsuki-lp/tree/torapa-tsuki)
    - [3dmodels](./3dmodels/)の中のモデルを3Dプリンタで印刷して、本モジュールをtorabo-tsuki LPのボトムプレートに取り付けられます。
    - torapa-tsukiブランチをビルドして書き込んでください。
- [QMK用サンプル](./qmk_firmware)
    - ピン設定はAuto-KDKコントローラに合わせてあります。必要に応じてconfig.hを編集してください。

## ライセンス

* `qmk_firmware/`以下はGPL-3.0、それ以外はMITライセンスです。

```
MIT License

Copyright (c) 2025 sekigon-gonnoc

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```