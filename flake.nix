{
  description = "Pastec - Image Recognition Engine";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let 
        pkgs = import nixpkgs { inherit system; };
      in {
        # Development environment
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Build tools
            cmake
            clang
            lld
            
            # Libraries
            opencv
            libmicrohttpd
            curl
            jsoncpp
            mimalloc
          ];
          
          # Environment variables for memory optimization
          shellHook = ''
            export MIMALLOC_LARGE_OS_PAGES=1
            export MALLOC_CONF="thp:always,metadata_thp:always"
            export GLIBC_TUNABLES=glibc.malloc.hugetlb=1
          '';
        };
        
        # Package definition
        packages.pastec = pkgs.stdenv.mkDerivation {
          pname = "pastec";
          version = "1.0.0";
          
          src = ./.;
          nativeBuildInputs = with pkgs; [
            cmake
            clang
            lld
          ];
          
          buildInputs = with pkgs; [
            opencv
            libmicrohttpd
            curl
            jsoncpp
            mimalloc
          ];
          
          cmakeFlags = [
            "-DCMAKE_C_COMPILER=clang"
            "-DCMAKE_CXX_COMPILER=clang++"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_PASTEC_LIB=ON"
            "-DBUILD_PASTEC_EXE=ON"
            "-DBUILD_EXAMPLES=OFF"
          ];
          
          installPhase = ''
            mkdir -p $out/bin
            cp pastec_server $out/bin/pastec
            mkdir -p $out/lib
            cp lib/libpastec.a $out/lib/
            mkdir -p $out/include
            cp -r ../include/pastec $out/include/
            mkdir -p $out/data
            cp ../visualWordsORB.dat $out/data/
          '';
        };
        
        # Default package
        defaultPackage = self.packages.${system}.pastec;
      }
    );
}
