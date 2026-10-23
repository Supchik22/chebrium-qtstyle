{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
  buildInputs = [ pkgs.qt6.qtbase pkgs.qt6.qtwayland ];
  QT_PLUGIN_PATH = "${pkgs.qt6.qtbase}/lib/qt-6/plugins:${pkgs.qt6.qtwayland}/lib/qt-6/plugins";
}
