{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
  };

  outputs = { self, nixpkgs }: {

    apps.x86_64-linux.default = {
      type = "app";
      program = "${self.packages.x86_64-linux.ud3tn}/bin/ud3tn";
    };

    packages.x86_64-linux =
      let
        pkgs = import nixpkgs { system = "x86_64-linux"; };
      in
      {
        ud3tn = pkgs.stdenv.mkDerivation {
          pname = "ud3tn";
          version = "0.13.0";

          src = pkgs.lib.sourceByRegex ./. [
            "Makefile"
            "^components.*"
            "^external.*"
            "^generated.*"
            "^include.*"
            "^mk.*"
          ];

          buildPhase = ''
            make type=release optimize=yes -j $NIX_BUILD_CORES ud3tn
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp build/posix/ud3tn $out/bin/
          '';
        };

        pyd3tn = with pkgs.python3Packages; buildPythonPackage {
          name = "pyd3tn";
          src = ./pyd3tn;
          propagatedBuildInputs = [ cbor ];
        };

        python-ud3tn-utils = with pkgs.python3Packages; buildPythonPackage {
          name = "ud3tn-utils";
          src = ./python-ud3tn-utils;
          propagatedBuildInputs = [ protobuf setuptools ];
        };
      };

    devShells.x86_64-linux.default =
      let
        pkgs = import nixpkgs { system = "x86_64-linux"; };
      in
      pkgs.mkShell {

        hardeningDisable = [ "all" ];

        packages = with self.packages.x86_64-linux; with pkgs; [
          # self
          pyd3tn
          python-ud3tn-utils
          ud3tn
          # nixpkgs
          clang-tools
          gdb
          llvmPackages.libcxxClang
          nixpkgs-fmt
          protobuf
          python3Packages.flake8
          python3Packages.pip
          python3Packages.protobuf
          python3Packages.pytest
          python3Packages.setuptools
        ];
      };

    formatter.x86_64-linux = nixpkgs.legacyPackages.x86_64-linux.nixpkgs-fmt;
  };
}
