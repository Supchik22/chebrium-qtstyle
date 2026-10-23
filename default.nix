# nix-build qtstyle  ->  result/lib/qt-6/plugins/styles/libchebrium.so
{ pkgs ? import <nixpkgs> {} }:
pkgs.stdenv.mkDerivation {
  pname = "chebrium-qtstyle";
  version = "0.1";
  src = ./.;
  nativeBuildInputs = with pkgs; [ cmake ninja ];
  buildInputs = [ pkgs.qt6.qtbase ];
  dontWrapQtApps = true;
}
