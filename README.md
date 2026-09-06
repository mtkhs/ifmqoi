# ifmqoi.sph - QOI Susie Plug-in

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-blue.svg)](https://www.microsoft.com/windows)

- QOI (Quite OK Image Format) 形式の画像ファイルを Susie 対応画像ビューアで表示するための 64bit プラグインです
- QOI デコーダを内包しており、外部ライブラリには依存しません
- あふｗで動作確認しています

## 機能

- QOI 形式 (*.qoi) の画像ファイルの読み込み

## ビルド方法

### 必要な環境

- Visual Studio 2022 以降
- CMake 3.15 以降
- Windows 10/11 (64bit)

### ビルド手順

1. リポジトリを取得
```bash
git clone <このリポジトリのURL>
cd ifmqoi
```

2. ビルドディレクトリを作成
```bash
mkdir build
cd build
```

3. CMake を実行
```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

4. コンパイル
```bash
cmake --build . --config Release
```

ビルドされたプラグインは `build/Release` に出力されます

## 📚 参考資料

### 開発に使用したソフトウェア・参考にした情報
- **[Susie 32bit / 64bit Plug-in の仕様(2025-8-10版) - TORO's Library](http://toro.d.dooo.jp/dlsphapi.html)**: susie.h を拝借
- **[runspx](https://github.com/toroidj/runspx)**: APIの動作確認用
- **[QOI](https://github.com/phoboslab/qoi)**
