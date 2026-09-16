{
  description = "YOLOv11 barcode detector - OpenCV + ONNX Runtime C++ deployment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      gtk_lib = with pkgs; [
        libGL
        glib
        libSM
        libICE
      ];
      opencvGtk = pkgs.opencv.override { enableGtk3 = true; };

    in
    {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          cmake
          pkg-config
          gcc
        ];

        buildInputs = with pkgs; [
          curl
          nlohmann_json
          opencvGtk
          onnxruntime
          openssl
          zbar
          pkgs.gtk3
        ];

        CMAKE_PREFIX_PATH = "${opencvGtk}:${pkgs.onnxruntime.dev}:${pkgs.zbar.dev}:${pkgs.openssl.dev}";
        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath gtk_lib;
        shellHook = ''
          echo "YOLOv11 Barcode Detector Dev Shell"
          echo "OpenCV:  $(pkg-config --modversion opencv4)"
          echo "ONNX RT: ${pkgs.onnxruntime.version}"
        '';
      };
    };
}
