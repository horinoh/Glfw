# Glfw

## インストール
- [Glfw](https://www.glfw.org/download)
- ここでは、インストール先環境変数を GLFW_SDK_PATH として作成した

## 他
### Meson を使用する場合
#### インストール
- Python をインストールしていない場合は予めインストールしておく

- [GitHub](https://github.com/mesonbuild/meson/releases) から .msi インストーラをダウンロード
- 実行してインストール
	- exe の場所 (C:\Program Files\Meson) は、インストール時に環境変数 Path に追加される模様

- 以下のようにしてバージョンが表示されれば OK
	~~~
	$meson.exe --version
	$ninja.exe --version
	~~~

#### ビルド
- Main.cpp があるフォルダまで行く
- 初回
    ~~~
    $meson init
    ~~~
    - meson.build の雛形が作成される

- ビルド環境の生成 (ここでは build フォルダとする) 
    ~~~
    $meson setup build
    // 再生成する場合
    $meson setup build --reconfigure
    // 既存のオプションを指定する場合
    $mesion setup build --prefix INSTALL_DIR --buildtype release
    // Visual Studio 用にする場合
    $meson setup build --backend vs
    ~~~
    - 自前のオプションを指定する場合
        - 予め meson_options.txt を作成し、自前変数を列挙しておく
        - -DXXX=YYY のように引数で指定、-DXXX=%ZZZ% のようにすれば環境変数 ZZZ を渡せる
        ~~~
        $meson setup build -DXXX=%ZZZ%
        ~~~
    - ここでは以下のように生成した (環境変数 VK_SDK_PATH, GLFW_SDK_PATH は設定してある前提)
        ~~~
        $meson setup build --reconfigure -DVK=%VK_SDK_PATH% -DGLFW=%GLFW_SDK_PATH%
        ~~~
- ビルド
    ~~~
    $pushd build
    $meson compile
    $popd
    ~~~


