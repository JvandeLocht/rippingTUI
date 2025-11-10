{
  description = "Blu-ray Ripper - TUI for MakeMKV and HandBrake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "bluray-ripper";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            # FTXUI library
            ftxui
            # Runtime dependencies
            makemkv
            handbrake
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          meta = with pkgs.lib; {
            description = "TUI for ripping Blu-rays with MakeMKV and HandBrake";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            gcc
            pkg-config
            clang-tools  # For clangd LSP
            gdb

            # FTXUI library
            ftxui

            # Runtime tools
            makemkv
            handbrake
          ];

          shellHook = ''
            echo "Blu-ray Ripper development environment"
            echo "Build with: cmake -B build && cmake --build build"
            echo "Run with: ./build/bluray-ripper"
          '';
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/bluray-ripper";
        };
      }
    );
}
