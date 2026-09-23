# ifmqoi.sph - QOI Susie Plug-in

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-blue.svg)](https://www.microsoft.com/windows)

- QOI (Quite OK Image Format) 形式の画像ファイルを Susie 対応画像ビューアで表示するための 64bit プラグインです
- QOI デコーダを内包しており、外部ライブラリには依存しません
- あふｗで動作確認しています

## 機能

- QOI 形式 (*.qoi) の画像ファイルの読み込み
- アルファは背景色に合成して出力します（既定 E0E0E0）

## 設定ファイル（任意）

プラグインと同じフォルダに `ifmqoi.ini` があれば読み込みます

```ini
[render]
; アルファ合成の背景色 (RRGGBB)。既定 E0E0E0
background_color=FFFFFF
```

## 📚 参考資料

### 開発に使用したソフトウェア・参考にした情報
- **[Susie 32bit / 64bit Plug-in の仕様(2025-8-10版) - TORO's Library](http://toro.d.dooo.jp/dlsphapi.html)**: susie.h を拝借
- **[runspx](https://github.com/toroidj/runspx)**: APIの動作確認用
- **[QOI](https://github.com/phoboslab/qoi)**: デコーダの移植元。表示は `NOTICE.md`
