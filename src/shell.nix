{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = [
    pkgs.clang
    pkgs.llvmPackages_latest.clang
    pkgs.llvmPackages_latest.clang-tools
    pkgs.llvmPackages_latest.libcxx
    pkgs.llvmPackages_latest.lld
    pkgs.python3
    pkgs.zlib
    pkgs.zziplib
    pkgs.minizip
    pkgs.pkg-config

    pkgs.glew
    pkgs.glfw
    pkgs.glm
    pkgs.cmake
    pkgs.bear
    pkgs.glsl_analyzer
    # Add anything else you need
  ];
}
