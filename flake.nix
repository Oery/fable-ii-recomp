{
  description = "Fable II recompilation and supplied GoD extraction tools";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = (pkgs.mkShell.override {
        stdenv = pkgs.llvmPackages_20.stdenv;
      }) {
        packages = with pkgs; [
          cmake ninja pkg-config git python3 file ripgrep
          llvmPackages_20.lld llvmPackages_20.llvm gdb
          autoconf automake libtool nasm
          lz4 zstd openssl curl wxwidgets_3_2
          gtk3 libX11 libXext libXrandr libXcursor libXi libXScrnSaver libxcb libXtst
          wayland wayland-protocols libxkbcommon libdecor
          alsa-lib libpulseaudio pipewire vulkan-loader vulkan-headers vulkan-tools mesa
        ];
        shellHook = ''
          # SDL loads these backends dynamically, so their runtime libraries
          # must remain discoverable even though SDL itself links statically.
          # This includes the X11/Wayland helper libraries SDL's video
          # drivers dlopen at startup (Xrandr/Xfixes/Xi/Xcursor/Xss);
          # without them SDL reports "No available video device".
          export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
            pkgs.alsa-lib
            pkgs.libpulseaudio
            pkgs.pipewire
            pkgs.vulkan-loader
            pkgs.mesa
            pkgs.libX11
            pkgs.libXext
            pkgs.libXrandr
            pkgs.libXcursor
            pkgs.libXi
            pkgs.libXfixes
            pkgs.libXScrnSaver
            pkgs.libxcb
            pkgs.libXtst
            pkgs.libxkbcommon
            pkgs.wayland
            pkgs.libdecor
          ]}:$LD_LIBRARY_PATH
          # Use Mesa's software Vulkan device for reproducible headless runs.
          export VK_ICD_FILENAMES=${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.x86_64.json
        '';
      };
    };
}
